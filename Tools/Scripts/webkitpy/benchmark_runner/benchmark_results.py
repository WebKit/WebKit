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

import json
import math
import re
import sys

from webkitpy.common.iteration_compatibility import iteritems


class BenchmarkResults(object):

    aggregators = {
        'Total': (lambda values: sum(values)),
        'Arithmetic': (lambda values: sum(values) // len(values)),
        'Geometric': (lambda values: math.exp(sum(map(math.log, values)) / len(values))),
    }
    metric_to_unit = {
        'FrameRate': 'fps',
        'Runs': '/s',
        'Time': 'ms',
        'Duration': 'ms',
        'Malloc': 'B',
        'Heap': 'B',
        'Allocations': 'B',
        'Score': 'pt',
        'Power': 'W',
    }
    SI_prefixes = ['n', 'u', 'm', '', 'K', 'M', 'G', 'T', 'P', 'E']

    def __init__(self, results):
        self._lint_results(results)
        self._results = self._aggregate_results(results)

    def format(self, scale_unit=True, show_iteration_values=False, max_depth=sys.maxsize):
        return self._format_tests(self._results, scale_unit, show_iteration_values, max_depth)

    @classmethod
    def _format_tests(cls, tests, scale_unit, show_iteration_values, max_depth, indent=''):
        output = ''
        config_name = 'current'
        for test_name in sorted(tests.keys()):
            is_first = True
            test = tests[test_name]
            metrics = test.get('metrics', {})
            for metric_name in sorted(metrics.keys()):
                metric = metrics[metric_name]
                for aggregator_name in sorted(metric.keys()):
                    output += indent
                    if is_first:
                        output += test_name
                        is_first = False
                    else:
                        output += ' ' * len(test_name)
                    output += ':' + metric_name + ':'
                    if aggregator_name:
                        output += aggregator_name + ':'
                    output += ' ' + cls._format_values(metric_name, metric[aggregator_name][config_name], scale_unit, show_iteration_values) + '\n'
            if 'tests' in test and max_depth > 1:
                output += cls._format_tests(test['tests'], scale_unit, show_iteration_values, max_depth - 1, indent=(indent + ' ' * len(test_name)))
        return output

    @classmethod
    def _format_values(cls, metric_name, values, scale_unit=True, show_iteration_values=False):
        values = list(map(float, values))
        total = sum(values)
        mean = total / len(values)
        square_sum = sum([x * x for x in values])
        sample_count = len(values)

        # With sum and sum of squares, we can compute the sample standard deviation in O(1).
        # See https://rniwa.com/2012-11-10/sample-standard-deviation-in-terms-of-sum-and-square-sum-of-samples/
        if sample_count <= 1:
            sample_stdev = 0
        else:
            # Be careful about round-off error when sample_stdev is 0.
            sample_stdev = math.sqrt(max(0, square_sum / (sample_count - 1) - total * total / (sample_count - 1) / sample_count))

        unit = cls._unit_from_metric(metric_name)

        if not scale_unit:
            formatted_value = '{mean:.3f}{unit} stdev={delta:.1%}'.format(mean=mean, delta=sample_stdev / mean, unit=unit)
            if show_iteration_values:
                formatted_value += ' [' + ', '.join(['{value:.3f}'.format(value=value) for value in values]) + ']'
            return formatted_value

        if unit == 'ms':
            unit = 's'
            mean = float(mean) / 1000
            values = list([float(value) / 1000 for value in values])
            sample_stdev /= 1000

        base = 1024 if unit == 'B' else 1000
        value_sig_fig = 1 - math.floor(math.log10(sample_stdev / mean)) if sample_stdev else 3
        SI_magnitude = math.floor(math.log(mean, base))

        scaling_factor = math.pow(base, -SI_magnitude)
        scaled_mean = mean * scaling_factor
        SI_prefix = cls.SI_prefixes[int(SI_magnitude) + 3]

        non_floating_digits = 1 + math.floor(math.log10(scaled_mean))
        floating_points_count = max(0, value_sig_fig - non_floating_digits)

        def format_scaled(value):
            return ('{value:.' + str(int(floating_points_count)) + 'f}').format(value=value)

        formatted_value = '{mean}{prefix}{unit} stdev={delta:.1%}'.format(mean=format_scaled(scaled_mean), delta=sample_stdev / mean, prefix=SI_prefix, unit=unit)
        if show_iteration_values:
            formatted_value += ' [' + ', '.join([format_scaled(value * scaling_factor) for value in values]) + ']'
        return formatted_value

    @classmethod
    def _unit_from_metric(cls, metric_name):
        # FIXME: Detect unknown mettric names
        suffix = re.match(r'.*?([A-z][a-z]+|FrameRate)$', metric_name)
        return cls.metric_to_unit[suffix.group(1)]

    @classmethod
    def _aggregate_results(cls, tests):
        results = {}
        for test_name, test in iteritems(tests):
            results[test_name] = cls._aggregate_results_for_test(test)
        return results

    @classmethod
    def _aggregate_results_for_test(cls, test):
        subtest_results = cls._aggregate_results(test['tests']) if 'tests' in test else {}
        results = {}
        for metric_name, metric in iteritems(test.get('metrics', {})):
            if not isinstance(metric, list):
                results[metric_name] = {None: {}}
                for config_name, values in iteritems(metric):
                    results[metric_name][None][config_name] = cls._flatten_list(values)
                continue

            # Filter duplicate aggregators that could have arisen from merging JSONs.
            aggregator_list = list(set(metric))
            results[metric_name] = {}
            for aggregator in aggregator_list:
                values_by_config_iteration = cls._subtest_values_by_config_iteration(subtest_results, metric_name, aggregator)
                for config_name, values_by_iteration in iteritems(values_by_config_iteration):
                    results[metric_name].setdefault(aggregator, {})
                    results[metric_name][aggregator][config_name] = [cls._aggregate_values(aggregator, values) for values in values_by_iteration]

        return {'metrics': results, 'tests': subtest_results}

    @classmethod
    def _flatten_list(cls, nested_list):
        flattened_list = []
        for item in nested_list:
            if isinstance(item, list):
                flattened_list += cls._flatten_list(item)
            else:
                flattened_list.append(item)
        return flattened_list

    @classmethod
    def _subtest_values_by_config_iteration(cls, subtest_results, metric_name, aggregator):
        values_by_config_iteration = {}
        for subtest_name, subtest in iteritems(subtest_results):
            results_for_metric = subtest['metrics'].get(metric_name, {})
            if aggregator in results_for_metric:
                results_for_aggregator = results_for_metric.get(aggregator)
            elif None in results_for_metric:
                results_for_aggregator = results_for_metric.get(None)
            elif len(list(results_for_metric.keys())) == 1:
                results_for_aggregator = results_for_metric.get(list(results_for_metric.keys())[0])
            else:
                results_for_aggregator = {}
            for config_name, values in iteritems(results_for_aggregator):
                values_by_config_iteration.setdefault(config_name, [[] for _ in values])
                for iteration, value in enumerate(values):
                    values_by_config_iteration[config_name][iteration].append(value)
        return values_by_config_iteration

    @classmethod
    def _aggregate_values(cls, aggregator, values):
        return cls.aggregators[aggregator](values)

    @classmethod
    def _lint_results(cls, tests):
        cls._lint_subtest_results(tests, None, None)
        return True

    @classmethod
    def _lint_subtest_results(cls, subtests, parent_test, parent_aggregator_list):
        iteration_groups_by_config = {}
        for test_name, test in iteritems(subtests):
            aggregator_list = None

            if 'metrics' not in test and 'tests' not in test:
                raise TypeError('"%s" does not contain metrics or tests' % test_name)

            if 'metrics' in test:
                metrics = test['metrics']
                if not isinstance(metrics, dict):
                    raise TypeError('The metrics in "%s" is not a dictionary' % test_name)
                for metric_name, metric in iteritems(metrics):
                    if isinstance(metric, list):
                        # Filter duplicate aggregators that could have arisen from merging JSONs.
                        aggregator_list = list(set(metric))
                        cls._lint_aggregator_list(test_name, metric_name, aggregator_list, parent_test, parent_aggregator_list)
                    elif isinstance(metric, dict):
                        cls._lint_configuration(test_name, metric_name, metric, parent_test, parent_aggregator_list, iteration_groups_by_config)
                    else:
                        raise TypeError('"%s" metric of "%s" was not an aggregator list or a dictionary of configurations: %s' % (metric_name, test_name, str(metric)))

            if 'tests' in test:
                cls._lint_subtest_results(test['tests'], test_name, aggregator_list)
            elif aggregator_list:
                raise TypeError('"%s" requires aggregation but it has no subtests' % (test_name))
        return iteration_groups_by_config

    @classmethod
    def _lint_aggregator_list(cls, test_name, metric_name, aggregator_list, parent_test, parent_aggregator_list):
        if len(aggregator_list) != len(set(aggregator_list)):
            raise TypeError('"%s" metric of "%s" had invalid aggregator list: %s' % (metric_name, test_name, json.dumps(aggregator_list)))
        if not aggregator_list:
            raise TypeError('The aggregator list is empty in "%s" metric of "%s"' % (metric_name, test_name))
        for aggregator_name in aggregator_list:
            if cls._is_numeric(aggregator_name):
                raise TypeError('"%s" metric of "%s" is not wrapped by a configuration; e.g. "current"' % (metric_name, test_name))
            if aggregator_name not in cls.aggregators:
                raise TypeError('"%s" metric of "%s" uses unknown aggregator: %s' % (metric_name, test_name, aggregator_name))
        if not parent_aggregator_list:
            return
        for parent_aggregator in parent_aggregator_list:
            if parent_aggregator not in aggregator_list and len(aggregator_list) > 1:
                raise TypeError('"%s" metric of "%s" has no value to aggregate as "%s" in a subtest "%s"' % (
                    metric_name, parent_test, parent_aggregator, test_name))

    @classmethod
    def _lint_configuration(cls, test_name, metric_name, configurations, parent_test, parent_aggregator_list, iteration_groups_by_config):
        # FIXME: Check that config_name is always "current".
        for config_name, values in iteritems(configurations):
            nested_list_count = [isinstance(value, list) for value in values].count(True)
            if nested_list_count not in [0, len(values)]:
                raise TypeError('"%s" metric of "%s" had malformed values: %s' % (metric_name, test_name, json.dumps(values)))

            if nested_list_count:
                value_shape = []
                for value_group in values:
                    value_shape.append(len(value_group))
                    cls._lint_values(test_name, metric_name, value_group)
            else:
                value_shape = len(values)
                cls._lint_values(test_name, metric_name, values)

            iteration_groups_by_config.setdefault(metric_name, {}).setdefault(config_name, value_shape)
            if parent_aggregator_list and value_shape != iteration_groups_by_config[metric_name][config_name]:
                raise TypeError('"%s" metric of "%s" had a mismatching subtest values' % (metric_name, parent_test))

    @classmethod
    def _lint_values(cls, test_name, metric_name, values):
        if any([not cls._is_numeric(value) for value in values]):
            raise TypeError('"%s" metric of "%s" contains non-numeric value: %s' % (metric_name, test_name, json.dumps(values)))

    @classmethod
    def _is_numeric(cls, value):
        return isinstance(value, int) or isinstance(value, float)
