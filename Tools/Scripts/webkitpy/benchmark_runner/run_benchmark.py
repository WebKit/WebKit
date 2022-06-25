#!/usr/bin/env python3

import argparse
import json
import logging
import os
import sys
import time

from webkitpy.benchmark_runner.browser_driver.browser_driver_factory import BrowserDriverFactory
from webkitpy.benchmark_runner.benchmark_runner import BenchmarkRunner
from webkitpy.benchmark_runner.webdriver_benchmark_runner import WebDriverBenchmarkRunner
from webkitpy.benchmark_runner.webserver_benchmark_runner import WebServerBenchmarkRunner


_log = logging.getLogger(__name__)
benchmark_runner_subclasses = {
    WebDriverBenchmarkRunner.name: WebDriverBenchmarkRunner,
    WebServerBenchmarkRunner.name: WebServerBenchmarkRunner,
}


def default_platform():
    if sys.platform.startswith('linux'):
        return 'linux'
    return 'osx'


def default_browser():
    if sys.platform.startswith('linux'):
        return 'minibrowser-gtk'
    return 'safari'


def default_diagnose_dir():
    return '/tmp/run-benchmark-diagnostics-{}/'.format(int(time.time()))


def config_argument_parser():
    diagnose_directory = default_diagnose_dir()
    parser = argparse.ArgumentParser(description='Run browser based performance benchmarks. To run a single benchmark in the recommended way, use run-benchmark --plan. To see the vailable benchmarks, use run-benchmark --list-plans. This script passes through the __XPC variables in its environment to the Safari process.')
    mutual_group = parser.add_mutually_exclusive_group(required=True)
    mutual_group.add_argument('--plan', help='Run a specific benchmark plan (e.g. speedometer, jetstream).')
    mutual_group.add_argument('--list-plans', action='store_true', help='List all available benchmark plans.')
    mutual_group.add_argument('--allplans', action='store_true', help='Run all available benchmark plans in order.')
    mutual_group.add_argument('--read-results-json', dest='json_file', help='Instead of running a benchmark, format the output saved in JSON_FILE.')
    parser.add_argument('--output-file', default=None, help='Save detailed results to OUTPUT in JSON format. By default, results will not be saved.')
    parser.add_argument('--count', type=int, help='Number of times to run the benchmark (e.g. 5).')
    parser.add_argument('--driver', default=WebServerBenchmarkRunner.name, choices=list(benchmark_runner_subclasses.keys()), help='Use the specified benchmark driver. Defaults to %s.' % WebServerBenchmarkRunner.name)
    parser.add_argument('--browser', default=default_browser(), choices=BrowserDriverFactory.available_browsers(), help='Browser to run the nechmark in. Defaults to %s.' % default_browser())
    parser.add_argument('--platform', default=default_platform(), choices=BrowserDriverFactory.available_platforms(), help='Platform that this script is running on. Defaults to %s.' % default_platform())
    parser.add_argument('--local-copy', help='Path to a local copy of the benchmark (e.g. PerformanceTests/SunSpider/).')
    parser.add_argument('--device-id', default=None, help='Undocumented option for mobile device testing.')
    parser.add_argument('--debug', action='store_true', help='Enable debug logging.')
    parser.add_argument('--diagnose-directory', dest='diagnose_dir', default=diagnose_directory, help='Directory for storing diagnose information on test failure. Defaults to {}.'.format(diagnose_directory))
    parser.add_argument('--no-adjust-unit', dest='scale_unit', action='store_false', help="Don't convert to scientific notation.")
    parser.add_argument('--show-iteration-values', dest='show_iteration_values', action='store_true', help="Show the measured value for each iteration in addition to averages.")
    parser.add_argument('--generate-profiles', dest='generate_profiles', action='store_true', help="Collect LLVM profiles for PGO, and copy them to the diagnose directory.")

    group = parser.add_mutually_exclusive_group()
    group.add_argument('--browser-path', help='Specify the path to a non-default copy of the target browser as a path to the .app.')
    group.add_argument('--build-directory', dest='build_dir', help='Path to the browser executable (e.g. WebKitBuild/Release/).')

    return parser


# FIXME: Remove default arguments when all dependent scripts adopt this change.
def parse_args(parser=None):
    if parser is None:
        parser = config_argument_parser()
    args = parser.parse_args()

    if args.debug:
        _log.setLevel(logging.DEBUG)
    _log.debug('Initializing program with following parameters')
    _log.debug('\toutput file name\t: %s' % args.output_file)
    _log.debug('\tbuild directory\t: %s' % args.build_dir)
    _log.debug('\tplan name\t: %s', args.plan)

    if args.generate_profiles and not os.path.isdir(args.diagnose_dir):
        _log.error("No diagnose directory to dump profiles to: {}".format(args.diagnose_dir))
        exit()

    if args.generate_profiles and args.platform is not 'osx':
        _log.error("Profile generation is currently only supported on macOS.")
        exit()

    return args


def run_benchmark_plan(args, plan):
    benchmark_runner_class = benchmark_runner_subclasses[args.driver]
    runner = benchmark_runner_class(plan,
                                    args.local_copy, args.count, args.build_dir, args.output_file,
                                    args.platform, args.browser, args.browser_path, args.scale_unit,
                                    args.show_iteration_values, args.device_id, args.diagnose_dir,
                                    args.diagnose_dir if args.generate_profiles else None)
    runner.execute()


def list_benchmark_plans():
    print("Available benchmark plans: ")
    for plan in BenchmarkRunner.available_plans():
        print("\t%s" % plan)


def start(args):
    if args.json_file:
        results_json = json.load(open(args.json_file, 'r'))
        if 'debugOutput' in results_json:
            del results_json['debugOutput']
        BenchmarkRunner.show_results(results_json, args.scale_unit, args.show_iteration_values)
        return
    if args.allplans:
        failed = []
        skipped = []
        planlist = BenchmarkRunner.available_plans()
        skippedfile = os.path.join(BenchmarkRunner.plan_directory(), 'Skipped')
        if not planlist:
            raise Exception('Cant find any .plan file in directory %s' % BenchmarkRunner.plan_directory())
        if os.path.isfile(skippedfile):
            skipped = [line.strip() for line in open(skippedfile) if not line.startswith('#') and len(line) > 1]
        for plan in sorted(planlist):
            if plan in skipped:
                _log.info('Skipping benchmark plan: %s because is listed on the Skipped file' % plan)
                continue
            _log.info('Starting benchmark plan: %s' % plan)
            try:
                run_benchmark_plan(args, plan)
                _log.info('Finished benchmark plan: %s' % plan)
            except KeyboardInterrupt:
                raise
            except:
                failed.append(plan)
                _log.exception('Error running benchmark plan: %s' % plan)
        if failed:
            _log.error('The following benchmark plans have failed: %s' % failed)
        return len(failed)
    if args.list_plans:
        list_benchmark_plans()
        return

    run_benchmark_plan(args, args.plan)


def format_logger(logger):
    logger.setLevel(logging.INFO)
    ch = logging.StreamHandler()
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
    ch.setFormatter(formatter)
    logger.addHandler(ch)


def main():
    return start(parse_args(config_argument_parser()))
