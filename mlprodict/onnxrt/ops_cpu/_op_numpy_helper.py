"""
@file
@brief numpy redundant functions.
"""
import numpy


def numpy_dot_inplace(inplaces, a, b):
    """
    Implements a dot product, deals with inplace information.
    """
    if inplaces.get(0, False):
        if a.flags['F_CONTIGUOUS']:
            if len(b.shape) == len(a.shape) == 2 and b.shape[1] <= a.shape[1]:
                numpy.dot(a, b, out=a[:, :b.shape[1]])
                return a[:, :b.shape[1]]
            if len(b.shape) == 1:
                numpy.dot(a, b.reshape(b.shape[0], 1), out=a[:, :1])
                return a[:, :1].reshape(a.shape[0])
    if inplaces.get(1, False):
        if b.flags['C_CONTIGUOUS']:
            if len(b.shape) == len(a.shape) == 2 and a.shape[0] <= b.shape[0]:
                numpy.dot(a, b, out=b[:a.shape[0], :])
                return b[:a.shape[0], :]
            if len(a.shape) == 1:
                numpy.dot(a, b, out=b[:1, :])
                return b[:1, :]
    return numpy.dot(a, b)