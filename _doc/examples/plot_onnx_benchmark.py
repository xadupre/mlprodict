"""
.. _l-example-onnx-benchmark:

Measure ONNX runtime performances
=================================

The following example shows how to use the
command line to compare one or two runtimes
with :epkg:`scikit-learn`.
It relies on function :func:`validate_runtime
<mlprodict.cli.validate_runtime>` which can be called
from *python* or through a command line
described in page :ref:`l-CMD2`.

.. contents::
    :local:

Run the benchmark
+++++++++++++++++

The following line creates a folder used to dump
information about models which failed during the benchmark.
"""
import os
if not os.path.exists("dump_errors"):
    os.mkdir("dump_errors")

###############################################
# The benchmark can be run with a python instruction
# or a command line:
#
# ::
#
#   python -m mlprodict validate_runtime -v 1 --out_raw data.csv --out_summary summary.csv
#              -b 1 --dump_folder dump_errors --runtime python,onnxruntime1
#              --models LinearRegression,DecisionTreeRegressor
#              --n_features 4,10 --out_graph bench_png
#              -t "{\"1\":{\"number\":10,\"repeat\":10},\"10\":{\"number\":5,\"repeat\":5}}"
#
# We use the python instruction in this example.
#
from mlprodict.cli import validate_runtime

validate_runtime(
    verbose=1,
    out_raw="data.csv", out_summary="summary.csv",
    benchmark=True, dump_folder="dump_errors",
    runtime=['python', 'onnxruntime1'],
    models=['LinearRegression', 'DecisionTreeRegressor'],
    n_features=[4, 10], dtype="32",
    out_graph="bench.png",
    time_kwargs={
        1: {"number": 100, "repeat": 100},
        10: {"number": 50, "repeat": 50},
        100: {"number": 40, "repeat": 50},
        1000: {"number": 40, "repeat": 40},
        10000: {"number": 20, "repeat": 20},
    }
)

########################################
# Let's show the results.
import pandas
df = pandas.read_csv("summary.csv")
print(df.head(n=2).T)

################################
# Let's display the graph generated by the function.

import matplotlib.pyplot as plt
import matplotlib.image as mpimg

img = mpimg.imread('bench.png')
fig = plt.imshow(img)
fig.axes.get_xaxis().set_visible(False)
fig.axes.get_yaxis().set_visible(False)
plt.show()
