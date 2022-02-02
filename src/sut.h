#ifndef __SUT_H__
#define __SUT_H__
#include <stdbool.h>
#include <ucontext.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>

//macros
#define MAX_THREADS             15
#define THREAD_STACK_SIZE       1012*64
#define READ_BUF_SIZE           128

//thread statuses
#define ACTIVE                  0       //requeue
#define INACTIVE                1       //schedule for deletion

//thread queue states
#define TOQUEUE                 0       //a process has been created or yielded and should be queued
#define QUEUED                  1       //a process is in the queue and should NOT be requed
#define NOQUEUE                 2       //a process has been scheduled for destruct and should NOT be requeued  
//#define RUNNING               3       //a process is running and should NOT be requed

//for wait status
#define READY                   0
#define WAIT                    1

//io request values
#define NOTASK                  0       //there is no task yet associated with this request
#define OPEN                    1                    
#define READ                    2
#define WRITE                   3
#define CLOSE                   4


//GLOBAL VARIABLES

typedef struct __tcb{
    int threadid;
    char *threadstack;
    void *threadfunc;
    ucontext_t threadcontext;
    int waitstatus;
    int schedule;
    int * socketfd;
} tcb;

typedef struct __ioreq{
    int request_thread;
    char host[24];
    uint16_t port;
    int sockfd;
    int task;
    char read_buffer[READ_BUF_SIZE];
    char * write_buffer;
    int size_write;
} ioreq;

char requester_buf[READ_BUF_SIZE];

ioreq * ioreq_ptr;

tcb * threadarr[MAX_THREADS];        //try making it an array of pointers TO the stucts
int numthreads;                     //number of live threads, attribute of tcb (protected by lock_tcb)
tcb * curthread_ptr;
int curthread;
static ucontext_t c_context;
static ucontext_t temp;
bool shutdown_k;

//queue pointers
struct queue *ready_qptr;
struct queue *wait_qptr;

//thread handles
pthread_t c_exec_handle;
pthread_t i_exec_handle;

//locks
pthread_mutex_t lock_rq;
pthread_mutex_t lock_tcb;
pthread_mutex_t lock_ioreq;

//semaphors
sem_t sema_task_start;

//TYPEDEF
typedef void (*sut_task_f)();

//FUNCTION DECLARATIONS

//a function for creating a client end socket
//called by io_connect
int connect_to_server(const char *host, uint16_t port, int *sockfd);

//
// A function called by the user of the library before all other functions
// Initializes kernel level threads C-exec and I-exec for managing user thread execution
// initializes mutex
//////////////////////////////////////////////////////////////////////////////////////////////
void sut_init();

//
// A function that c-exec thread manages all user level threads in
// initializes ready queue
//MEM BAR: waits for a signal from sut_create to commence processing
//CALLS tbc_arr_mait(), task_scheduler()
//////////////////////////////////////////////////////////////////////////////////////////////
void *c_exec_routine(void *arg);

//
//a function for performing maitenance on the tcb array
//called from c-exec
//loops through entirety of the tbc array
//schedules tasks that are UNSCHEDULED && ACTIVE
//destroys memory allocated for tasks that are INACTIVE
//if there are no active tasks (numthreads =0) it signals sema_tasks_comp
//this allows shutdown function to commence
//it waits for shutdown on sema_shutdown ///////SHOULD I RELEASE THIS?
//CALLS: add_tcb_q(), tcb_destroy()
//////////////////////////////////////////////////////////////////////////////////////////////
void tcb_arr_maitenance();

//helper function for tcb_arr_maitenance to ready thread status safely while locked
//////////////////////////////////////////////////////////////////////////////////////////////
int get_threadstatus(tcb * threadptr);

//helper function for tcb_arr_maitenance to ready thread scheduling safely while locked
//////////////////////////////////////////////////////////////////////////////////////////////
int get_threadsschdule(tcb * threadptr);

//
//adds a thread to the scheduling queue
//if a thread if status is TOQUEUE or TOWAITQUEUE
//called by tcb_arr_maitenance
//////////////////////////////////////////////////////////////////////////////////////////////
//void add_tcb_q(tcb * enqueueptr);
void add_tcb_q(int i);


//
//destroys a thread by deallocing memory and changing curnum threads
//called by tcb_arr_maitenance()
//////////////////////////////////////////////////////////////////////////////////////////////
void tcb_destroy(tcb * destroyptr);

//
//a simple scheulder that pops the head off the of ready queue
//queue ops encapsulated by mutex
//args: takes pointer to the mutex its using
//returns: a pointer to the thread
//tcb * task_scheduler(pthread_mutex_t *mLock);
//Called by c_exec
//////////////////////////////////////////////////////////////////////////////////////////////
tcb * task_scheduler();

//
//called by user of library in order to add new tas
//new task should be scheduled to run on your c-exec thread
//params:
//fn: function the user would like to run in the task
//MEM BAR: Sends a signal to c-exec to commence loop if hasn't been done
//////////////////////////////////////////////////////////////////////////////////////////////
bool sut_create(sut_task_f fn);

//
//a function that returns the next avail thread id by checking the tcb array
//////////////////////////////////////////////////////////////////////////////////////////////
int avail_threadid();

//
//called within a user task
//the context of the currently running task is saved into tcb
//status of task is saved to tcb, threadschedule = ACTIVE
//so scheduled by c-exec later in maitenance
//control handd back to c_exec thread
//////////////////////////////////////////////////////////////////////////////////////////////
void sut_yield();

//
//a thread is marked as INACTIVE
//control is handed back to c-exec thread
//when tcb array maitenances occurs by c-exec thread, tcb destroyed and mem deallocated
//////////////////////////////////////////////////////////////////////////////////////////////
void sut_exit();

//exect IO ecec to make the connection to the rem process w/o error
//ERRORs not handled
//open the connection and move to the next statement
//////////////////////////////////////////////////////////////////////////////////////////////
void sut_open(char *dest, int port);

//write the data to a remote process NON-BLOCKING
//it copies data to a local buffer and expect IO exec to reliably send it to the dest
//////////////////////////////////////////////////////////////////////////////////////////////
void sut_write(char *buf, int size);

//a function called by a task to close a socket connection
//////////////////////////////////////////////////////////////////////////////////////////////
void sut_close();

//when task issues sut_read, send request to a request queue and put the task itself on a wait queue
//WHEN DATA ARRIVES, task is moved from wait queue to ready queue
//control transferred back to C-exec, task put on wait queue
//////////////////////////////////////////////////////////////////////////////////////////////
char *sut_read();

//the threading library is shutdown
//upon receving a finishing signal, commences shutdown
//kernel level threads joined
//any remaining memory for tcb array deallocated
//mutexes destroyed
//////////////////////////////////////////////////////////////////////////////////////////////
void sut_shutdown();


//
//A function that i-exec thread manages i/o within
//waits until it receives a signal from sut_open to commence io actions
//c-exec signals its shutdown
//////////////////////////////////////////////////////////////////////////////////////////////
void *i_exec_routine(void *arg); 

//i_exec helper function to create and connect to a socket
//////////////////////////////////////////////////////////////////////////////////////////////
void io_connect();

//i_exec helper function to read from a socket
//puts read value in the io_req struct
//////////////////////////////////////////////////////////////////////////////////////////////
void io_read();

//i_exec helper function to write to a socket
//takes value to write from the io_req struct
//////////////////////////////////////////////////////////////////////////////////////////////
void io_write();

//i_exec helper function to close a socket AND uninitialize the io_req struct for next use
//////////////////////////////////////////////////////////////////////////////////////////////
void io_close();


//i_exec helper function to createa socket
//called by io_connect
//////////////////////////////////////////////////////////////////////////////////////////////
int connect_to_server(const char *host, uint16_t port, int *sockfd) ;

#endif
