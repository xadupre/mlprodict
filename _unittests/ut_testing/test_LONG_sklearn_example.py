"""
@brief      test log(time=2s)
"""
import unittest
import os
import pickle
import numpy
import pandas
import matplotlib.pyplot as plt
from pyquickhelper.pycode import ExtTestCase, get_temp_folder
from pyquickhelper.loghelper import fLOG
from pyquickhelper.loghelper.repositories.pygit_helper import clone
from sklearn.cross_decomposition import CCA
from sklearn.multiclass import OneVsRestClassifier
from sklearn.svm import SVC
from skl2onnx.common.exceptions import MissingShapeCalculator
from mlprodict.testing.script_testing import verify_script, MissingVariableError


class TestLONGSklearnExample(ExtTestCase):

    skipped_examples = {
        'plot_changed_only_pprint_parameter.py',  # no trained model
        # NameError: name 'l' is not defined (comprehension)
        'plot_isotonic_regression.py',
        'plot_johnson_lindenstrauss_bound.py',  # no trained model
        # 'numpy.bool_' object has no attribute 'encode'
        'plot_roc_curve_visualization_api.py',
        'plot_face_recognition.py',  # too long
        "plot_affinity_propagation.py",  # no converter
        'plot_coin_segmentation.py',  # spectral clustering
    }

    begin = 1000 if __name__ == "__main__" else 0

    existing_loc = {
        'CCA': CCA,
        'np': numpy,
        'OneVsRestClassifier': OneVsRestClassifier,
        'plt': plt,
        'SVC': SVC,
    }

    skipped_folder = {
        'applications',  # too long
        'bicluster',  # no converter
    }

    def test_plot_examples(self):
        fLOG(__file__, self._testMethodName, OutputPrint=__name__ == "__main__")
        datas = ["git"]

        no_onnx = {}
        noconv = {}
        skipped = {}
        issues = {}
        has_onnx = {}

        for data in datas:  # pylint: disable=R1702
            if data == "git":
                temp_git = get_temp_folder(
                    __file__, "temp_sklearn", clean=False)
                examples = os.path.join(temp_git, "examples")
                if not os.path.exists(examples):
                    fLOG('cloning scikit-learn...')
                    clone(temp_git, "github.com", "scikit-learn",
                          "scikit-learn", fLOG=fLOG)
                    fLOG('done.')
                data = examples
            fold = os.path.join(os.path.dirname(__file__), data)
            if not os.path.exists(fold):
                continue
            for ind_root, (root, dirs, files) in enumerate(os.walk(fold)):
                last = os.path.split(root)[-1]
                if last in TestLONGSklearnExample.skipped_folder:
                    continue
                for ind, nfile in enumerate(files):
                    full_ind = ind_root * 1000 + ind
                    if full_ind < TestLONGSklearnExample.begin:
                        skipped[nfile] = None
                        continue
                    nfile = nfile.replace("\\", "/")
                    if nfile in TestLONGSklearnExample.skipped_examples:
                        skipped[nfile] = None
                        continue
                    ext = os.path.splitext(nfile)[-1]
                    if ext != '.py':
                        continue
                    name = os.path.split(nfile)[-1]
                    if not name.startswith('plot_'):
                        continue
                    fLOG("verify {}/{}:{} - '{}'".format(ind +
                                                         1, len(files), full_ind, nfile))
                    plot = os.path.join(root, nfile)

                    try:
                        res = verify_script(
                            plot, existing_loc=TestLONGSklearnExample.existing_loc)
                    except MissingShapeCalculator as e:
                        fLOG('    missing converter', str(e).split('\n')[0])
                        noconv[nfile] = e
                        continue
                    except MissingVariableError as e:
                        issues[nfile] = e
                        fLOG('    missing variable', str(e).split('\n')[0])
                        continue
                    except (KeyError, NameError, RuntimeError, TypeError,
                            ImportError, AttributeError) as e:
                        issues[nfile] = e
                        fLOG('    local function', str(e).split('\n')[0])
                        continue

                    if res is not None:
                        if any(filter(lambda n: n.endswith('_onnx'), res['locals'])):
                            fLOG('   ONNX ok')
                            has_onnx[nfile] = res['onx_info']
                            if __name__ == "__main__":
                                break
                        else:
                            no_onnx[nfile] = res['locals']
                            fLOG('   no onnx')
                    else:
                        fLOG('   issue')
                if len(has_onnx) > 0 and __name__ == "__main__":
                    break
            if len(has_onnx) > 0 and __name__ == "__main__":
                break

        if len(has_onnx) == 0:
            raise RuntimeError("Unable to find any example in\n{}".format(
                "\n".join(datas)))

        rows = []
        for n, d in [('no_onnx', no_onnx),
                     ('noconv', noconv),
                     ('issues', issues),
                     ]:
            for k, v in d.items():
                sv = str(v).replace("\r", "").replace("\n", " ")
                rows.append(dict(name=k, result=sv, kind=n))
        for k, v in has_onnx.items():
            rows.append(dict(name=k, result='OK', kind='ONNX'))

        temp = get_temp_folder(__file__, 'temp_plot_examples')
        stats = os.path.join(temp, "stats.csv")
        pandas.DataFrame(rows).to_csv(stats, index=False)
        for k, v in has_onnx.items():
            pkl = os.path.join(temp, k + '.pkl')
            with open(pkl, 'wb') as f:
                pickle.dump(v, f)


if __name__ == "__main__":
    unittest.main()