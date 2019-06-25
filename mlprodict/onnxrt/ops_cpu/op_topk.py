# -*- encoding: utf-8 -*-
# pylint: disable=E0203,E1101,C0111
"""
@file
@brief Runtime operator.
"""
import numpy
from ._op import OpRun


class TopK(OpRun):

    atts = {'axis': -1}

    def __init__(self, onnx_node, desc=None, **options):
        if desc is None:
            raise ValueError("desc should not be None.")
        OpRun.__init__(self, onnx_node, desc=desc,
                       expected_attributes=TopK.atts,
                       **options)

    def _run(self, data, ink):  # pylint: disable=W0221
        # Not the most efficient.
        # Negative axis means to begin from the last axis.
        # See https://github.com/Microsoft/onnxruntime/blob/master/onnxruntime/core/providers/cpu/math/top_k.cc#L63.
        k = ink[0]
        axis = self.axis if self.axis >= 0 else (self.axis + len(data.shape))
        sorti = numpy.argsort(data, axis=axis)
        sort = numpy.sort(data, axis=axis)
        if k > 0:
            shapes = [0 for s in data.shape]
            shapes[axis] = data.shape[axis] - k
            indices = tuple(slice(b, e) for b, e in zip(shapes, data.shape))
            return (sort[indices], sorti[indices])
        else:
            return (sort, sorti)