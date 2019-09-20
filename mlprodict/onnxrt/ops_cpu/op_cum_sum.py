# -*- encoding: utf-8 -*-
# pylint: disable=E0203,E1101,C0111
"""
@file
@brief Runtime operator.
"""
import numpy
from ._op import OpRun


class CumSum(OpRun):

    atts = {'exclusive': 0, 'reverse': 0}

    def __init__(self, onnx_node, desc=None, **options):
        OpRun.__init__(self, onnx_node, desc=desc,
                       expected_attributes=CumSum.atts,
                       **options)

    def _run(self, x, *axis):  # pylint: disable=W0221
        axis = None if len(axis) == 0 else axis[0]
        if axis is None:
            if self.reverse or self.exclusive:
                raise NotImplementedError(
                    'reverse=1 or exclusive=1 not implemented')
            if self.inplaces.get(0, False):
                return (numpy.cumsum(x, out=x), )
            else:
                return (numpy.cumsum(x), )
        if len(axis.shape) != 1 or axis.shape[0] != 1:
            raise RuntimeError(
                "axis must be an array of one number not {}".format(axis))
        axis = axis[0]
        if self.reverse or self.exclusive:
            raise NotImplementedError(
                'reverse=1 or exclusive=1 not implemented')
        if self.inplaces.get(0, False):
            return (numpy.cumsum(x, axis=axis, out=x), )
        else:
            return (numpy.cumsum(x, axis=axis), )

    def _infer_shapes(self, x, *axis):  # pylint: disable=W0221
        return (x, )