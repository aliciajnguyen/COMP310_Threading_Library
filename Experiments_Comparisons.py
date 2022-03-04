from NaiveBayes import * # MultinomialNaiveBayes as m #m for model
import DataBayes as d #d for data
import Utilities as u
from sklearn import naive_bayes

FEATURE_NUM = None

count_newsgroup_x_train, count_newsgroup_x_test, count_newsgroup_y_train, count_newsgroup_y_test = d.create_newsgroup_count(FEATURE_NUM)

test_model = naive_bayes.MultinomialNB()
test_model.fit(X=count_newsgroup_x_train, y=count_newsgroup_y_train)
y_pred = test_model.predict(count_newsgroup_x_test)
u.evaluate_acc(count_newsgroup_y_test, y_pred)