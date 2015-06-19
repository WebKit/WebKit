#!/usr/bin/env python

import argparse
import logging
import platform
import os

from benchmark_runner import BenchmarkRunner
from browser_driver.browser_driver_factory import BrowserDriverFactory


_log = logging.getLogger(__name__)


def parse_args():
    parser = argparse.ArgumentParser(description='Automate the browser based performance benchmarks')
    parser.add_argument('--output-file', dest='output', default=None)
    parser.add_argument('--build-directory', dest='buildDir', help='Path to the browser executable. e.g. WebKitBuild/Release/')
    parser.add_argument('--plan', dest='plan', required=True, help='Benchmark plan to run. e.g. speedometer, jetstream')
    parser.add_argument('--platform', dest='platform', required=True, choices=BrowserDriverFactory.available_platforms())
    # FIXME: Should we add chrome as an option? Well, chrome uses webkit in iOS.
    parser.add_argument('--browser', dest='browser', required=True, choices=BrowserDriverFactory.available_browsers())
    parser.add_argument('--debug', action='store_true')
    parser.add_argument('--local-copy', dest='localCopy', help='Path to a local copy of the benchmark. e.g. PerformanceTests/SunSpider/')
    parser.add_argument('--count', dest='countOverride', type=int, help='Number of times to run the benchmark. e.g. 5')
    parser.add_argument('--http-server-driver', dest='httpServerDriverOverride', default=None, help='Specify which HTTP server you wants to use')
    parser.add_argument('--device-id', dest='device_id', default=None)

    args = parser.parse_args()

    if args.debug:
        _log.setLevel(logging.DEBUG)
    _log.debug('Initializing program with following parameters')
    _log.debug('\toutput file name\t: %s' % args.output)
    _log.debug('\tbuild directory\t: %s' % args.buildDir)
    _log.debug('\tplan name\t: %s', args.plan)

    return args


def start(args):
    runner = BenchmarkRunner(args.plan, args.localCopy, args.countOverride, args.buildDir, args.output, args.platform, args.browser, args.httpServerDriverOverride, args.device_id)
    runner.execute()


def format_logger(logger):
    logger.setLevel(logging.INFO)
    ch = logging.StreamHandler()
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
    ch.setFormatter(formatter)
    logger.addHandler(ch)


def main():
    start(parse_args())
