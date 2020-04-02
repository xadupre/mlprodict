"""
@file
@brief Scenarios for validation.
"""
from sklearn.experimental import enable_hist_gradient_boosting  # pylint: disable=W0611
from sklearn import __all__ as sklearn__all__, __version__ as sklearn_version
from sklearn.cluster import KMeans
from sklearn.calibration import CalibratedClassifierCV
from sklearn.decomposition import SparseCoder, LatentDirichletAllocation
from sklearn.ensemble import (
    VotingClassifier, AdaBoostRegressor, VotingRegressor,
    ExtraTreesRegressor, ExtraTreesClassifier,
    RandomForestRegressor, RandomForestClassifier,
    HistGradientBoostingRegressor, HistGradientBoostingClassifier,
    AdaBoostClassifier, GradientBoostingClassifier, GradientBoostingRegressor
)
from sklearn.feature_extraction import DictVectorizer, FeatureHasher
from sklearn.feature_selection import (
    SelectFromModel, SelectPercentile, RFE, RFECV,
    SelectKBest, SelectFwe
)
from sklearn.gaussian_process import GaussianProcessRegressor
from sklearn.gaussian_process.kernels import ExpSineSquared, DotProduct, RationalQuadratic, RBF
from sklearn.linear_model import LogisticRegression, SGDClassifier, LinearRegression
from sklearn.model_selection import GridSearchCV, RandomizedSearchCV
from sklearn.multiclass import OneVsRestClassifier, OneVsOneClassifier, OutputCodeClassifier
from sklearn.multioutput import MultiOutputRegressor, MultiOutputClassifier, ClassifierChain, RegressorChain
from sklearn.neighbors import LocalOutlierFactor, KNeighborsRegressor, KNeighborsClassifier
from sklearn.neural_network import MLPClassifier
from sklearn.preprocessing import Normalizer, PowerTransformer
from sklearn.random_projection import GaussianRandomProjection, SparseRandomProjection
from sklearn.svm import SVC, NuSVC, SVR
from sklearn.tree import DecisionTreeRegressor, DecisionTreeClassifier, ExtraTreeClassifier

try:
    from sklearn.ensemble import StackingClassifier, StackingRegressor
except ImportError:
    # new in 0.22
    StackingClassifier, StackingRegressor = None, None


def build_custom_scenarios():
    """
    Defines parameters values for some operators.

    .. runpython::
        :showcode:

        from mlprodict.onnxrt.validate.validate_scenarios import build_custom_scenarios
        import pprint
        pprint.pprint(build_custom_scenarios())
    """
    options = {
        # skips
        SparseCoder: None,
        # scenarios
        AdaBoostClassifier: [
            ('default', {'n_estimators': 5},
             {'conv_options': [{AdaBoostClassifier: {'zipmap': False}}]}),
        ],
        AdaBoostRegressor: [
            ('default', {'n_estimators': 5}),
        ],
        CalibratedClassifierCV: [
            ('sgd', {
                'base_estimator': SGDClassifier(),
            }),
            ('default', {}),
        ],
        ClassifierChain: [
            ('logreg', {
                'base_estimator': LogisticRegression(solver='liblinear'),
            })
        ],
        DecisionTreeClassifier: [
            ('default', {}, {'conv_options': [
             {DecisionTreeClassifier: {'zipmap': False}}]})
        ],
        DictVectorizer: [
            ('default', {}),
        ],
        ExtraTreeClassifier: [
            ('default', {},
             {'conv_options': [{ExtraTreeClassifier: {'zipmap': False}}]}),
        ],
        ExtraTreesClassifier: [
            ('default', {'n_estimators': 10},
             {'conv_options': [{ExtraTreesClassifier: {'zipmap': False}}]}),
        ],
        ExtraTreesRegressor: [
            ('default', {'n_estimators': 10}),
        ],
        FeatureHasher: [
            ('default', {}),
        ],
        GaussianProcessRegressor: [
            ('expsine', {
                'kernel': ExpSineSquared(),
                'alpha': 20.,
            }, {'optim': [None, 'onnx'],
                'conv_options': [{}, {GaussianProcessRegressor: {'optim': 'cdist'}}]}),
            ('dotproduct', {
                'kernel': DotProduct(),
                'alpha': 100.,
            }),
            ('rational', {
                'kernel': RationalQuadratic(),
                'alpha': 100.,
            }, {'conv_options': [{}, {GaussianProcessRegressor: {'optim': 'cdist'}}]}),
            ('rbf', {
                'kernel': RBF(),
                'alpha': 100.,
            }, {'optim': [None, 'onnx'],
                'conv_options': [{}, {GaussianProcessRegressor: {'optim': 'cdist'}}]}),
        ],
        GaussianRandomProjection: [
            ('eps95', {'eps': 0.95}),
        ],
        GradientBoostingClassifier: [
            ('default', {'n_estimators': 10},
             {'conv_options': [{GradientBoostingClassifier: {'zipmap': False}}]}),
        ],
        GradientBoostingRegressor: [
            ('default', {'n_estimators': 10}),
        ],
        GridSearchCV: [
            ('cl', {
                'estimator': LogisticRegression(solver='liblinear'),
                'param_grid': {'fit_intercept': [False, True]},
            }, ['b-cl', 'm-cl', '~b-cl-64']),
            ('reg', {
                'estimator': LinearRegression(),
                'param_grid': {'fit_intercept': [False, True]},
            }, ['b-reg', 'm-reg', '~b-reg-64']),
            ('reg', {
                'estimator': KMeans(),
                'param_grid': {'n_clusters': [2, 3]},
            }, ['cluster']),
        ],
        HistGradientBoostingClassifier: [
            ('default', {'max_iter': 10},
             {'conv_options': [{HistGradientBoostingClassifier: {'zipmap': False}}]}),
        ],
        HistGradientBoostingRegressor: [
            ('default', {'max_iter': 10}),
        ],
        KNeighborsClassifier: [
            ('default_k3', {'algorithm': 'brute', 'n_neighbors': 3},
             {'conv_options': [{}, {KNeighborsClassifier: {'optim': 'cdist', 'zipmap': False}}]}),
            ('weights_k3', {'algorithm': 'brute',
                            'weights': 'distance', 'n_neighbors': 3},
             {'conv_options': [{KNeighborsClassifier: {'optim': 'cdist', 'zipmap': False}}]}),
            ('kdt_k3', {'algorithm': 'kd_tree', 'n_neighbors': 3},
             {'conv_options': [{KNeighborsClassifier: {'optim': 'cdist', 'zipmap': False}}]}),
            ('kdt_mink_k3', {'algorithm': 'kd_tree',
                             'metric': "minkowski",
                             'metric_params': {'p': 2.1},
                             'n_neighbors': 3},
             {'conv_options': [{KNeighborsClassifier: {
                 'optim': 'cdist', 'zipmap': False}}]}),
        ],
        KNeighborsRegressor: [
            ('default_k3', {'algorithm': 'brute', 'n_neighbors': 3},
             {'conv_options': [{}, {KNeighborsRegressor: {'optim': 'cdist'}}]}),
            ('weights_k3', {'algorithm': 'brute',
                            'weights': 'distance', 'n_neighbors': 3},
             {'conv_options': [{KNeighborsRegressor: {'optim': 'cdist'}}]}),
            ('kd_tree_k3', {'algorithm': 'kd_tree', 'n_neighbors': 3}),
            ('mink_k3', {'algorithm': 'kd_tree',
                         'metric': "minkowski",
                         'metric_params': {'p': 2.1},
                         'n_neighbors': 3},
             {'conv_options': [{KNeighborsRegressor: {'optim': 'cdist'}}]}),
        ],
        LatentDirichletAllocation: [
            ('default', {'n_components': 2}),
        ],
        LocalOutlierFactor: [
            ('novelty', {'novelty': True}),
        ],
        LogisticRegression: [
            ('liblinear', {'solver': 'liblinear', },
             {'optim': [None, 'onnx'],
              'conv_options': [{}, {LogisticRegression: {'zipmap': False}}],
              'subset_problems': ['b-cl', '~b-cl-64', 'm-cl']}),
            ('liblinear-dec',
             {'solver': 'liblinear', },
             {'conv_options': [{LogisticRegression: {'raw_scores': True, 'zipmap': False}}],
              'subset_problems': ['~b-cl-dec', '~m-cl-dec']}),
        ],
        MLPClassifier: [
            ('default', {}, {'conv_options': [
             {MLPClassifier: {'zipmap': False}}]}),
        ],
        MultiOutputClassifier: [
            ('logreg', {
                'estimator': LogisticRegression(solver='liblinear'),
            })
        ],
        MultiOutputRegressor: [
            ('linreg', {
                'estimator': LinearRegression(),
            })
        ],
        Normalizer: [
            ('l2', {'norm': 'l2', }),
            ('l1', {'norm': 'l1', }),
            ('max', {'norm': 'max', }),
        ],
        NuSVC: [
            ('prob', {
                'probability': True,
            }),
        ],
        OneVsOneClassifier: [
            ('logreg', {
                'estimator': LogisticRegression(solver='liblinear'),
            })
        ],
        OneVsRestClassifier: [
            ('logreg', {
                'estimator': LogisticRegression(solver='liblinear'),
            })
        ],
        OutputCodeClassifier: [
            ('logreg', {
                'estimator': LogisticRegression(solver='liblinear'),
            })
        ],
        PowerTransformer: [
            ('yeo-johnson', {'method': 'yeo-johnson'}),
            ('box-cox', {'method': 'box-cox'}),
        ],
        RandomForestClassifier: [
            ('default', {'n_estimators': 10},
             {'conv_options': [{}, {RandomForestClassifier: {'zipmap': False}}]}),
        ],
        RandomForestRegressor: [
            ('default', {'n_estimators': 10}),
        ],
        RandomizedSearchCV: [
            ('cl', {
                'estimator': LogisticRegression(solver='liblinear'),
                'param_distributions': {'fit_intercept': [False, True]},
            }),
            ('reg', {
                'estimator': LinearRegression(),
                'param_distributions': {'fit_intercept': [False, True]},
            }),
        ],
        RegressorChain: [
            ('linreg', {
                'base_estimator': LinearRegression(),
            })
        ],
        RFE: [
            ('reg', {
                'estimator': LinearRegression(),
            })
        ],
        RFECV: [
            ('reg', {
                'estimator': LinearRegression(),
            })
        ],
        SelectFromModel: [
            ('rf', {
                'estimator': DecisionTreeRegressor(),
            }),
        ],
        SelectFwe: [
            ('alpha100', {
                'alpha': 100.0,
            }),
        ],
        SelectKBest: [
            ('k2', {
                'k': 2,
            }),
        ],
        SelectPercentile: [
            ('p50', {
                'percentile': 50,
            }),
        ],
        SGDClassifier: [
            ('log', {'loss': 'log'},
             {'conv_options': [{SGDClassifier: {'zipmap': False}}]}),
        ],
        SparseRandomProjection: [
            ('eps95', {'eps': 0.95}),
        ],
        SVC: [
            ('linear', {'probability': True, 'kernel': 'linear'},
             {'conv_options': [{SVC: {'zipmap': False}}]}),
            ('poly', {'probability': True, 'kernel': 'poly'},
             {'conv_options': [{SVC: {'zipmap': False}}]}),
            ('rbf', {'probability': True, 'kernel': 'rbf'},
             {'conv_options': [{SVC: {'zipmap': False}}]}),
            ('sigmoid', {'probability': True, 'kernel': 'sigmoid'},
             {'conv_options': [{SVC: {'zipmap': False}}]}),
        ],
        SVR: [
            ('linear', {'kernel': 'linear'}),
            ('poly', {'kernel': 'poly'}),
            ('rbf', {'kernel': 'rbf'}),
            ('sigmoid', {'kernel': 'sigmoid'}),
        ],
        VotingClassifier: [
            ('logreg-noflatten', {
                'voting': 'soft',
                'flatten_transform': False,
                'estimators': [
                    ('lr1', LogisticRegression(
                        solver='liblinear', fit_intercept=True)),
                    ('lr2', LogisticRegression(
                        solver='liblinear', fit_intercept=False)),
                ],
            }, {'conv_options': [{VotingClassifier: {'zipmap': False}}]})
        ],
        VotingRegressor: [
            ('linreg', {
                'estimators': [
                    ('lr1', LinearRegression()),
                    ('lr2', LinearRegression(fit_intercept=False)),
                ],
            })
        ],
    }
    if StackingClassifier is not None and StackingRegressor is not None:
        options.update({
            StackingClassifier: [
                ('logreg', {
                    'estimators': [
                        ('lr1', LogisticRegression(solver='liblinear')),
                        ('lr2', LogisticRegression(
                            solver='liblinear', fit_intercept=False)),
                    ],
                }, {'conv_options': [{StackingClassifier: {'zipmap': False}}]})
            ],
            StackingRegressor: [
                ('linreg', {
                    'estimators': [
                        ('lr1', LinearRegression()),
                        ('lr2', LinearRegression(fit_intercept=False)),
                    ],
                })
            ],
        })
    return options


def interpret_options_from_string(st):
    """
    Converts a string into a dictionary.

    @param      st      string
    @return             evaluated object
    """
    if isinstance(st, dict):
        return st
    value = eval(st)  # pylint: disable=W0123
    return value


_extra_parameters = build_custom_scenarios()
