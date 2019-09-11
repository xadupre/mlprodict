"""
@brief      test log(time=2s)
"""
import unittest
import numpy
from pyquickhelper.pycode import ExtTestCase, unittest_require_at_least
import skl2onnx
from skl2onnx.algebra.onnx_ops import (  # pylint: disable=E0611
    OnnxAdd, OnnxMul, OnnxSub, OnnxIdentity
)
from skl2onnx.common.data_types import FloatTensorType
from mlprodict.onnxrt.optim.onnx_helper import onnx_statistics
from mlprodict.onnxrt import OnnxInference
from mlprodict.onnxrt.optim import onnx_remove_node_redundant, onnx_remove_node, onnx_optimisations


class TestOptimOnnxRedundant(ExtTestCase):

    @unittest_require_at_least(skl2onnx, '1.5.9999')
    def test_onnx_remove_redundant(self):
        dtype = numpy.float32
        x = numpy.array([1, 2, 4, 5, 5, 4]).astype(
            numpy.float32).reshape((3, 2))
        cop = OnnxAdd('X', numpy.array([1], dtype=dtype))
        cop2 = OnnxAdd('X', numpy.array([1], dtype=dtype))
        cop3 = OnnxAdd('X', numpy.array([2], dtype=dtype))
        cop4 = OnnxSub(OnnxMul(cop, cop3), cop2, output_names=['final'])
        model_def = cop4.to_onnx({'X': x})
        stats = onnx_statistics(model_def, optim=True)
        c1 = model_def.SerializeToString()
        new_model = onnx_remove_node_redundant(model_def, max_hash_size=10)
        c2 = model_def.SerializeToString()
        self.assertEqual(c1, c2)
        stats2 = onnx_statistics(model_def, optim=True)
        stats3 = onnx_statistics(new_model, optim=False)
        self.assertEqual(stats['ninits'], 3)
        self.assertEqual(stats2['ninits'], 3)
        self.assertEqual(stats3['ninits'], 2)
        self.assertEqual(stats2['nnodes'], 5)
        self.assertEqual(stats3['nnodes'], 4)
        oinf1 = OnnxInference(model_def)
        y1 = oinf1.run({'X': x})

        oinf2 = OnnxInference(new_model)
        y2 = oinf2.run({'X': x})
        self.assertEqualArray(y1['final'], y2['final'])

    @unittest_require_at_least(skl2onnx, '1.5.9999')
    def test_onnx_remove_two_outputs(self):
        dtype = numpy.float32
        x = numpy.array([1, 2, 4, 5, 5, 4]).astype(
            numpy.float32).reshape((3, 2))
        cop = OnnxAdd('X', numpy.array([1], dtype=dtype))
        cop2 = OnnxAdd('X', numpy.array(
            [1], dtype=dtype), output_names=['keep'])
        cop3 = OnnxAdd('X', numpy.array([2], dtype=dtype))
        cop4 = OnnxSub(OnnxMul(cop, cop3), cop2, output_names=['final'])
        model_def = cop4.to_onnx({'X': x},
                                 outputs=[('keep', FloatTensorType([None, 2])),
                                          ('final', FloatTensorType([None, 2]))])
        c1 = model_def.SerializeToString()
        self.assertEqual(len(model_def.graph.output), 2)
        c2 = model_def.SerializeToString()
        self.assertEqual(c1, c2)
        stats = onnx_statistics(model_def, optim=True)
        new_model = onnx_remove_node_redundant(model_def, max_hash_size=10)
        stats2 = onnx_statistics(model_def, optim=True)
        stats3 = onnx_statistics(new_model, optim=False)
        self.assertEqual(stats['ninits'], 3)
        self.assertEqual(stats2['ninits'], 3)
        self.assertEqual(stats3['ninits'], 2)
        self.assertEqual(stats2['nnodes'], 5)
        self.assertEqual(stats3['nnodes'], 4)
        oinf1 = OnnxInference(model_def)
        y1 = oinf1.run({'X': x})

        oinf2 = OnnxInference(new_model)
        y2 = oinf2.run({'X': x})
        self.assertEqualArray(y1['final'], y2['final'])
        self.assertEqualArray(y1['keep'], y2['keep'])

    @unittest_require_at_least(skl2onnx, '1.5.9999')
    def test_onnx_remove_redundant_subgraphs(self):
        from skl2onnx.algebra.complex_functions import onnx_squareform_pdist
        x = numpy.array([1, 2, 4, 5, 5, 4]).astype(
            numpy.float32).reshape((3, 2))
        cop = OnnxAdd(OnnxIdentity('input'), 'input')
        cdist = onnx_squareform_pdist(cop, dtype=numpy.float32)
        cdist2 = onnx_squareform_pdist(cop, dtype=numpy.float32)
        cop2 = OnnxAdd(cdist, cdist2, output_names=['cdist'])

        model_def = cop2.to_onnx(
            {'input': FloatTensorType()},
            outputs=[('cdist', FloatTensorType())])
        c1 = model_def.SerializeToString()
        stats = onnx_statistics(model_def, optim=False)
        c2 = model_def.SerializeToString()
        self.assertEqual(c1, c2)
        self.assertIn('subgraphs', stats)
        self.assertGreater(stats['subgraphs'], 1)
        self.assertGreater(stats['op_Identity'], 2)

        new_model = onnx_remove_node_redundant(model_def)
        stats2 = onnx_statistics(new_model, optim=False)
        self.assertEqual(stats['subgraphs'], 2)
        self.assertEqual(stats2['subgraphs'], 1)

        oinf1 = OnnxInference(model_def)
        oinf2 = OnnxInference(new_model)
        y1 = oinf1.run({'input': x})['cdist']
        y2 = oinf2.run({'input': x})['cdist']
        self.assertEqualArray(y1, y2)

        new_model = onnx_remove_node_redundant(model_def)
        stats3 = onnx_statistics(new_model, optim=False)
        self.assertEqual(stats2, stats3)

        new_model = onnx_remove_node(model_def)
        stats3 = onnx_statistics(new_model, optim=False)
        self.assertLess(stats3['size'], stats2['size'])
        self.assertLess(stats3['nnodes'], stats2['nnodes'])
        self.assertLess(stats3['op_Identity'], stats2['op_Identity'])

    @unittest_require_at_least(skl2onnx, '1.5.9999')
    def test_onnx_remove_redundant_subgraphs_full(self):
        from skl2onnx.algebra.complex_functions import onnx_squareform_pdist
        cop = OnnxAdd(OnnxIdentity('input'), 'input')
        cdist = onnx_squareform_pdist(cop, dtype=numpy.float32)
        cdist2 = onnx_squareform_pdist(cop, dtype=numpy.float32)
        cop2 = OnnxAdd(cdist, cdist2, output_names=['cdist'])

        model_def = cop2.to_onnx(
            {'input': FloatTensorType()},
            outputs=[('cdist', FloatTensorType())])
        stats = onnx_statistics(model_def, optim=False)
        new_model = onnx_optimisations(model_def)
        stats2 = onnx_statistics(new_model, optim=False)
        self.assertLess(stats2['size'], stats['size'])
        self.assertLess(stats2['nnodes'], stats['nnodes'])
        self.assertLess(stats2['op_Identity'], stats['op_Identity'])


if __name__ == "__main__":
    unittest.main()