import numpy as np

def evaluate_acc(y_test, y_pred):
    accuracy = np.sum(y_pred == y_test)/y_test.shape[0]
    print(f'test accuracy: {accuracy}')
    return accuracy

def logsumexp(Z):                                              # dimension C x N
    Zmax = np.max(Z,axis=0)[None,:]                              # max over C
    log_sum_exp = Zmax + np.log(np.sum(np.exp(Z - Zmax), axis=0))
    return log_sum_exp