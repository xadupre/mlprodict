"""
@file
@brief Command line about validation of prediction runtime.
"""
import os
from logging import getLogger
import warnings
import json
from multiprocessing import Pool
from pandas import DataFrame
from sklearn.exceptions import ConvergenceWarning


def validate_runtime(verbose=1, opset_min=9, opset_max="",
                     check_runtime=True, runtime='python', debug=False,
                     models=None, out_raw="model_onnx_raw.xlsx",
                     out_summary="model_onnx_summary.xlsx",
                     dump_folder=None, dump_all=False, benchmark=False,
                     catch_warnings=True, assume_finite=True,
                     versions=False, skip_models=None,
                     extended_list=True, separate_process=False,
                     time_kwargs=None, n_features=None, fLOG=print,
                     out_graph=None, force_return=False,
                     dtype=None, skip_long_test=False):
    """
    Walks through most of :epkg:`scikit-learn` operators
    or model or predictor or transformer, tries to convert
    them into :epkg:`ONNX` and computes the predictions
    with a specific runtime.

    :param verbose: integer from 0 (None) to 2 (full verbose)
    :param opset_min: tries every conversion from this minimum opset
    :param opset_max: tries every conversion up to maximum opset
    :param check_runtime: to check the runtime
        and not only the conversion
    :param runtime: runtime to check, python,
        onnxruntime1 to check :epkg:`onnxruntime`,
        onnxruntime2 to check every ONNX node independently
        with onnxruntime, many runtime can be checked at the same time
        if the value is a comma separated list
    :param models: comma separated list of models to test or empty
        string to test them all
    :param skip_models: models to skip
    :param debug: stops whenever an exception is raised,
        only if *separate_process* is False
    :param out_raw: output raw results into this file (excel format)
    :param out_summary: output an aggregated view into this file (excel format)
    :param dump_folder: folder where to dump information (pickle)
        in case of mismatch
    :param dump_all: dumps all models, not only the failing ones
    :param benchmark: run benchmark
    :param catch_warnings: catch warnings
    :param assume_finite: See `config_context
        <https://scikit-learn.org/stable/modules/generated/sklearn.config_context.html>`_,
        If True, validation for finiteness will be skipped, saving time, but leading
        to potential crashes. If False, validation for finiteness will be performed,
        avoiding error.
    :param versions: add columns with versions of used packages,
        :epkg:`numpy`, :epkg:`scikit-learn`, :epkg:`onnx`, :epkg:`onnxruntime`,
        :epkg:`sklearn-onnx`
    :param extended_list: extends the list of :epkg:`scikit-learn` converters
        with converters implemented in this module
    :param separate_process: run every model in a separate process,
        this option must be used to run all model in one row
        even if one of them is crashing
    :param time_kwargs: a dictionary which defines the number of rows and
        the parameter *number* and *repeat* when benchmarking a model,
        the value must follow :epkg:`json` format
    :param n_features: change the default number of features for
        a specific problem, it can also be a comma separated list
    :param force_return: forces the function to return the results,
        used when the results are produces through a separate process
    :param out_graph: image name, to output a graph which summarizes
        a benchmark in case it was run
    :param dtype: '32' or '64' or None for both,
        limits the test to one specific number types
    :param skip_long_test: skips tests for high values of N if
        they seem too long
    :param fLOG: logging function

    .. cmdref::
        :title: Validate a runtime against scikit-learn
        :cmd: -m mlprodict validate_runtime --help
        :lid: l-cmd-validate_runtime

        The command walks through all scikit-learn operators,
        tries to convert them, checks the predictions,
        and produces a report.

        Example::

            python -m mlprodict validate_runtime --models LogisticRegression,LinearRegression

        Following example benchmarks models
        :epkg:`sklearn:ensemble:RandomForestRegressor`,
        :epkg:`sklearn:tree:DecisionTreeRegressor`, it compares
        :epkg:`onnxruntime` against :epkg:`scikit-learn` for opset 10.

        ::

            python -m mlprodict validate_runtime -v 1 -o 10 -op 10 -c 1 -r onnxruntime1
                   -m RandomForestRegressor,DecisionTreeRegressor -out bench_onnxruntime.xlsx -b 1

    Parameter ``--time_kwargs`` may be used to reduce or increase
    bencharmak precisions. The following value tells the function
    to run a benchmarks with datasets of 1 or 10 number, to repeat
    a given number of time *number* predictions in one row.
    The total time is divided by :math:`number \\times repeat``.

        -t "{\\"1\\":{\\"number\\":10,\\"repeat\\":10},\\"10\\":{\\"number\\":5,\\"repeat\\":5}}"

    The following example dumps every model in the list:

    ::

        python -m mlprodict validate_runtime --out_raw raw.csv --out_summary sum.csv
               --models LinearRegression,LogisticRegression,DecisionTreeRegressor,DecisionTreeClassifier
               -r python,onnxruntime1 -o 10 -op 10 -v 1 -b 1 -dum 1
               -du model_dump -n 20,100,500 --out_graph benchmark.png --dtype 32

    The command line generates a graph produced by function
    :func:`plot_validate_benchmark
    <mlprodict.onnxrt.validate.validate_graph.plot_validate_benchmark>`.
    """
    if separate_process:
        return _validate_runtime_separate_process(
            verbose=verbose, opset_min=opset_min, opset_max=opset_max,
            check_runtime=check_runtime, runtime=runtime, debug=debug,
            models=models, out_raw=out_raw,
            out_summary=out_summary, dump_all=dump_all,
            dump_folder=dump_folder, benchmark=benchmark,
            catch_warnings=catch_warnings, assume_finite=assume_finite,
            versions=versions, skip_models=skip_models,
            extended_list=extended_list, time_kwargs=time_kwargs,
            n_features=n_features, fLOG=fLOG, force_return=True,
            out_graph=None, dtype=dtype, skip_long_test=skip_long_test)

    from ..onnxrt.validate import enumerate_validated_operator_opsets  # pylint: disable=E0402

    models = None if models in (None, "") else models.strip().split(',')
    skip_models = {} if skip_models in (
        None, "") else skip_models.strip().split(',')
    logger = getLogger('skl2onnx')
    logger.disabled = True
    if not dump_folder:
        dump_folder = None
    if dump_folder and not os.path.exists(dump_folder):
        raise FileNotFoundError("Cannot find dump_folder '{0}'.".format(
            dump_folder))

    # handling parameters
    if opset_max == "":
        opset_max = None
    if isinstance(opset_min, str):
        opset_min = int(opset_min)
    if isinstance(opset_max, str):
        opset_max = int(opset_max)
    if isinstance(verbose, str):
        verbose = int(verbose)
    if isinstance(extended_list, str):
        extended_list = extended_list in ('1', 'True', 'true')
    if time_kwargs in (None, ''):
        time_kwargs = None
    if isinstance(time_kwargs, str):
        time_kwargs = json.loads(time_kwargs)
        # json only allows string as keys
        time_kwargs = {int(k): v for k, v in time_kwargs.items()}
    if time_kwargs is not None and not isinstance(time_kwargs, dict):
        raise ValueError("time_kwargs must be a dictionary not {}\n{}".format(
            type(time_kwargs), time_kwargs))
    if n_features in (None, ""):
        n_features = None
    elif ',' in n_features:
        n_features = list(map(int, n_features.split(',')))
    else:
        n_features = int(n_features)
    if ',' in runtime:
        runtime = runtime.split(',')

    def fct_filter_exp(m, s):
        return str(m) not in skip_models

    if dtype in ('', None):
        fct_filter = fct_filter_exp
    elif dtype == '32':
        def fct_filter_exp2(m, p):
            return fct_filter_exp(m, p) and '64' not in p
        fct_filter = fct_filter_exp2
    elif dtype == '64':
        def fct_filter_exp3(m, p):
            return fct_filter_exp(m, p) and '64' in p
        fct_filter = fct_filter_exp3
    else:
        raise ValueError("dtype must be empty, 32, 64 not '{}'.".format(dtype))

    # body

    def build_rows(models_):
        rows = list(enumerate_validated_operator_opsets(
            verbose, models=models_, fLOG=fLOG, runtime=runtime, debug=debug,
            dump_folder=dump_folder, opset_min=opset_min, opset_max=opset_max,
            benchmark=benchmark, assume_finite=assume_finite, versions=versions,
            extended_list=extended_list, time_kwargs=time_kwargs, dump_all=dump_all,
            n_features=n_features, filter_exp=fct_filter,
            skip_long_test=skip_long_test))
        return rows

    def catch_build_rows(models_):
        if catch_warnings:
            with warnings.catch_warnings():
                warnings.simplefilter("ignore",
                                      (UserWarning, ConvergenceWarning,
                                       RuntimeWarning, FutureWarning))
                rows = build_rows(models_)
        else:
            rows = build_rows(models_)
        return rows

    rows = catch_build_rows(models)
    res = _finalize(rows, out_raw, out_summary,
                    verbose, models, out_graph, fLOG)
    return res if (force_return or verbose >= 2) else None


def _finalize(rows, out_raw, out_summary, verbose, models, out_graph, fLOG):
    from ..onnxrt.validate import summary_report  # pylint: disable=E0402

    # Drops data which cannot be serialized.
    for row in rows:
        keys = []
        for k in row:
            if 'lambda' in k:
                keys.append(k)
        for k in keys:
            del row[k]

    df = DataFrame(rows)
    if os.path.splitext(out_raw)[-1] == ".xlsx":
        df.to_excel(out_raw, index=False)
    else:
        df.to_csv(out_raw, index=False)
    if df.shape[0] == 0:
        raise RuntimeError("No result produced by the benchmark.")
    piv = summary_report(df)
    if 'optim' not in piv:
        raise RuntimeError("Unable to produce a summary. Missing column in \n{}".format(
            piv.columns))
    if os.path.splitext(out_summary)[-1] == ".xlsx":
        piv.to_excel(out_summary, index=False)
    else:
        piv.to_csv(out_summary, index=False)
    if verbose > 0 and models is not None:
        fLOG(piv.T)
    if out_graph is not None:
        if verbose > 0:
            fLOG("Saving graph into '{}'.".format(out_graph))
        from ..onnxrt.validate.validate_graph import plot_validate_benchmark
        fig = plot_validate_benchmark(piv)[0]
        fig.savefig(out_graph)

    return rows


def _validate_runtime_dict(kwargs):
    return validate_runtime(**kwargs)


def _validate_runtime_separate_process(**kwargs):
    models = kwargs['models']
    if models in (None, ""):
        from ..onnxrt.validate.validate_helper import sklearn_operators
        models = [_['name'] for _ in sklearn_operators(extended=True)]
    else:
        models = models.strip().split(',')

    skip_models = kwargs['skip_models']
    skip_models = {} if skip_models in (
        None, "") else skip_models.strip().split(',')

    verbose = kwargs['verbose']
    fLOG = kwargs['fLOG']
    all_rows = []
    skls = [m for m in models if m not in skip_models]
    skls.sort()

    if verbose > 0:
        from tqdm import tqdm
        pbar = tqdm(skls)
    else:
        pbar = skls

    for op in pbar:
        if not isinstance(pbar, list):
            pbar.set_description("[%s]" % (op + " " * (25 - len(op))))

        if kwargs['out_raw']:
            out_raw = os.path.splitext(kwargs['out_raw'])
            out_raw = "".join([out_raw[0], "_", op, out_raw[1]])
        else:
            out_raw = None

        if kwargs['out_summary']:
            out_summary = os.path.splitext(kwargs['out_summary'])
            out_summary = "".join([out_summary[0], "_", op, out_summary[1]])
        else:
            out_summary = None

        new_kwargs = kwargs.copy()
        if 'fLOG' in new_kwargs:
            del new_kwargs['fLOG']
        new_kwargs['out_raw'] = out_raw
        new_kwargs['out_summary'] = out_summary
        new_kwargs['models'] = op
        new_kwargs['verbose'] = 0  # tqdm fails
        new_kwargs['out_graph'] = None

        with Pool(1) as p:
            try:
                result = p.apply_async(_validate_runtime_dict, [new_kwargs])
                lrows = result.get(timeout=150)  # timeout fixed to 150s
                all_rows.extend(lrows)
            except Exception as e:  # pylint: disable=W0703
                all_rows.append({
                    'name': op, 'scenario': 'CRASH',
                    'ERROR-msg': str(e).replace("\n", " -- ")
                })

    return _finalize(all_rows, kwargs['out_raw'], kwargs['out_summary'],
                     verbose, models, kwargs.get('out_graph', None), fLOG)
