import numpy as np
import math
import pandas as pd
import itertools
import matplotlib.pyplot as plt
from sklearn import datasets
from sklearn.datasets import fetch_20newsgroups
from sklearn.feature_extraction.text import CountVectorizer
from pprint import pprint
from sklearn.feature_extraction.text import TfidfVectorizer
import Utilities as u

def create_newsgroup_count(FEATURE_NUM=None):

    categories = ['alt.atheism',
    'comp.graphics',
    'comp.os.ms-windows.misc',
    'comp.sys.ibm.pc.hardware',
    'comp.sys.mac.hardware',
    'comp.windows.x',
    'misc.forsale',
    'rec.autos',
    'rec.motorcycles',
    'rec.sport.baseball',
    'rec.sport.hockey',
    'sci.crypt',
    'sci.electronics',
    'sci.med',
    'sci.space',
    'soc.religion.christian',
    'talk.politics.guns',
    'talk.politics.mideast',
    'talk.politics.misc',
    'talk.religion.misc']

    ##Try basic load for the form of input necessary for bayes
    #NOTE didn't end up doing split beforehand because it messes with the vectorization 
    #x_train, y_train = newsgroups_train = fetch_20newsgroups(subset='train', remove=('headers', 'footers', 'quotes'), return_X_y=True, random_state=42)
    #x_test, y_test = newsgroups_train = fetch_20newsgroups(subset='test', remove=('headers', 'footers', 'quotes'), return_X_y=True, random_state=42)

    x, y  = fetch_20newsgroups(remove=('headers', 'footers', 'quotes'), return_X_y=True, random_state=42)

#WAS 42

    #VECTORIZATION
    #COUNT
    vectorizer_count = CountVectorizer(stop_words='english', strip_accents='ascii', max_features=FEATURE_NUM)
    count_newsgroup_x = vectorizer_count.fit_transform(x)
    words_as_features_count = vectorizer_count.get_feature_names_out()
    #print(words_as_features_count[50:100])
    #print(count_newsgroup_x)
    #print(count_newsgroup_x.shape)

    #TFID
    #vectorize to make each word a feature
    #vectorizer_tfid = TfidfVectorizer(stop_words='english', strip_accents='ascii', max_features=FEATURE_NUM)
    #tfid_newsgroup_x = vectorizer_tfid.fit_transform(x)
    #words_as_features_tfid = vectorizer_tfid.get_feature_names_out()
    #print(words_as_features_tfid[50:100])
    #print(tfid_newsgroup_x[10:20])
    #print(tfid_newsgroup_x.shape)

    #TEST TRAIN SPLIT
    TRAIN_PERCENTAGE = 80 
    #Count Data
    num_instances_count = count_newsgroup_x.shape[0]
    splitIndex = math.floor(num_instances_count*(TRAIN_PERCENTAGE/100))
    count_newsgroup_x_train, count_newsgroup_x_test = count_newsgroup_x[:splitIndex], count_newsgroup_x[splitIndex:]
    count_newsgroup_y_train, count_newsgroup_y_test = y[:splitIndex], y[splitIndex:]

    #Tfid Data
    #num_instances_tfid = tfid_newsgroup_x.shape[0]
    #splitIndex = math.floor(num_instances_tfid*(TRAIN_PERCENTAGE/100))
    #tfid_newsgroup_x_train, tfid_newsgroup_x_test = tfid_newsgroup_x[:splitIndex], tfid_newsgroup_x[splitIndex:]
    #tfid_newsgroup_y_train, tfid_newsgroup_y_test = y[:splitIndex], y[splitIndex:]

    return count_newsgroup_x_train, count_newsgroup_x_test, count_newsgroup_y_train, count_newsgroup_y_test