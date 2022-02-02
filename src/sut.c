#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>

#include "queue.h"
#include "sut.h"

void sut_init(){
    numthreads = 0;
    shutdown_k = false;

    pthread_mutex_init(&lock_rq, NULL);
    pthread_mutex_init(&lock_tcb, NULL);
    pthread_mutex_init(&lock_ioreq, NULL);
   
    //initialize semaphores
    sem_init(&sema_task_start, 2, 0);
    
    //allocate memory for the io request structure
    ioreq_ptr= (ioreq *)malloc(sizeof(ioreq));

    //create kernel level threads
    pthread_create(&c_exec_handle, NULL, c_exec_routine, NULL);//last args what we pass to the function
    pthread_create(&i_exec_handle, NULL, i_exec_routine, NULL);
}

void *c_exec_routine(void *arg){  
    tcb *taskptr;
    int s_start_val;

    //initialize the ready and wait queues
    struct queue q = queue_create();
    queue_init(&q);
    ready_qptr = &q;
    struct queue q2 = queue_create();
    queue_init(&q2);
    wait_qptr = &q2;
    
    //MEMORY BARRIER: We ensure that sut_create has happened once so we can start proc
    sem_wait(&sema_task_start);

    //c_exec operations in while
    while(shutdown_k==false){
        tcb_arr_maitenance();

        //CHECK IF OUR NUMBER OF THREADS IS 0 and send signal to shutdown that it can go
        if(numthreads==0){
            shutdown_k=true;
            return 0;
        }

        //unload ready queue via task scheduler
        taskptr = task_scheduler();

        if(taskptr==NULL){
            usleep(1);
            continue;
        }

        pthread_mutex_lock(&lock_tcb);        
        curthread_ptr=taskptr;
        curthread=taskptr->threadid;
        pthread_mutex_unlock(&lock_tcb);

        //switch to executing user level thread
        swapcontext(&c_context, &(taskptr->threadcontext));
    }
}



void tcb_arr_maitenance(){
    tcb * mait_thread;
    for(int i =0; i<MAX_THREADS; i++){
        pthread_mutex_lock(&lock_tcb);
        mait_thread = threadarr[i];
        pthread_mutex_unlock(&lock_tcb);
        //there is no thread at this place in array
        if(mait_thread==NULL){
            continue;
        }
        //if a thread is TOQUEUE and READY its put in queue
        else if(get_threadsschdule(mait_thread)==TOQUEUE && get_threadstatus(mait_thread)==READY){
            add_tcb_q(i);
        }
    }
}
int get_threadstatus(tcb * threadptr){
    int status;
    pthread_mutex_lock(&lock_tcb);
    status=threadptr->waitstatus;
    pthread_mutex_unlock(&lock_tcb);
    return status;
}

int get_threadsschdule(tcb * threadptr){
    int schedule;
    pthread_mutex_lock(&lock_tcb);
    schedule=threadptr->schedule;
    pthread_mutex_unlock(&lock_tcb);
    return schedule;
}

void add_tcb_q(int i){
    //create new node and add to ready queue
    pthread_mutex_lock(&lock_tcb);
    pthread_mutex_lock(&lock_rq);
    struct queue_entry * node = queue_new_node(threadarr[i]);
    queue_insert_tail(ready_qptr, node);
    threadarr[i]->schedule=QUEUED;
    pthread_mutex_unlock(&lock_rq);
    pthread_mutex_unlock(&lock_tcb);
}

void tcb_destroy(tcb * destroyptr){
    int id_dest = destroyptr->threadid;

    //dealloc tcb's stack, structure and pointer
    pthread_mutex_lock(&lock_tcb);
    free(destroyptr->threadstack);      //free stack
    free(destroyptr);                   //free entire dcb entry
    threadarr[id_dest]=NULL;             //destroy pointer itself
    numthreads--;
    pthread_mutex_unlock(&lock_tcb);
}

tcb * task_scheduler(){
    tcb *Qtaskptr;
    struct queue_entry *q_pop_ptr;

    //check if there is something in queue
    q_pop_ptr = queue_peek_front(ready_qptr);
    if (q_pop_ptr==NULL){
        return NULL;
    }

    //if there's something in the queue dequeue it
    pthread_mutex_lock(&lock_rq);    
    q_pop_ptr = queue_pop_head(ready_qptr);   
    Qtaskptr = (tcb *)q_pop_ptr->data;  
    int x = pthread_mutex_unlock(&lock_rq);

    return Qtaskptr;
}


void *i_exec_routine(void *arg){
    int error;
    int task_val=NOTASK; 

    //initialize io_req
    pthread_mutex_lock(&lock_ioreq);
    ioreq_ptr->request_thread=0;
    memset(ioreq_ptr->host, 0, sizeof(ioreq_ptr->host));
    ioreq_ptr->port=0;
    ioreq_ptr->sockfd=0;  
    ioreq_ptr->task=NOTASK;
    memset(ioreq_ptr->read_buffer, 0, sizeof(ioreq_ptr->read_buffer));
    ioreq_ptr->write_buffer=NULL;
    ioreq_ptr->size_write=0;
    pthread_mutex_unlock(&lock_ioreq);

    //i_exec operations loop
    while(shutdown_k==false){
        //busy wait for a thread to make an open request        
        while(task_val==NOTASK && shutdown_k==false){
            pthread_mutex_lock(&lock_ioreq);
            task_val=ioreq_ptr->task;
            pthread_mutex_unlock(&lock_ioreq);
            usleep(1);
        }      

        pthread_mutex_lock(&lock_ioreq);
        task_val=ioreq_ptr->task;
        pthread_mutex_unlock(&lock_ioreq);

        //if task issues request then commence open
        if(task_val==OPEN){
            io_connect();    ;
        }
        
        pthread_mutex_lock(&lock_ioreq);
        task_val=ioreq_ptr->task;
        pthread_mutex_unlock(&lock_ioreq);

        //if task issues request then commence read
        if(task_val==READ){
                io_read();
        }

        pthread_mutex_lock(&lock_ioreq);
        task_val=ioreq_ptr->task;
        pthread_mutex_unlock(&lock_ioreq);

        //if task issues request then commence write
        if(task_val==WRITE){
            io_write();
        }

        pthread_mutex_lock(&lock_ioreq);
        task_val=ioreq_ptr->task;
        pthread_mutex_unlock(&lock_ioreq);

        //commence close because message received in io_req struct
        if(task_val==CLOSE){
            io_close();
        }   

    }
    //if we get here it's because shutdown initiated by c_exec
}

int connect_to_server(const char *host, uint16_t port, int *sockfd) {
    struct sockaddr_in server_address = { 0 };

    pthread_mutex_lock(&lock_ioreq);

    //create a new socket
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd < 0) {
        perror("Failed to create a new socket\n");
    }

    // connect to server
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, host, &(server_address.sin_addr.s_addr));
    server_address.sin_port = htons(port);
    if (connect(*sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Failed to connect to server\n");
    }

    pthread_mutex_unlock(&lock_ioreq);

    return 0;
}


void io_connect(){
    int error;

    //call for socket creation and connection
    error = connect_to_server(ioreq_ptr->host, ioreq_ptr->port, &(ioreq_ptr->sockfd));
    if(error==1){
        printf("FAILLED TO CONNECT TO SERVER.");
    }

    //associate this socket with thread who made request && change that thread's ready status
    pthread_mutex_lock(&lock_ioreq);
    pthread_mutex_lock(&lock_tcb);
    //update ioreq task val
    ioreq_ptr->task=NOTASK;
    //update the thread's attributes
    threadarr[ioreq_ptr->request_thread]->waitstatus=READY;
    threadarr[ioreq_ptr->request_thread]->schedule=TOQUEUE;
    pthread_mutex_unlock(&lock_tcb);
    pthread_mutex_unlock(&lock_ioreq);
}

void io_read(){
    char msg_read[READ_BUF_SIZE];
    int sockfd_temp2;

    pthread_mutex_lock(&lock_ioreq);
    sockfd_temp2 = ioreq_ptr->sockfd;
    pthread_mutex_unlock(&lock_ioreq);

    //receive a message
    memset(msg_read, 0, sizeof(msg_read));
    pthread_mutex_lock(&lock_ioreq);
    ssize_t byte_count = recv(sockfd_temp2, &msg_read, sizeof(msg_read), 0);
    pthread_mutex_unlock(&lock_ioreq);
    if (byte_count<=0){
        printf("ERROR: in reading message\n");
        return;
    }
    
    pthread_mutex_lock(&lock_ioreq);
    pthread_mutex_lock(&lock_tcb);
    //update the io_request structure
    strcpy(ioreq_ptr->read_buffer, msg_read);
    ioreq_ptr->task=NOTASK;
    //tell the associated thread that the data is ready
    threadarr[ioreq_ptr->request_thread]->waitstatus=READY;
    threadarr[ioreq_ptr->request_thread]->schedule=TOQUEUE;
    pthread_mutex_unlock(&lock_tcb);
    pthread_mutex_unlock(&lock_ioreq);
}

void io_write(){
    //send a message
    ssize_t byte_count = send(ioreq_ptr->sockfd, ioreq_ptr->write_buffer, ioreq_ptr->size_write, 0);
    if (byte_count<=0){
        printf("ERROR: in writing message\n");
        return;
    }

    pthread_mutex_lock(&lock_ioreq);
    pthread_mutex_lock(&lock_tcb);
    //update the io_request structure
    ioreq_ptr->task=NOTASK;
    //tell the associated thread that the data is ready
    threadarr[ioreq_ptr->request_thread]->waitstatus=READY;
    threadarr[ioreq_ptr->request_thread]->schedule=TOQUEUE;
    pthread_mutex_unlock(&lock_tcb);
    pthread_mutex_unlock(&lock_ioreq);
}

void io_close(){
    int sockfd;
    int error;

    //read sockfd from io_request structure
    pthread_mutex_lock(&lock_ioreq);
    sockfd=ioreq_ptr->sockfd;
    pthread_mutex_unlock(&lock_ioreq);

    if(error=close(sockfd)==-1){
        printf("ERROR: Socket not closed properly.\n");
    }

    //UNINITIALIZE io_req structure
    pthread_mutex_lock(&lock_ioreq);
    ioreq_ptr->request_thread=0;
    memset(ioreq_ptr->host, 0, sizeof(ioreq_ptr->host));
    ioreq_ptr->port=0;
    ioreq_ptr->sockfd=0;  
    ioreq_ptr->task=NOTASK;
    memset(ioreq_ptr->read_buffer, 0, sizeof(ioreq_ptr->read_buffer));
    ioreq_ptr->write_buffer=NULL;
    ioreq_ptr->size_write=0;
    pthread_mutex_unlock(&lock_ioreq);
}

//CREATING A TASK
bool sut_create(sut_task_f fn){    
    tcb *tdescptr;
    int t_id;

    if(numthreads > MAX_THREADS){
        printf("MAX NUMBER OF THREADS REACHED. Creation failed.\n");
        return false;
     }

    //create a process control block

    //get available thread id
    t_id = avail_threadid();

    //init tcb and put it in array
    pthread_mutex_lock(&lock_tcb);

    tdescptr= (tcb *)malloc(sizeof(tcb));
    tdescptr->threadid = t_id; 
    tdescptr->schedule=TOQUEUE;
    //create thread context    
    getcontext(&(tdescptr->threadcontext));
	tdescptr->threadstack = (char *)malloc(THREAD_STACK_SIZE);
	tdescptr->threadcontext.uc_stack.ss_sp = tdescptr->threadstack;
	tdescptr->threadcontext.uc_stack.ss_size = THREAD_STACK_SIZE;
	tdescptr->threadcontext.uc_link = &c_context;    //WHERE SHOULD IT RETURN TO? unecc?
	tdescptr->threadcontext.uc_stack.ss_flags = 0;
	tdescptr->threadfunc = fn;
    makecontext(&(tdescptr->threadcontext), fn, 1, tdescptr);   //put context into tcb
    threadarr[t_id]=tdescptr;       
    numthreads++;    

    pthread_mutex_unlock(&lock_tcb); 

    //MEMORY BARRIER: if we haven't notified c-exec that it can start while, do now
    int s_start_val;
    sem_getvalue(&sema_task_start, &s_start_val);
    if (s_start_val==0){
        sem_post(&sema_task_start);
    }
    
   	return true;
}

int avail_threadid(){
    int id;
    pthread_mutex_lock(&lock_tcb);
    for(int i=0; i<MAX_THREADS; i++){
        if(threadarr[i]==NULL){
            id=i;
            break;
        }
    }
    pthread_mutex_unlock(&lock_tcb);
    return id;
}

void sut_shutdown(){
    tcb * tocleanptr;

    //join kernel threads
    pthread_join(c_exec_handle, NULL);
    pthread_join(i_exec_handle, NULL);

    //double check all memory is deallocatd -NOT NECESSARY WE DO THIS IN SUT_EXIT
    pthread_mutex_lock(&lock_tcb);
    for(int i =0; i<MAX_THREADS; i++){
        tocleanptr = threadarr[i];
        if(tocleanptr==NULL){
            continue;
        }
        free(tocleanptr->threadstack);
        free(tocleanptr);
    }
    pthread_mutex_unlock(&lock_tcb);

    //free the io request struct
    free(ioreq_ptr);

    //destroy mutexes
    pthread_mutex_destroy(&lock_rq);
    pthread_mutex_destroy(&lock_tcb);
    pthread_mutex_destroy(&lock_ioreq);

    printf("SUT library successfully shut down!\n");
}

void sut_yield(){
    //update threadarr of tcb's w new sched status
    pthread_mutex_lock(&lock_tcb);
    threadarr[curthread]->schedule=TOQUEUE;
    pthread_mutex_unlock(&lock_tcb);

    //swap back to C-Exec
    swapcontext(&(curthread_ptr->threadcontext), &c_context);
}


void sut_exit(){
    //destroy thread
    tcb_destroy(curthread_ptr);

    //swap back to c_exec
    swapcontext (&temp, &c_context);
}

void sut_open(char *dest, int port){
    //instruct io to open a tcp socket at address dest and port port and i_exec in struct
    pthread_mutex_lock(&lock_ioreq);
    pthread_mutex_lock(&lock_tcb);
    //update io req struct
    ioreq_ptr->request_thread=curthread;
    strcpy(ioreq_ptr->host,dest);
    ioreq_ptr->port=port;
    ioreq_ptr->task=NOTASK;
    ioreq_ptr->task=OPEN;
    //update cur thread tcb                           
    threadarr[curthread]->waitstatus=WAIT;
    threadarr[curthread]->schedule=NOQUEUE;         
    pthread_mutex_unlock(&lock_ioreq);
    pthread_mutex_unlock(&lock_tcb);

    swapcontext(&(curthread_ptr->threadcontext), &c_context);
}

char *sut_read(){
    //tell the i_execthread it can read from socket
    pthread_mutex_lock(&lock_ioreq);
    ioreq_ptr->task=READ;
    pthread_mutex_unlock(&lock_ioreq);

    //tcb updated so it will be put on wait queue
    pthread_mutex_lock(&lock_tcb);
    threadarr[curthread]->waitstatus=WAIT;
    threadarr[curthread]->schedule=NOQUEUE;
    pthread_mutex_unlock(&lock_tcb);
    
    swapcontext(&(curthread_ptr->threadcontext), &c_context);
    
    //if we've returned here data is ready
    pthread_mutex_lock(&lock_ioreq);
    strcpy(requester_buf, ioreq_ptr->read_buffer);
    pthread_mutex_unlock(&lock_ioreq);

    return ioreq_ptr->read_buffer;
}


void sut_write(char *buf, int size){
    //tell the i_execthread there is a write task
    pthread_mutex_lock(&lock_ioreq);
    ioreq_ptr->task=WRITE;
    ioreq_ptr->size_write=size;
    ioreq_ptr->write_buffer=buf;
    pthread_mutex_unlock(&lock_ioreq);

    //tcb updated so it will be put on wait queue
    pthread_mutex_lock(&lock_tcb);
    threadarr[curthread]->waitstatus=WAIT;
    threadarr[curthread]->schedule=NOQUEUE;
    pthread_mutex_unlock(&lock_tcb);

    swapcontext(&(curthread_ptr->threadcontext), &c_context);
    //if we've returned here data is ready
}

void sut_close(){
    //send a signal to i_exec that we need to close
    pthread_mutex_lock(&lock_ioreq);
    ioreq_ptr->task=CLOSE;
    pthread_mutex_unlock(&lock_ioreq);

    //tcb updated so it will be put on wait queue
    pthread_mutex_lock(&lock_tcb);
    threadarr[curthread]->waitstatus=READY;
    threadarr[curthread]->schedule=TOQUEUE;
    pthread_mutex_unlock(&lock_tcb);

    swapcontext(&(curthread_ptr->threadcontext), &c_context);
}

