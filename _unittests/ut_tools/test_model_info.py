"""
@brief      test log(time=8s)
"""
import unittest
from lightgbm import LGBMClassifier, LGBMRegressor
from xgboost import XGBClassifier, XGBRegressor
from sklearn.experimental import enable_hist_gradient_boosting  # pylint: disable=W0611
from sklearn.datasets import load_iris
from sklearn.linear_model import LogisticRegression, LinearRegression
from sklearn.neighbors import KNeighborsClassifier
from sklearn.tree import DecisionTreeClassifier, DecisionTreeRegressor
from sklearn.ensemble import (
    GradientBoostingClassifier,
    HistGradientBoostingClassifier,
    HistGradientBoostingRegressor,
    RandomForestClassifier,
    RandomForestRegressor,
)
from pyquickhelper.pycode import ExtTestCase, skipif_circleci
from mlprodict.tools.model_info import analyze_model


class TestModelInfo(ExtTestCase):

    def fit(self, model):
        data = load_iris()
        X, y = data.data, data.target
        model.fit(X, y)
        return model

    def test_logreg(self):
        model = self.fit(LogisticRegression(solver='liblinear'))
        info = analyze_model(model)
        self.assertIn('classes_.shape', info)
        self.assertEqual(info['classes_.shape'], 3)
        self.assertEqual(info['coef_.shape'], (3, 4))
        self.assertEqual(info['intercept_.shape'], 3)

    def test_linreg(self):
        model = self.fit(LinearRegression())
        info = analyze_model(model)
        self.assertEqual(info['coef_.shape'], 4)
        self.assertEqual(info['intercept_.shape'], 1)

    def test_dtc(self):
        model = self.fit(DecisionTreeClassifier())
        info = analyze_model(model)
        self.assertIn('classes_.shape', info)
        self.assertEqual(info['classes_.shape'], 3)
        self.assertGreater(info['tree_.node_count'], 15)
        self.assertGreater(info['tree_.leave_count'], 8)
        self.assertGreater(info['tree_.max_depth'], 3)

    def test_dtr(self):
        model = self.fit(DecisionTreeRegressor())
        info = analyze_model(model)
        self.assertGreater(info['tree_.node_count'], 15)
        self.assertGreater(info['tree_.leave_count'], 8)
        self.assertGreater(info['tree_.max_depth'], 3)

    def test_rfc(self):
        model = self.fit(RandomForestClassifier())
        info = analyze_model(model)
        self.assertIn('classes_.shape', info)
        self.assertEqual(info['classes_.shape'], 3)
        self.assertEqual(info['estimators_.classes_.shape'], 3)
        self.assertGreater(info['estimators_.size'], 10)
        self.assertGreater(info['estimators_.sum|tree_.node_count'], 100)
        self.assertGreater(info['estimators_.sum|tree_.leave_count'], 100)
        self.assertGreater(info['estimators_.max|tree_.max_depth'], 3)

    def test_rfr(self):
        model = self.fit(RandomForestRegressor())
        info = analyze_model(model)
        self.assertGreater(info['estimators_.size'], 10)
        self.assertGreater(info['estimators_.sum|tree_.node_count'], 100)
        self.assertGreater(info['estimators_.sum|tree_.leave_count'], 100)
        self.assertGreater(info['estimators_.max|tree_.max_depth'], 3)

    def test_hgbc(self):
        model = self.fit(HistGradientBoostingClassifier())
        info = analyze_model(model)
        self.assertIn('classes_.shape', info)
        self.assertEqual(info['classes_.shape'], 3)
        self.assertGreater(info['_predictors.size'], 10)
        self.assertGreater(info['_predictors.sum|tree_.node_count'], 100)
        self.assertGreater(info['_predictors.sum|tree_.leave_count'], 100)
        self.assertGreater(info['_predictors.max|tree_.max_depth'], 3)

    def test_hgbr(self):
        model = self.fit(HistGradientBoostingRegressor())
        info = analyze_model(model)
        self.assertGreater(info['_predictors.size'], 10)
        self.assertGreater(info['_predictors.sum|tree_.node_count'], 100)
        self.assertGreater(info['_predictors.sum|tree_.leave_count'], 100)
        self.assertGreater(info['_predictors.max|tree_.max_depth'], 3)

    @skipif_circleci('issue, too long')
    def test_lgbmc(self):
        model = self.fit(LGBMClassifier())
        info = analyze_model(model)
        self.assertEqual(info['n_classes'], 3)
        self.assertGreater(info['ntrees'], 10)
        self.assertEqual(info['objective'], 'multiclass num_class:3')
        self.assertGreater(info['estimators_.size'], 10)
        self.assertGreater(info['leave_count'], 100)
        self.assertGreater(info['mode_count'], 2)
        self.assertGreater(info['node_count'], 100)

    @skipif_circleci('issue, too long')
    def test_lgbmr(self):
        model = self.fit(LGBMRegressor())
        info = analyze_model(model)
        self.assertGreater(info['ntrees'], 10)
        self.assertEqual(info['objective'], 'regression')
        self.assertGreater(info['estimators_.size'], 10)
        self.assertGreater(info['leave_count'], 100)
        self.assertGreater(info['mode_count'], 2)
        self.assertGreater(info['node_count'], 100)

    @skipif_circleci('issue, too long')
    def test_xgbc(self):
        model = self.fit(XGBClassifier())
        info = analyze_model(model)
        self.assertEqual(info['classes_.shape'], 3)
        self.assertGreater(info['ntrees'], 10)
        self.assertEqual(info['objective'], 'multi:softprob')
        self.assertGreater(info['estimators_.size'], 10)
        self.assertGreater(info['leave_count'], 100)
        self.assertGreater(info['mode_count'], 2)
        self.assertGreater(info['node_count'], 100)

    @skipif_circleci('issue, too long')
    def test_xgbr(self):
        model = self.fit(XGBRegressor())
        info = analyze_model(model)
        self.assertGreater(info['ntrees'], 10)
        self.assertEqual(info['objective'], 'reg:linear')
        self.assertGreater(info['estimators_.size'], 10)
        self.assertGreater(info['leave_count'], 100)
        self.assertGreater(info['mode_count'], 2)
        self.assertGreater(info['node_count'], 100)

    def test_knnc(self):
        model = self.fit(KNeighborsClassifier())
        info = analyze_model(model)
        self.assertIn('classes_.shape', info)
        self.assertEqual(info['classes_.shape'], 3)
        self.assertEqual(info['_fit_X.shape'], (150, 4))

    def test_gbc(self):
        model = self.fit(GradientBoostingClassifier())
        info = analyze_model(model)
        self.assertIn('classes_.shape', info)
        self.assertEqual(info['classes_.shape'], 3)
        self.assertGreater(info['estimators_.sum|.sum|tree_.node_count'], 15)
        self.assertGreater(info['estimators_.sum|.sum|tree_.leave_count'], 8)
        self.assertGreater(info['estimators_.max|.max|tree_.max_depth'], 3)


if __name__ == "__main__":
    unittest.main()
