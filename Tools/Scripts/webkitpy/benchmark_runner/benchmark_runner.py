import json
import logging
import sys
import os
from collections import defaultdict, OrderedDict

from webkitpy.benchmark_runner.benchmark_builder import BenchmarkBuilder
from webkitpy.benchmark_runner.benchmark_results import BenchmarkResults
from webkitpy.benchmark_runner.browser_driver.browser_driver_factory import BrowserDriverFactory


if sys.version_info > (3, 0):
    def istext(a):
        return isinstance(a, bytes) or isinstance(a, str)
else:
    def istext(a):
        return isinstance(a, str) or isinstance(a, unicode)


_log = logging.getLogger(__name__)


class BenchmarkRunner(object):
    name = 'benchmark_runner'

    def __init__(self, plan_file, local_copy, count_override, timeout_override, build_dir, output_file, platform, browser, browser_path, subtests=None, scale_unit=True, show_iteration_values=False, device_id=None, diagnose_dir=None, pgo_profile_output_dir=None, profile_output_dir=None):
        self._plan_name, self._plan = BenchmarkRunner._load_plan_data(plan_file)
        if 'options' not in self._plan:
            self._plan['options'] = {}
        if local_copy:
            self._plan['local_copy'] = local_copy
        if count_override:
            self._plan['count'] = count_override
        if timeout_override:
            self._plan['timeout'] = timeout_override
        self._subtests = self.validate_subtests(subtests) if subtests else None
        self._browser_driver = BrowserDriverFactory.create(platform, browser)
        self._browser_path = browser_path
        self._build_dir = os.path.abspath(build_dir) if build_dir else None
        self._diagnose_dir = os.path.abspath(diagnose_dir) if diagnose_dir else None
        if self._diagnose_dir:
            os.makedirs(self._diagnose_dir, exist_ok=True)
            _log.info('Collecting diagnostics to {}'.format(self._diagnose_dir))
        self._pgo_profile_output_dir = os.path.abspath(pgo_profile_output_dir) if pgo_profile_output_dir else None
        if self._pgo_profile_output_dir:
            os.makedirs(self._pgo_profile_output_dir, exist_ok=True)
            _log.info('Collecting PGO profiles to {}'.format(self._pgo_profile_output_dir))
        self._profile_output_dir = os.path.abspath(profile_output_dir) if profile_output_dir else None
        if self._profile_output_dir:
            os.makedirs(self._profile_output_dir, exist_ok=True)
            _log.info('Collecting profiles to {}'.format(self._profile_output_dir))
        self._output_file = output_file
        self._scale_unit = scale_unit
        self._show_iteration_values = show_iteration_values
        self._config = self._plan.get('config', {})
        if device_id:
            self._config['device_id'] = device_id
        self._config['enable_signposts'] = True if self._profile_output_dir else False

    @staticmethod
    def _find_plan_file(plan_file):
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
    def _load_plan_data(plan_file):
        plan_file = BenchmarkRunner._find_plan_file(plan_file)
        try:
            with open(plan_file, 'r') as fp:
                plan_name = os.path.split(os.path.splitext(plan_file)[0])[1]
                plan = json.load(fp)
                return plan_name, plan
        except IOError as error:
            _log.error('Can not open plan file: {plan_file} - Error {error}'.format(plan_file=plan_file, error=error))
            raise error
        except ValueError as error:
            _log.error('Plan file: {plan_file} may not follow JSON format - Error {error}'.format(plan_file=plan_file, error=error))
            raise error

    @staticmethod
    def plan_directory():
        return os.path.join(os.path.dirname(__file__), 'data/plans')

    @staticmethod
    def available_plans():
        plans = [os.path.splitext(plan_file)[0] for plan_file in os.listdir(BenchmarkRunner.plan_directory()) if plan_file.endswith(".plan")]
        return plans

    @staticmethod
    def format_subtests(subtests):
        # An ordered dictionary helps us prioritize listing subtests without a suite (denoted with key: "")
        subtests = OrderedDict(subtests)
        subtest_list = []
        for suite, tests in subtests.items():
            subtest_list += ['{suite}/{test}'.format(suite=suite, test=test) if suite else test for test in tests]
        return subtest_list

    @staticmethod
    def available_subtests(plan_file):
        plan = BenchmarkRunner._load_plan_data(plan_file)[1]
        return plan.get('subtests', {})

    def validate_subtests(self, subtests):
        if 'subtests' not in self._plan:
            raise Exception('Subtests are not available for the specified plan.')
        valid_subtests = OrderedDict(self._plan['subtests'])
        subtests_to_run = defaultdict(list)
        for subtest in subtests:
            if '/' in subtest:
                subtest_suite, subtest_name = subtest.split('/')
            else:
                subtest_suite, subtest_name = None, subtest
            did_append_subtest = False
            if subtest_suite:
                if subtest_suite not in valid_subtests:
                    _log.warning('{} does not belong to a valid suite, skipping'.format(subtest))
                    continue
                if subtest_name in valid_subtests[subtest_suite]:
                    subtests_to_run[subtest_suite].append(subtest_name)
                    did_append_subtest = True
            else:  # If no suite is provided, we should do our best to infer one.
                for suite, tests in valid_subtests.items():
                    if subtest_name in tests:
                        subtests_to_run[suite].append(subtest_name)
                        did_append_subtest = True
                        break
            if not did_append_subtest:
                _log.warning('{} is not a valid subtest, skipping'.format(subtest))
        if subtests_to_run:
            _log.info('Running subtests: {}'.format(BenchmarkRunner.format_subtests(subtests_to_run)))
            return subtests_to_run
        else:
            raise Exception('No valid subtests were specified')

    def _run_one_test(self, web_root, test_file, iteration):
        raise NotImplementedError('BenchmarkRunner is an abstract class and shouldn\'t be instantiated.')

    def _run_benchmark(self, count, web_root):
        results = []
        debug_outputs = []
        try:
            self._browser_driver.prepare_initial_env(self._config)
            for iteration in range(1, count + 1):
                _log.info('Start the iteration {current_iteration} of {iterations} for current benchmark'.format(current_iteration=iteration, iterations=count))
                try:
                    self._browser_driver.prepare_env(self._config)

                    if 'entry_point' in self._plan:
                        result = self._run_one_test(web_root, self._plan['entry_point'], iteration)
                        debug_outputs.append(result.pop('debugOutput', None))
                        assert(result)
                        results.append(result)
                    elif 'test_files' in self._plan:
                        run_result = {}
                        for test in self._plan['test_files']:
                            result = self._run_one_test(web_root, test, iteration)
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
        with BenchmarkBuilder(self._plan_name, self._plan, self.name, enable_signposts=self._config['enable_signposts']) as web_root:
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
        if arg_type == list and len(a) and istext(a[0]):
            return a
        if arg_type == dict:
            result = {}
            for key, value in list(a.items()):
                if key in b:
                    result[key] = cls._merge(value, b[key])
                else:
                    result[key] = value
            for key, value in list(b.items()):
                if key not in result:
                    result[key] = value
            return result
        # for other types
        return a + b

    @classmethod
    def show_results(cls, results, scale_unit=True, show_iteration_values=False):
        results = BenchmarkResults(results)
        print(results.format(scale_unit, show_iteration_values))
