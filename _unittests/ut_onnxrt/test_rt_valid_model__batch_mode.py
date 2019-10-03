"""
@brief      test log(time=2s)
"""
import unittest
from logging import getLogger
import onnx
from pyquickhelper.loghelper import fLOG
from pyquickhelper.pycode import ExtTestCase, unittest_require_at_least
from sklearn.exceptions import ConvergenceWarning
from sklearn.utils.testing import ignore_warnings
from skl2onnx import __version__ as skl2onnx_version
from mlprodict.onnxrt.validate import enumerate_validated_operator_opsets


class TestRtValidateBatchMode(ExtTestCase):

    @ignore_warnings(category=(UserWarning, ConvergenceWarning, RuntimeWarning))
    def test_rt_select_python(self):
        fLOG(__file__, self._testMethodName, OutputPrint=__name__ == "__main__")
        logger = getLogger('skl2onnx')
        logger.disabled = True
        verbose = 1 if __name__ == "__main__" else 0

        debug = True
        buffer = []

        def myprint(*args, **kwargs):
            buffer.append(" ".join(map(str, args)))

        rows = list(enumerate_validated_operator_opsets(
            verbose, models={
                "SelectPercentile", "SelectKBest",
                "SelectFwe", "SelectFdr", "SelectFpr",
                "SelectFromModel",
            },
            opset_min=11, fLOG=myprint,
            runtime='python', debug=debug,
            filter_exp=lambda m, p: '-64' not in p))
        self.assertGreater(len(rows), 1)
        self.assertGreater(len(buffer), 1 if debug else 0)

    @ignore_warnings(category=(UserWarning, ConvergenceWarning, RuntimeWarning))
    def test_rt_perceptron_python_nodec(self):
        fLOG(__file__, self._testMethodName, OutputPrint=__name__ == "__main__")
        logger = getLogger('skl2onnx')
        logger.disabled = True
        verbose = 1 if __name__ == "__main__" else 0

        debug = True
        buffer = []

        def myprint(*args, **kwargs):
            buffer.append(" ".join(map(str, args)))

        rows = list(enumerate_validated_operator_opsets(
            verbose, models={"Perceptron"},
            opset_min=11, fLOG=myprint,
            runtime='python', debug=debug,
            filter_exp=lambda m, p: '-dec' not in p))
        self.assertGreater(len(rows), 1)
        self.assertGreater(len(buffer), 1 if debug else 0)

    @ignore_warnings(category=(UserWarning, ConvergenceWarning, RuntimeWarning))
    def test_rt_perceptron_python_dec(self):
        fLOG(__file__, self._testMethodName, OutputPrint=__name__ == "__main__")
        logger = getLogger('skl2onnx')
        logger.disabled = True
        verbose = 2 if __name__ == "__main__" else 0

        debug = False
        buffer = []

        def myprint(*args, **kwargs):
            buffer.append(" ".join(map(str, args)))

        rows = list(enumerate_validated_operator_opsets(
            verbose, models={
                "Perceptron",
            },
            opset_min=11, fLOG=myprint,
            runtime='python', debug=debug,
            filter_exp=lambda m, p: '-dec' in p))
        self.assertGreater(len(rows), 1)
        self.assertGreater(len(buffer), 1 if debug else 0)

    @unittest_require_at_least(onnx, '1.5.29')
    @ignore_warnings(category=(UserWarning, ConvergenceWarning, RuntimeWarning))
    def test_rt_polynomial_python(self):
        fLOG(__file__, self._testMethodName, OutputPrint=__name__ == "__main__")
        logger = getLogger('skl2onnx')
        logger.disabled = True
        verbose = 1 if __name__ == "__main__" else 0

        debug = True
        buffer = []

        def myprint(*args, **kwargs):
            buffer.append(" ".join(map(str, args)))

        rows = list(enumerate_validated_operator_opsets(
            verbose, models={"PolynomialFeatures"},
            opset_min=11, fLOG=myprint,
            runtime='python', debug=debug,
            filter_exp=lambda m, p: '-64' not in p))
        self.assertGreater(len(rows), 1)
        self.assertGreater(len(buffer), 1 if debug else 0)

    @ignore_warnings(category=(UserWarning, ConvergenceWarning, RuntimeWarning))
    def test_rt_rfe(self):
        fLOG(__file__, self._testMethodName, OutputPrint=__name__ == "__main__")
        logger = getLogger('skl2onnx')
        logger.disabled = True
        verbose = 1 if __name__ == "__main__" else 0

        debug = True
        buffer = []

        def myprint(*args, **kwargs):
            buffer.append(" ".join(map(str, args)))

        rows = list(enumerate_validated_operator_opsets(
            verbose, models={"RFE"},
            opset_min=11, fLOG=myprint,
            runtime='python', debug=debug,
            filter_exp=lambda m, p: '-64' not in p))
        self.assertGreater(len(rows), 1)
        self.assertGreater(len(buffer), 1 if debug else 0)


if __name__ == "__main__":
    # TestRtValidateBatchMode().test_rt_rfe()
    unittest.main()