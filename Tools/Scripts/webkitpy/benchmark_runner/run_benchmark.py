#!/usr/bin/env python

import argparse
import json
import logging
import platform
import os
import sys

from benchmark_runner import BenchmarkRunner
from browser_driver.browser_driver_factory import BrowserDriverFactory


_log = logging.getLogger(__name__)


def default_platform():
    if sys.platform.startswith('linux'):
        return 'linux'
    return 'osx'


def default_browser():
    if sys.platform.startswith('linux'):
        return 'minibrowser-gtk'
    return 'safari'


def parse_args():
    parser = argparse.ArgumentParser(description='Automate the browser based performance benchmarks')
    parser.add_argument('--output-file', dest='output', default=None)
    parser.add_argument('--build-directory', dest='buildDir', help='Path to the browser executable. e.g. WebKitBuild/Release/')
    parser.add_argument('--platform', dest='platform', default=default_platform(), choices=BrowserDriverFactory.available_platforms())
    parser.add_argument('--browser', dest='browser', default=default_browser(), choices=BrowserDriverFactory.available_browsers())
    parser.add_argument('--debug', action='store_true')
    parser.add_argument('--local-copy', dest='localCopy', help='Path to a local copy of the benchmark. e.g. PerformanceTests/SunSpider/')
    parser.add_argument('--count', dest='countOverride', type=int, help='Number of times to run the benchmark. e.g. 5')
    parser.add_argument('--device-id', dest='device_id', default=None)
    parser.add_argument('--no-adjust-unit', dest='scale_unit', action='store_false')
    mutual_group = parser.add_mutually_exclusive_group(required=True)
    mutual_group.add_argument('--read-results-json', dest='json_file', help='Specify file you want to format')
    mutual_group.add_argument('--plan', dest='plan', help='Benchmark plan to run. e.g. speedometer, jetstream')
    mutual_group.add_argument('--allplans', action='store_true', help='Run all available benchmark plans sequentially')

    args = parser.parse_args()

    if args.debug:
        _log.setLevel(logging.DEBUG)
    _log.debug('Initializing program with following parameters')
    _log.debug('\toutput file name\t: %s' % args.output)
    _log.debug('\tbuild directory\t: %s' % args.buildDir)
    _log.debug('\tplan name\t: %s', args.plan)

    return args


def start(args):
    if args.json_file:
        results_json = json.load(open(args.json_file, 'r'))
        if 'debugOutput' in results_json:
            del results_json['debugOutput']
        BenchmarkRunner.show_results(results_json, args.scale_unit)
        return
    if args.allplans:
        failed = []
        skipped = []
        plandir = os.path.join(os.path.dirname(__file__), 'data/plans')
        planlist = [os.path.splitext(f)[0] for f in os.listdir(plandir) if f.endswith('.plan')]
        skippedfile = os.path.join(plandir, 'Skipped')
        if not planlist:
            raise Exception('Cant find any .plan file in directory %s' % plandir)
        if os.path.isfile(skippedfile):
            skipped = [line.strip() for line in open(skippedfile) if not line.startswith('#') and len(line) > 1]
        for plan in sorted(planlist):
            if plan in skipped:
                _log.info('Skipping benchmark plan: %s because is listed on the Skipped file' % plan)
                continue
            _log.info('Starting benchmark plan: %s' % plan)
            try:
                runner = BenchmarkRunner(plan, args.localCopy, args.countOverride, args.buildDir, args.output, args.platform, args.browser, args.scale_unit, args.device_id)
                runner.execute()
                _log.info('Finished benchmark plan: %s' % plan)
            except KeyboardInterrupt:
                raise
            except:
                failed.append(plan)
                _log.exception('Error running benchmark plan: %s' % plan)
        if failed:
            _log.error('The following benchmark plans have failed: %s' % failed)
        return len(failed)
    runner = BenchmarkRunner(args.plan, args.localCopy, args.countOverride, args.buildDir, args.output, args.platform, args.browser, args.scale_unit, args.device_id)
    runner.execute()


def format_logger(logger):
    logger.setLevel(logging.INFO)
    ch = logging.StreamHandler()
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
    ch.setFormatter(formatter)
    logger.addHandler(ch)


def main():
    return start(parse_args())
