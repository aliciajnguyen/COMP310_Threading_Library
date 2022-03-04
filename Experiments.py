from NaiveBayes import * # MultinomialNaiveBayes as m #m for model
import DataBayes as d #d for data
import Utilities as u
from sklearn import naive_bayes

FEATURE_NUM = 10

count_newsgroup_x_train, count_newsgroup_x_test, count_newsgroup_y_train, count_newsgroup_y_test = d.create_newsgroup_count(FEATURE_NUM)

#20 Newsgroup Dataset
#Fit
#model = MultinomialNaiveBayes(N=count_newsgroup_x_train.shape[0], D=count_newsgroup_x_train.shape[1], C = (np.max(count_newsgroup_x_train) + 1))
model = MultinomialNaiveBayes()
model.fit(count_newsgroup_x_train, count_newsgroup_y_train)

#for this we're not returning probabilities, but an array of numbers y_pred representing the class predicted for each instance
y_pred = model.predict(count_newsgroup_x_test)
u.evaluate_acc(count_newsgroup_y_test, y_pred)



