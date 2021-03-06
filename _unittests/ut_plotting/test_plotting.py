# -*- coding: utf-8 -*-
"""
@brief      test log(time=2s)
"""
import os
import unittest
from pyquickhelper.pycode import ExtTestCase, get_temp_folder
from mlprodict.plotting.plotting import plot_benchmark_metrics


class TestPlotBenchScatter(ExtTestCase):

    def test_plot_logreg_xtime(self):
        from matplotlib import pyplot as plt
        temp = get_temp_folder(__file__, "temp_plot_benchmark_metrics")
        img = os.path.join(temp, "plot_bench.png")

        data = {(1, 1): 0.1, (10, 1): 1, (1, 10): 2,
                (10, 10): 100, (100, 1): 100, (100, 10): 1000}
        fig, ax = plt.subplots(1, 2, figsize=(10, 4))
        plot_benchmark_metrics(data, ax=ax[0], cbar_kw={'shrink': 0.6})
        plot_benchmark_metrics(data, ax=ax[1], transpose=True, xlabel='X', ylabel='Y',
                               cbarlabel="ratio")
        # fig = ax[0].get_figure()
        fig.savefig(img)
        if __name__ == "__main__":
            plt.show()
        plt.close('all')
        self.assertExists(img)


if __name__ == "__main__":
    unittest.main()
