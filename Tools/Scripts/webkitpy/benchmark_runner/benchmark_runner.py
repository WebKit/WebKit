#!/usr/bin/env python

import json
import logging
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import types
import os
import urlparse

from benchmark_builder import BenchmarkBuilder
from benchmark_results import BenchmarkResults
from browser_driver.browser_driver_factory import BrowserDriverFactory


_log = logging.getLogger(__name__)


class BenchmarkRunner(object):
    name = 'benchmark_runner'

    def __init__(self, plan_file, local_copy, count_override, build_dir, output_file, platform, browser, browser_path, scale_unit=True, show_iteration_values=False, device_id=None, diagnose_dir=None):
        try:
            plan_file = self._find_plan_file(plan_file)
            with open(plan_file, 'r') as fp:
                self._plan_name = os.path.split(os.path.splitext(plan_file)[0])[1]
                self._plan = json.load(fp)
                if not 'options' in self._plan:
                    self._plan['options'] = {}
                if local_copy:
                    self._plan['local_copy'] = local_copy
                if count_override:
                    self._plan['count'] = count_override
                self._browser_driver = BrowserDriverFactory.create(platform, browser)
                self._browser_path = browser_path
                self._build_dir = os.path.abspath(build_dir) if build_dir else None
                self._diagnose_dir = os.path.abspath(diagnose_dir) if diagnose_dir else None
                self._output_file = output_file
                self._scale_unit = scale_unit
                self._show_iteration_values = show_iteration_values
                self._config = self._plan.get('config', {})
                if device_id:
                    self._config['device_id'] = device_id
        except IOError as error:
            _log.error('Can not open plan file: {plan_file} - Error {error}'.format(plan_file=plan_file, error=error))
            raise error
        except ValueError as error:
            _log.error('Plan file: {plan_file} may not follow JSON format - Error {error}'.format(plan_file=plan_file, error=error))
            raise error

    def _find_plan_file(self, plan_file):
        if not os.path.exists(plan_file):
            abs_path = os.path.join(BenchmarkRunner.plan_directory(), plan_file)
            if os.path.exists(abs_path):
                return abs_path
            if not abs_path.endswith('.plan'):
                abs_path += '.plan'
            if os.path.exists(abs_path):
                return abs_path
        return plan_file

    @staticmethod
    def plan_directory():
        return os.path.join(os.path.dirname(__file__), 'data/plans')

    @staticmethod
    def available_plans():
        plans = [os.path.splitext(plan_file)[0] for plan_file in os.listdir(BenchmarkRunner.plan_directory()) if plan_file.endswith(".plan")]
        return plans

    def _run_one_test(self, web_root, test_file):
        raise NotImplementedError('BenchmarkRunner is an abstract class and shouldn\'t be instantiated.')

    def _run_benchmark(self, count, web_root):
        results = []
        debug_outputs = []
        try:
            self._browser_driver.prepare_initial_env(self._config)
            for iteration in xrange(1, count + 1):
                _log.info('Start the iteration {current_iteration} of {iterations} for current benchmark'.format(current_iteration=iteration, iterations=count))
                try:
                    self._browser_driver.prepare_env(self._config)

                    if 'entry_point' in self._plan:
                        result = self._run_one_test(web_root, self._plan['entry_point'])
                        debug_outputs.append(result.pop('debugOutput', None))
                        assert(result)
                        results.append(result)
                    elif 'test_files' in self._plan:
                        run_result = {}
                        for test in self._plan['test_files']:
                            result = self._run_one_test(web_root, test)
                            assert(result)
                            run_result = self._merge(run_result, result)
                            debug_outputs.append(result.pop('debugOutput', None))

                        results.append(run_result)
                    else:
                        raise Exception('Plan does not contain entry_point or test_files')

                finally:
                    self._browser_driver.restore_env()

                _log.info('End the iteration {current_iteration} of {iterations} for current benchmark'.format(current_iteration=iteration, iterations=count))
        finally:
            self._browser_driver.restore_env_after_all_testing()

        results = self._wrap(results)
        output_file = self._output_file if self._output_file else self._plan['output_file']
        self._dump(self._merge({'debugOutput': debug_outputs}, results), output_file)
        self.show_results(results, self._scale_unit, self._show_iteration_values)

    def execute(self):
        with BenchmarkBuilder(self._plan_name, self._plan, self.name) as web_root:
            self._run_benchmark(int(self._plan['count']), web_root)

    @classmethod
    def _dump(cls, results, output_file):
        _log.info('Dumping the results to file {output_file}'.format(output_file=output_file))
        try:
            with open(output_file, 'w') as fp:
                json.dump(results, fp)
        except IOError as error:
            _log.error('Cannot open output file: {output_file} - Error: {error}'.format(output_file=output_file, error=error))
            _log.error('Results are:\n {result}'.format(result=json.dumps(results)))

    @classmethod
    def _wrap(cls, dicts):
        _log.debug('Merging following results:\n{results}'.format(results=json.dumps(dicts)))
        if not dicts:
            return None
        ret = {}
        for dic in dicts:
            ret = cls._merge(ret, dic)
        _log.debug('Results after merging:\n{result}'.format(result=json.dumps(ret)))
        return ret

    @classmethod
    def _merge(cls, a, b):
        assert(isinstance(a, type(b)))
        arg_type = type(a)
        # special handle for list type, and should be handle before equal check
        if arg_type == types.ListType and len(a) and (type(a[0]) == types.StringType or type(a[0]) == types.UnicodeType):
            return a
        if arg_type == types.DictType:
            result = {}
            for key, value in a.items():
                if key in b:
                    result[key] = cls._merge(value, b[key])
                else:
                    result[key] = value
            for key, value in b.items():
                if key not in result:
                    result[key] = value
            return result
        # for other types
        return a + b

    @classmethod
    def show_results(cls, results, scale_unit=True, show_iteration_values=False):
        results = BenchmarkResults(results)
        print(results.format(scale_unit, show_iteration_values))
