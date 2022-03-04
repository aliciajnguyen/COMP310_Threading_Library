from functools import total_ordering
import numpy as np
import itertools
import scipy
import matplotlib.pyplot as plt
from sklearn import datasets
from sklearn import datasets
from sklearn.datasets import fetch_20newsgroups
from sklearn.feature_extraction.text import CountVectorizer
from pprint import pprint
from sklearn.feature_extraction.text import TfidfVectorizer
import Utilities as u

class MultinomialNaiveBayes:
    
    def __init__(self, alpha=1):
        self.alpha = 1 #for laplace smoothing
        #self.C  # num of classes
        #self.D  # num of features/words
        #self.N  # num of instances in train
        #self.theta_c
        #self.theta_x
        return
        #attributes we add to this object later
          #self.pi = (Nc+1)/(N+C)       #Laplace Smooth of prior

    
    def fit(self, x, y):
        #TODO make the constructor take these instead
        N, D = x.shape
        C = np.max(y) + 1

        #TODO move to constructor: set sizes after discarding features #PASSED TO CONSTRUCTOR AS PER ASSIGN INSTRUCTS
        self.C = C
        self.D = D
        self.N = N

        print("N, D, C:", N, D, C)

        #counts to help us calaculate our parametrs
        #word_count_d = np.zeros((C,D))         #Wc has C rows and D columns to represent the counts of word 'd' in all documents labelled 'this class'
        #word_count_total = np.zeros(C)          #were we'll put out total word ocunts to operate w theta and calc likelihoods

        #our parameters
        #theta_x = np.zeros((C,D)) #likelihood of each feature p(x_i|y)
        #theta_c = np.zeros(C) # priors
        log_theta_x = np.zeros((C,D)) #log likelihood of each feature p(x_i|y)
        log_theta_c = np.zeros(C) # log priors
        
        #Nc = np.zeros(C) # number of instances in class c (to fill for all classes)

        #compute likelihoods for each class, for each word (and the prior while we're at it)
        for c in range(C):

            #get all rows of data belonging to c, gather some data about it
            x_c = x[y == c]                       #slice all the elements from class c
            num_class_elements = x_c.shape[0]     #get number of elements of class c
            #Nc[c] = num_class_elements                  

            #calculate p(x_i|y)  the likelihood for each word in this class
            #sum of all word counts in this class, from our one class data splice
            occurences = np.sum(x_c, axis =0) # D 1d matrix
                        
            #LAPLACE SMOOTHING alpha + 1
            #TODO AH SHOULD THIS BE DONE IN THIS STEP THO, with this splice, for for enttire x_train?
            occurences = occurences + self.alpha if self.alpha > 0 else occurences
            
            #count all words in class
            total_words_in_class = np.sum(occurences, axis=1)  #don't get confused, just a value (not matrix) that gets put in word_count_total      
            #word_count_total[c] = total_words_in_class + D if self.isSmooth else total_words_in_class 
            #SMOOTH, our vocabular size is just the number of features bc of how we processed data, so D
            
            #calculate frequence
            frequency = np.apply_along_axis((lambda x : x/total_words_in_class),axis=1, arr=occurences)
            #theta_x[c,:] = frequency

            #now do it in the log domin
            log_total_words_in_class = np.log(total_words_in_class)
            log_frequency = np.apply_along_axis((lambda x : np.log(x) - log_total_words_in_class) ,axis=1, arr=occurences)
            log_theta_x[c,:] = log_frequency

            #smooth here
            num_class_elements = num_class_elements + 1 if self.isSmooth else num_class_elements
            total_elements = N + C if self.isSmooth else N

            #get prior of class (and the log domain prior)
            #theta_c[c] = num_class_elements/ total_elements
            log_theta_c[c] = np.log(num_class_elements) - np.log(total_elements)


        #self.theta_x = theta_x
        #self.theta_c = theta_c 
        self.log_theta_x = log_theta_x
        self.log_theta_c = log_theta_c 

        #self.pi = (Nc+1)/(N+C)                        #Laplace smoothing (using alpha_c=1 for all c) for pi = prior
     


        return self

    def predict(self, xt):
        Ntest, Dtest = xt.shape
 
        y_pred = np.zeros((Ntest))

        #SAVE MEMORY TRICK
        #this 1d array has a slot for each class
        #we'll temporarily store the calculate p(y) * p(x_i|y) values here, then take arg max for predic
        #but since we go instance by instance, minimal new memoray allocation
        #this array populated in the for loop
        posteriors_per_class = np.zeros(self.C)
        #array to store the likelihoods for cur class
        likelihoods_class = np.zeros(self.D)
        log_likelihoods_class = np.zeros(self.D)

        #to avoid memory issues we'll go instance by instance
        for n in range(Ntest):
          
          instance = xt[n].toarray() #remember this will be sparse array

          for c in range(self.C):
            log_prior = self.log_theta_c[c]

            #likelihoods = self.theta_x[c] 
            #work in log domain, a single row of theta representing ALL likelihoods for that class (for each feature/word D)
            #log_likelihoods = np.log(likelihoods)
            #TRY:
            log_likelihoods = self.log_theta_x[c]

            #for multinomial likelihood, we would calculate theta^count
            #but since we're working in log domain: log(theta^count) = count * log(theta)
            #so perform the likelihood calculation ONLY WHERE WE HAVE COUNT (ie where instnace[i] != 0)
            #otherwise we return a 0 and that'll be fine because it won't affect the sum we're doing bc log domain
            log_likelihoods =np.where((instance != 0), np.multiply(instance, log_likelihoods), 0)
            log_likelihood_sum = np.sum(log_likelihoods)

            #log_prediction_per_class[c] = log_likelihood_sum + log_prior 
            
            posteriors_per_class[c] = np.exp(log_likelihood_sum + log_prior) 


            #after we've done our for loop for each class, but before we move on to the next instnace
            #take the max value, put in yhat as our prediction for this instance
        
        #y_pred[n] = np.max(log_prediction_per_class, axis = 0)
        y_pred[n] = np.argmax(posteriors_per_class, axis = 0)

        return y_pred