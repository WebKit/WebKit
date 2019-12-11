# Copyright (C) 2015 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from benchmark_results import BenchmarkResults


class BenchmarkResultsTest(unittest.TestCase):
    def test_init(self):
        results = BenchmarkResults({'SomeTest': {'metrics': {'Time': {'current': [1, 2, 3]}}}})
        self.assertEqual(results._results, {'SomeTest': {'metrics': {'Time': {None: {'current': [1, 2, 3]}}}, 'tests': {}}})

        with self.assertRaisesRegexp(TypeError, r'"Time" metric of "SomeTest" contains non-numeric value: \[1, 2, "a"\]'):
            BenchmarkResults({'SomeTest': {'metrics': {'Time': {'current': [1, 2, 'a']}}}})

    def test_format(self):
        result = BenchmarkResults({'SomeTest': {'metrics': {'Time': {'current': [1, 2, 3]}}}})
        self.assertEqual(result.format(), 'SomeTest:Time: 2.0ms stdev=50.0%\n')

        result = BenchmarkResults({'SomeTest': {'metrics': {'Time': {'current': [1, 2, 3]}, 'Score': {'current': [2, 3, 4]}}}})
        self.assertEqual(result.format(), '''
SomeTest:Score: 3.0pt stdev=33.3%
        :Time: 2.0ms stdev=50.0%
'''[1:])

        result = BenchmarkResults({'SomeTest': {
            'metrics': {'Time': ['Total', 'Arithmetic']},
            'tests': {
                'SubTest1': {'metrics': {'Time': {'current': [1, 2, 3]}}},
                'SubTest2': {'metrics': {'Time': {'current': [4, 5, 6]}}}}}})
        self.assertEqual(result.format(), '''
SomeTest:Time:Arithmetic: 3.0ms stdev=33.3%
        :Time:Total: 7.0ms stdev=28.6%
        SubTest1:Time: 2.0ms stdev=50.0%
        SubTest2:Time: 5.0ms stdev=20.0%
'''[1:])

    def test_format_with_depth_limit(self):
        result = BenchmarkResults({'SomeTest': {
            'metrics': {'Time': ['Total', 'Arithmetic']},
            'tests': {
                'SubTest1': {'metrics': {'Time': {'current': [1, 2, 3]}}},
                'SubTest2': {'metrics': {'Time': {'current': [4, 5, 6]}}}}}})
        self.assertEqual(result.format(max_depth=1), '''
SomeTest:Time:Arithmetic: 3.0ms stdev=33.3%
        :Time:Total: 7.0ms stdev=28.6%
'''[1:])

    def test_format_values_with_large_error(self):
        self.assertEqual(BenchmarkResults._format_values('Runs', [1, 2, 3]), '2.0/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [10, 20, 30]), '20/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [100, 200, 300]), '200/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [1000, 2000, 3000]), '2.0K/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [10000, 20000, 30000]), '20K/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [100000, 200000, 300000]), '200K/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [1000000, 2000000, 3000000]), '2.0M/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.1, 0.2, 0.3]), '200m/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.01, 0.02, 0.03]), '20m/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.001, 0.002, 0.003]), '2.0m/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.0001, 0.0002, 0.0003]), '200u/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.00001, 0.00002, 0.00003]), '20u/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.000001, 0.000002, 0.000003]), '2.0u/s stdev=50.0%')

    def test_format_values_with_small_error(self):
        self.assertEqual(BenchmarkResults._format_values('Runs', [1.1, 1.2, 1.3]), '1.20/s stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [11, 12, 13]), '12.0/s stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [110, 120, 130]), '120/s stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [1100, 1200, 1300]), '1.20K/s stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [11000, 12000, 13000]), '12.0K/s stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [110000, 120000, 130000]), '120K/s stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [1100000, 1200000, 1300000]), '1.20M/s stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.11, 0.12, 0.13]), '120m/s stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.011, 0.012, 0.013]), '12.0m/s stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.0011, 0.0012, 0.0013]), '1.20m/s stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.00011, 0.00012, 0.00013]), '120u/s stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.000011, 0.000012, 0.000013]), '12.0u/s stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.0000011, 0.0000012, 0.0000013]), '1.20u/s stdev=8.3%')

    def test_format_values_with_time(self):
        self.assertEqual(BenchmarkResults._format_values('Time', [1, 2, 3]), '2.0ms stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Time', [10, 20, 30]), '20ms stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Time', [100, 200, 300]), '200ms stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Time', [1000, 2000, 3000]), '2.0s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Time', [10000, 20000, 30000]), '20s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Time', [100000, 200000, 300000]), '200s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Time', [0.11, 0.12, 0.13]), '120us stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Time', [0.011, 0.012, 0.013]), '12.0us stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Time', [0.0011, 0.0012, 0.0013]), '1.20us stdev=8.3%')
        self.assertEqual(BenchmarkResults._format_values('Time', [0.00011, 0.00012, 0.00013]), '120ns stdev=8.3%')

    def test_format_values_with_no_unit_scaling(self):
        self.assertEqual(BenchmarkResults._format_values('Runs', [1, 2, 3], scale_unit=False), '2.000/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [10, 20, 30], scale_unit=False), '20.000/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [100, 200, 300], scale_unit=False), '200.000/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [1000, 2000, 3000], scale_unit=False), '2000.000/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [10000, 20000, 30000], scale_unit=False), '20000.000/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [100000, 200000, 300000], scale_unit=False), '200000.000/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [1000000, 2000000, 3000000], scale_unit=False), '2000000.000/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.1, 0.2, 0.3], scale_unit=False), '0.200/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.01, 0.02, 0.03], scale_unit=False), '0.020/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.001, 0.002, 0.003], scale_unit=False), '0.002/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.0001, 0.0002, 0.0003], scale_unit=False), '0.000/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.00001, 0.00002, 0.00003], scale_unit=False), '0.000/s stdev=50.0%')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.000001, 0.000002, 0.000003], scale_unit=False), '0.000/s stdev=50.0%')

    def test_format_values_with_iteration_values(self):
        self.assertEqual(BenchmarkResults._format_values('Time', [1, 2, 3], show_iteration_values=True), '2.0ms stdev=50.0% [1.0, 2.0, 3.0]')
        self.assertEqual(BenchmarkResults._format_values('Time', [10, 20, 30], show_iteration_values=True), '20ms stdev=50.0% [10, 20, 30]')
        self.assertEqual(BenchmarkResults._format_values('Time', [100, 200, 300], show_iteration_values=True), '200ms stdev=50.0% [100, 200, 300]')
        self.assertEqual(BenchmarkResults._format_values('Time', [1000, 2000, 3000], show_iteration_values=True), '2.0s stdev=50.0% [1.0, 2.0, 3.0]')
        self.assertEqual(BenchmarkResults._format_values('Time', [10000, 20000, 30000], show_iteration_values=True), '20s stdev=50.0% [10, 20, 30]')
        self.assertEqual(BenchmarkResults._format_values('Time', [100000, 200000, 300000], show_iteration_values=True), '200s stdev=50.0% [100, 200, 300]')
        self.assertEqual(BenchmarkResults._format_values('Time', [0.11, 0.12, 0.13], show_iteration_values=True), '120us stdev=8.3% [110, 120, 130]')
        self.assertEqual(BenchmarkResults._format_values('Time', [0.011, 0.012, 0.013], show_iteration_values=True), '12.0us stdev=8.3% [11.0, 12.0, 13.0]')
        self.assertEqual(BenchmarkResults._format_values('Time', [0.0011, 0.0012, 0.0013], show_iteration_values=True), '1.20us stdev=8.3% [1.10, 1.20, 1.30]')
        self.assertEqual(BenchmarkResults._format_values('Time', [0.00011, 0.00012, 0.00013], show_iteration_values=True), '120ns stdev=8.3% [110, 120, 130]')

    def test_format_values_with_no_unit_scaling_and_iteration_values(self):
        self.assertEqual(BenchmarkResults._format_values('Runs', [1, 2, 3], scale_unit=False, show_iteration_values=True),
            '2.000/s stdev=50.0% [1.000, 2.000, 3.000]')
        self.assertEqual(BenchmarkResults._format_values('Runs', [100, 200, 300], scale_unit=False, show_iteration_values=True),
            '200.000/s stdev=50.0% [100.000, 200.000, 300.000]')
        self.assertEqual(BenchmarkResults._format_values('Runs', [1000, 2000, 3000], scale_unit=False, show_iteration_values=True),
            '2000.000/s stdev=50.0% [1000.000, 2000.000, 3000.000]')
        self.assertEqual(BenchmarkResults._format_values('Runs', [1000000, 2000000, 3000000], scale_unit=False, show_iteration_values=True),
            '2000000.000/s stdev=50.0% [1000000.000, 2000000.000, 3000000.000]')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.1, 0.2, 0.3], scale_unit=False, show_iteration_values=True),
            '0.200/s stdev=50.0% [0.100, 0.200, 0.300]')
        self.assertEqual(BenchmarkResults._format_values('Runs', [0.0001, 0.0002, 0.0003], scale_unit=False, show_iteration_values=True),
            '0.000/s stdev=50.0% [0.000, 0.000, 0.000]')

    def test_format_values_with_no_error(self):
        self.assertEqual(BenchmarkResults._format_values('Time', [1, 1, 1]), '1.00ms stdev=0.0%')

    def test_format_values_with_small_difference(self):
        self.assertEqual(BenchmarkResults._format_values('Time', [5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4]),
            '4.05ms stdev=5.5%')

    def test_aggregate_results(self):
        self.maxDiff = None
        self.assertEqual(BenchmarkResults._aggregate_results(
            {'SomeTest': {'metrics': {'Time': {'current': [1, 2, 3]}}}}),
            {'SomeTest': {'metrics': {'Time': {None: {'current': [1, 2, 3]}}}, 'tests': {}}})

        self.assertEqual(BenchmarkResults._aggregate_results(
            {'SomeTest': {
                'metrics': {'Time': ['Total']},
                'tests': {
                    'SubTest1': {'metrics': {'Time': {'current': [1, 2, 3]}}},
                    'SubTest2': {'metrics': {'Time': {'current': [4, 5, 6]}}}}}}),
            {'SomeTest': {
                'metrics': {'Time': {'Total': {'current': [5, 7, 9]}}},
                'tests': {
                    'SubTest1': {'metrics': {'Time': {None: {'current': [1, 2, 3]}}}, 'tests': {}},
                    'SubTest2': {'metrics': {'Time': {None: {'current': [4, 5, 6]}}}, 'tests': {}}}}})

        self.assertEqual(BenchmarkResults._aggregate_results(
            {'SomeTest': {
                'metrics': {'Time': ['Total'], 'Runs': ['Total']},
                'tests': {
                    'SubTest1': {'metrics': {'Time': {'current': [1, 2, 3]}}},
                    'SubTest2': {'metrics': {'Time': {'current': [4, 5, 6]}}},
                    'SubTest3': {'metrics': {'Runs': {'current': [7, 8, 9]}}}}}}),
            {'SomeTest': {
                'metrics': {
                    'Time': {'Total': {'current': [5, 7, 9]}},
                    'Runs': {'Total': {'current': [7, 8, 9]}}},
                'tests': {
                    'SubTest1': {'metrics': {'Time': {None: {'current': [1, 2, 3]}}}, 'tests': {}},
                    'SubTest2': {'metrics': {'Time': {None: {'current': [4, 5, 6]}}}, 'tests': {}},
                    'SubTest3': {'metrics': {'Runs': {None: {'current': [7, 8, 9]}}}, 'tests': {}}}}})

    def test_aggregate_results_with_gropus(self):
        self.maxDiff = None
        self.assertEqual(BenchmarkResults._aggregate_results(
            {'SomeTest': {
                'metrics': {'Time': ['Total']},
                'tests': {
                    'SubTest1': {'metrics': {'Time': {'current': [[1, 2], [3, 4]]}}},
                    'SubTest2': {'metrics': {'Time': {'current': [[5, 6], [7, 8]]}}}}}}),
            {'SomeTest': {
                'metrics': {'Time': {'Total': {'current': [6, 8, 10, 12]}}},
                'tests': {
                    'SubTest1': {'metrics': {'Time': {None: {'current': [1, 2, 3, 4]}}}, 'tests': {}},
                    'SubTest2': {'metrics': {'Time': {None: {'current': [5, 6, 7, 8]}}}, 'tests': {}}}}})

    def test_aggregate_nested_results(self):
        self.maxDiff = None
        self.assertEqual(BenchmarkResults._aggregate_results(
            {'SomeTest': {
                'metrics': {'Time': ['Total']},
                'tests': {
                    'SubTest1': {
                        'metrics': {'Time': ['Total']},
                        'tests': {
                            'GrandChild1': {'metrics': {'Time': {'current': [1, 2]}}},
                            'GrandChild2': {'metrics': {'Time': {'current': [3, 4]}}}}},
                    'SubTest2': {'metrics': {'Time': {'current': [5, 6]}}}}}}),
            {'SomeTest': {
                'metrics': {'Time': {'Total': {'current': [9, 12]}}},
                'tests': {
                    'SubTest1': {
                        'metrics': {'Time': {'Total': {'current': [4, 6]}}},
                        'tests': {
                            'GrandChild1': {'metrics': {'Time': {None: {'current': [1, 2]}}}, 'tests': {}},
                            'GrandChild2': {'metrics': {'Time': {None: {'current': [3, 4]}}}, 'tests': {}}}},
                    'SubTest2': {'metrics': {'Time': {None: {'current': [5, 6]}}}, 'tests': {}}}}})

        self.assertEqual(BenchmarkResults._aggregate_results(
            {'SomeTest': {
                'metrics': {'Time': ['Total']},
                'tests': {
                    'SubTest1': {
                        'metrics': {'Time': ['Total', 'Arithmetic']},
                        'tests': {
                            'GrandChild1': {'metrics': {'Time': {'current': [1, 2]}}},
                            'GrandChild2': {'metrics': {'Time': {'current': [3, 4]}}}}},
                    'SubTest2': {'metrics': {'Time': {'current': [5, 6]}}}}}}),
            {'SomeTest': {
                'metrics': {'Time': {'Total': {'current': [9, 12]}}},
                'tests': {
                    'SubTest1': {
                        'metrics': {'Time': {'Total': {'current': [4, 6]}, 'Arithmetic': {'current': [2, 3]}}},
                        'tests': {
                            'GrandChild1': {'metrics': {'Time': {None: {'current': [1, 2]}}}, 'tests': {}},
                            'GrandChild2': {'metrics': {'Time': {None: {'current': [3, 4]}}}, 'tests': {}}}},
                    'SubTest2': {'metrics': {'Time': {None: {'current': [5, 6]}}}, 'tests': {}}}}})

    def test_aggregate_results_from_another_aggregator(self):
        self.maxDiff = None
        self.assertEqual(BenchmarkResults._aggregate_results(
            {'SomeTest': {
                'metrics': {'Time': ['Arithmetic', 'Geometric']},
                'tests': {
                    'SubTest1': {
                        'metrics': {'Time': ['Total']},
                        'tests': {
                            'GrandChild1': {'metrics': {'Time': {'current': [1, 2]}}},
                            'GrandChild2': {'metrics': {'Time': {'current': [3, 4]}}}}},
                    'SubTest2': {'metrics': {'Time': {'current': [9, 24]}}}}}}),
            {'SomeTest': {
                'metrics': {'Time': {'Geometric': {'current': [6.0, 12.0]}, 'Arithmetic': {'current': [6, 15]}}},
                'tests': {
                    'SubTest1': {
                        'metrics': {'Time': {'Total': {'current': [4, 6]}}},
                        'tests': {
                            'GrandChild1': {'metrics': {'Time': {None: {'current': [1, 2]}}}, 'tests': {}},
                            'GrandChild2': {'metrics': {'Time': {None: {'current': [3, 4]}}}, 'tests': {}}}},
                    'SubTest2': {'metrics': {'Time': {None: {'current': [9, 24]}}}, 'tests': {}}}}})

    def test_lint_results(self):
        with self.assertRaisesRegexp(TypeError, r'"SomeTest" does not contain metrics or tests'):
            BenchmarkResults._lint_results({'SomeTest': {}})

        with self.assertRaisesRegexp(TypeError, r'The metrics in "SomeTest" is not a dictionary'):
            BenchmarkResults._lint_results({'SomeTest': {'metrics': []}})

        with self.assertRaisesRegexp(TypeError, r'The aggregator list is empty in "Time" metric of "SomeTest"'):
            BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': []}}})

        with self.assertRaisesRegexp(TypeError, r'"Time" metric of "SomeTest" is not wrapped by a configuration; e.g. "current"'):
            BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': [1, 2]}}})

        self.assertTrue(BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': {'current': [1, 2]}}}}))

        with self.assertRaisesRegexp(TypeError, r'"Time" metric of "SomeTest" was not an aggregator list or a dictionary of configurations: 1'):
            BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': 1}}})

        with self.assertRaisesRegexp(TypeError, r'"Time" metric of "SomeTest" contains non-numeric value: \["Total"\]'):
            BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': {'current': ['Total']}}}})

        with self.assertRaisesRegexp(TypeError, r'"Time" metric of "SomeTest" contains non-numeric value: \["Total", "Geometric"\]'):
            BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': {'current': [['Total', 'Geometric']]}}}})

        with self.assertRaisesRegexp(TypeError, r'"SomeTest" requires aggregation but it has no subtests'):
            BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': ['Total']}}})

        with self.assertRaisesRegexp(TypeError, r'"OtherTest" requires aggregation but it has no subtests'):
            BenchmarkResults._lint_results({'OtherTest': {'metrics': {'Time': ['Total']}}})

        with self.assertRaisesRegexp(TypeError, r'"Time" metric of "SomeTest" had invalid aggregator list: \["Total", "Total"\]'):
            BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': ['Total', 'Total']}, 'tests': {
                'SubTest1': {'metrics': {'Time': {'current': []}}}}}})

        with self.assertRaisesRegexp(TypeError, r'"Time" metric of "SomeTest" uses unknown aggregator: KittenMean'):
            BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': ['KittenMean']}, 'tests': {
                'SubTest1': {'metrics': {'Time': {'current': []}}}}}})

        with self.assertRaisesRegexp(TypeError, r'"Time" metric of "SomeTest" had a mismatching subtest values'):
            BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': ['Total']}, 'tests': {
                'SubTest1': {'metrics': {'Time': {'current': [1, 2, 3]}}},
                'SubTest2': {'metrics': {'Time': {'current': [4, 5, 6, 7]}}}}}})

        with self.assertRaisesRegexp(TypeError, r'"Time" metric of "SomeTest" had a mismatching subtest values'):
            BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': ['Total']}, 'tests': {
                'SubTest1': {'metrics': {'Time': {'current': [[1, 2], [3]]}}},
                'SubTest2': {'metrics': {'Time': {'current': [[4, 5], [6, 7]]}}}}}})

        with self.assertRaisesRegexp(TypeError, r'"Time" metric of "SomeTest" had malformed values: \[1, \[2\], 3\]'):
            BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': {'current': [1, [2], 3]}}}})

        with self.assertRaisesRegexp(TypeError, r'"Time" metric of "SomeTest" has no value to aggregate as "Arithmetic" in a subtest "SubTest1"'):
            BenchmarkResults._lint_results({'SomeTest': {
                'metrics': {'Time': ['Arithmetic']},
                'tests': {
                    'SubTest1': {
                        'metrics': {'Time': ['Total', 'Geometric']},
                        'tests': {
                            'GrandChild1': {'metrics': {'Time': {'current': [1, 2]}}},
                            'GrandChild2': {'metrics': {'Time': {'current': [3, 4]}}}}},
                    'SubTest2': {'metrics': {'Time': {'current': [9, 24]}}}}}})

        self.assertTrue(BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': ['Total']}, 'tests': {
            'SubTest1': {'metrics': {'Time': {'current': [1, 2, 3]}}},
            'SubTest2': {'metrics': {'Time': {'current': [4, 5, 6], 'baseline': [7]}}}}}}))

        self.assertTrue(BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': ['Total']}, 'tests': {
            'SubTest1': {'metrics': {'Time': {'current': [1, 2, 3]}}},
            'SubTest2': {'metrics': {'Runs': {'current': [4, 5, 6, 7]}}}}}}))

        self.assertTrue(BenchmarkResults._lint_results({'SomeTest': {'metrics': {'Time': ['Total']}, 'tests': {
            'SubTest1': {'metrics': {'Time': {'current': [[1, 2], [3, 4]]}}},
            'SubTest2': {'metrics': {'Time': {'current': [[5, 6], [7, 8]]}}}}}}))
