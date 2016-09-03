#!/usr/bin/env python
#-------------------------------------------------------------------------------------------------------
# Copyright (C) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#-------------------------------------------------------------------------------------------------------

from __future__ import print_function
from datetime import datetime
from multiprocessing import Pool, Manager
from threading import Timer
import sys
import os
import subprocess as SP
import traceback
import argparse
import xml.etree.ElementTree as ET
import re
import time

# handle command line args
parser = argparse.ArgumentParser(
    description='ChakraCore *nix Test Script',
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog='''\
Samples:

test all folders:
    runtests.py

test only Array:
    runtests.py Array

test a single file:
    runtests.py Basics/hello.js
''')

parser.add_argument('folders', metavar='folder', nargs='*',
                    help='folder subset to run tests')
parser.add_argument('-b', '--binary', metavar='bin', help='ch full path')
parser.add_argument('-d', '--debug', action='store_true',
                    help='use debug build');
parser.add_argument('-t', '--test', action='store_true', help='use test build')
parser.add_argument('--include-slow', action='store_true',
                    help='include slow tests')
parser.add_argument('--only-slow', action='store_true',
                    help='run only slow tests')
parser.add_argument('--nightly', action='store_true',
                    help='run as nightly tests')
parser.add_argument('--tag', nargs='*',
                    help='select tests with given tags')
parser.add_argument('--not-tag', nargs='*',
                    help='exclude tests with given tags')
parser.add_argument('--timeout', type=int, default=60,
                    help='test timeout (default 60 seconds)')
parser.add_argument('-l', '--logfile', metavar='logfile', help='file to log results to', default=None)
parser.add_argument('--x86', action='store_true', help='use x86 build')
parser.add_argument('--x64', action='store_true', help='use x64 build')
args = parser.parse_args()


test_root = os.path.dirname(os.path.realpath(__file__))
repo_root = os.path.dirname(test_root)

# arch: x86, x64
arch = 'x86' if args.x86 else ('x64' if args.x64 else None)
if arch == None:
    arch = os.environ.get('_BuildArch', 'x86')
if sys.platform != 'win32':
    arch = 'x64'    # xplat: hard code arch == x64
arch_alias = 'amd64' if arch == 'x64' else None

# flavor: debug, test, release
type_flavor = {'chk':'Debug', 'test':'Test', 'fre':'Release'}
flavor = 'Debug' if args.debug else ('Test' if args.test else None)
if flavor == None:
    print("ERROR: Test build target wasn't defined.")
    print("Try '-t' (test build) or '-d' (debug build).")
    sys.exit(1)
flavor_alias = 'chk' if flavor == 'Debug' else 'fre'

# binary: full ch path
binary = args.binary
if binary == None:
    if sys.platform == 'win32':
        binary = 'Build/VcBuild/bin/{}_{}/ch.exe'.format(arch, flavor)
    else:
        binary = 'BuildLinux/{0}/ch'.format(flavor)
    binary = os.path.join(repo_root, binary)
if not os.path.isfile(binary):
    print('{} not found. Did you run ./build.sh already?'.format(binary))
    sys.exit(1)

# global tags/not_tags
tags = set(args.tag or [])
not_tags = set(args.not_tag or []).union(['fail', 'exclude_' + arch])

if arch_alias:
    not_tags.add('exclude_' + arch_alias)
if flavor_alias:
    not_tags.add('exclude_' + flavor_alias)
if args.only_slow:
    tags.add('Slow')
elif not args.include_slow:
    not_tags.add('Slow')

not_tags.add('exclude_nightly' if args.nightly else 'nightly')

# xplat: temp hard coded to exclude unsupported tests
if sys.platform != 'win32':
    not_tags.add('exclude_xplat')
    not_tags.add('Intl')
    not_tags.add('require_backend')
    not_tags.add('require_debugger')
if sys.platform == 'darwin':
    not_tags.add('exclude_mac')
not_compile_flags = set(['-simdjs']) \
    if sys.platform != 'win32' else None

# use tags/not_tags/not_compile_flags as case-insensitive
def lower_set(s):
    return set([x.lower() for x in s] if s else [])

tags = lower_set(tags)
not_tags = lower_set(not_tags)
not_compile_flags = lower_set(not_compile_flags)

# split tags text into tags set
_empty_set = set()
def split_tags(text):
    return set(x.strip() for x in text.lower().split(',')) if text \
            else _empty_set

class LogFile(object):
    def __init__(self, log_file_path = None):
        self.file = None

        if log_file_path is None:
            # Set up the log file paths
            # Make sure the right directory exists and the log file doesn't
            log_file_name = "testrun.{0}{1}.log".format(arch, flavor)
            log_file_directory = os.path.join(repo_root, "test", "logs")

            if not os.path.exists(log_file_directory):
                os.mkdir(log_file_directory)

            self.log_file_path = os.path.join(log_file_directory, log_file_name)

            if os.path.exists(self.log_file_path):
                os.remove(self.log_file_path)
        else:
            self.log_file_path = log_file_path

        self.file = open(self.log_file_path, "w")

    def log(self, args):
        self.file.write(args)

    def __del__(self):
        if not (self.file is None):
            self.file.close()

if __name__ == '__main__':
    log_file = LogFile(args.logfile)

def log_message(msg = ""):
    log_file.log(msg + "\n")

def print_and_log(msg = ""):
    print(msg)
    log_message(msg)

# remove carriage returns at end of line to avoid platform difference
def normalize_new_line(text):
    return re.sub(b'[\r]+\n', b'\n', text)

# A test simply contains a collection of test attributes.
# Misc attributes added by test run:
#   filename        full path of test file
#   elapsed_time    elapsed time when running the test
#
class Test(dict):
    __setattr__ = dict.__setitem__
    __delattr__ = dict.__delitem__

    # support dot syntax for normal attribute access
    def __getattr__(self, key):
        return super(Test, self).__getattr__(key) if key.startswith('__') \
                else self.get(key)

    # mark start of this test run, to compute elapsed_time
    def start(self):
        self.start_time = datetime.now()

    # mark end of this test run, compute elapsed_time
    def done(self):
        if not self.elapsed_time:
            self.elapsed_time = (datetime.now() - self.start_time)\
                                .total_seconds()

# records pass_count/fail_count
class PassFailCount(object):
    def __init__(self):
        self.pass_count = 0
        self.fail_count = 0

    def __str__(self):
        return 'passed {}, failed {}'.format(self.pass_count, self.fail_count)

    def total_count(self):
        return self.pass_count + self.fail_count

# records total and individual folder's pass_count/fail_count
class TestResult(PassFailCount):
    def __init__(self):
        super(self.__class__, self).__init__()
        self.folders = {}

    def _get_folder_result(self, folder):
        r = self.folders.get(folder)
        if not r:
            r = PassFailCount()
            self.folders[folder] = r
        return r

    def log(self, filename, fail=False):
        folder = os.path.basename(os.path.dirname(filename))
        r = self._get_folder_result(folder)
        if fail:
            r.fail_count += 1
            self.fail_count += 1
        else:
            r.pass_count += 1
            self.pass_count += 1

# test variants:
#   interpreted: -maxInterpretCount:1 -maxSimpleJitRunCount:1 -bgjit-
#   dynapogo: -forceNative -off:simpleJit -bgJitDelay:0
class TestVariant(object):
    def __init__(self, name, compile_flags=[]):
        self.name = name
        self.compile_flags = \
            ['-WERExceptionSupport', '-ExtendedErrorStackForTestHost',
             '-BaselineMode'] + compile_flags
        self.tags = tags.copy()
        self.not_tags = not_tags.union(
            ['{}_{}'.format(x, name) for x in ('fails','exclude')])

        self.msg_queue = Manager().Queue() # messages from multi processes
        self.test_result = TestResult()
        self._print_lines = [] # _print lines buffer
        self._last_len = 0

    # check if this test variant should run a given test
    def _should_test(self, test):
        tags = split_tags(test.get('tags'))
        if not tags.isdisjoint(self.not_tags):
            return False
        if self.tags and not self.tags.issubset(tags):
            return False
        if not_compile_flags: # exclude unsupported compile-flags if any
            flags = test.get('compile-flags')
            if flags and \
                    not not_compile_flags.isdisjoint(flags.lower().split()):
                return False
        return True

    # print output from multi-process run, to be sent with result message
    def _print(self, line):
        self._print_lines.append(str(line))

    # queue a test result from multi-process runs
    def _log_result(self, test, fail):
        output = '\n'.join(self._print_lines) # collect buffered _print output
        self._print_lines = []
        self.msg_queue.put((test.filename, fail, test.elapsed_time, output))

    # (on main process) process one queued message
    def _process_msg(self, msg):
        filename, fail, elapsed_time, output = msg
        self.test_result.log(filename, fail=fail)
        line = '[{}/{} {:4.2f}] {} -> {}'.format(
            self.test_result.total_count(),
            self.test_count,
            elapsed_time,
            'Failed' if fail else 'Passed',
            self._short_name(filename))
        padding = self._last_len - len(line)
        print(line + ' ' * padding, end='\n' if fail else '\r')
        log_message(line)
        self._last_len = len(line) if not fail else 0
        if len(output) > 0:
            print_and_log(output)

    # get a shorter test file path for display only
    def _short_name(self, filename):
        folder = os.path.basename(os.path.dirname(filename))
        return os.path.join(folder, os.path.basename(filename))

    # (on main process) wait and process one queued message
    def _process_one_msg(self):
        self._process_msg(self.msg_queue.get())

    # log a failed test with details
    def _show_failed(self, test, flags, exit_code, output,
                    expected_output=None, timedout=False):
        if timedout:
            self._print('ERROR: Test timed out!')
        self._print('{} {} {}'.format(binary, ' '.join(flags), test.filename))
        if expected_output == None or timedout:
            self._print("\nOutput:")
            self._print("----------------------------")
            self._print(output)
            self._print("----------------------------")
        else:
            lst_output = output.split(b'\n')
            lst_expected = expected_output.split(b'\n')
            ln = min(len(lst_output), len(lst_expected))
            for i in range(0, ln):
                if lst_output[i] != lst_expected[i]:
                    self._print("Output: (at line " + str(i) + ")")
                    self._print("----------------------------")
                    self._print(lst_output[i])
                    self._print("----------------------------")
                    self._print("Expected Output:")
                    self._print("----------------------------")
                    self._print(lst_expected[i])
                    self._print("----------------------------")
                    break

        self._print("exit code: {}".format(exit_code))
        self._log_result(test, fail=True)

    # temp: try find real file name on hard drive if case mismatch
    def _check_file(self, folder, filename):
        path = os.path.join(folder, filename)
        if os.path.isfile(path):
            return path     # file exists on disk

        filename_lower = filename.lower()
        files = os.listdir(folder)
        for i in range(len(files)):
            if files[i].lower() == filename_lower:
                self._print('\nWARNING: {} should be {}\n'.format(
                    path, files[i]))
                return os.path.join(folder, files[i])

        # cann't find the file, just return the path and let it error out
        return path

    # run one test under this variant
    def test_one(self, test):
        try:
            test.start()
            self._run_one_test(test)
        except Exception:
            test.done()
            self._print(traceback.format_exc())
            self._log_result(test, fail=True)

    # internally perform one test run
    def _run_one_test(self, test):
        folder = test.folder
        js_file = test.filename = self._check_file(folder, test.files)
        js_output = b''

        working_path = os.path.dirname(js_file)

        flags = test.get('compile-flags')
        flags = self.compile_flags + (flags.split() if flags else [])
        cmd = [binary] + flags + [os.path.basename(js_file)]

        test.start()
        proc = SP.Popen(cmd, stdout=SP.PIPE, stderr=SP.STDOUT, cwd=working_path)
        timeout_data = [proc, False]
        def timeout_func(timeout_data):
            timeout_data[0].kill()
            timeout_data[1] = True
        timeout = test.get('timeout', args.timeout) # test override or default
        timer = Timer(timeout, timeout_func, [timeout_data])
        try:
            timer.start()
            js_output = normalize_new_line(proc.communicate()[0])
            exit_code = proc.wait()
        finally:
            timer.cancel()
        test.done()

        # shared _show_failed args
        fail_args = { 'test': test, 'flags': flags,
                      'exit_code': exit_code, 'output': js_output };

        # check timed out
        if (timeout_data[1]):
            return self._show_failed(timedout=True, **fail_args)

        # check ch failed
        if exit_code != 0:
            return self._show_failed(**fail_args)

        # check output
        if 'baseline' not in test:
            # output lines must be 'pass' or 'passed' or empty
            lines = (line.lower() for line in js_output.split(b'\n'))
            if any(line != b'' and line != b'pass' and line != b'passed'
                    for line in lines):
                return self._show_failed(**fail_args)
        else:
            baseline = test.get('baseline')
            if baseline:
                # perform baseline comparison
                baseline = self._check_file(working_path, baseline)
                with open(baseline, 'rb') as bs_file:
                    baseline_output = bs_file.read()

                # Cleanup carriage return
                # todo: remove carriage return at the end of the line
                #       or better fix ch to output same on all platforms
                expected_output = normalize_new_line(baseline_output)

                if expected_output != js_output:
                    return self._show_failed(
                        expected_output=expected_output, **fail_args)

        # passed
        self._log_result(test, fail=False)

    # run tests under this variant, using given multiprocessing Pool
    def run(self, tests, pool):
        print_and_log('\n############# Starting {} variant #############'\
                        .format(self.name))
        if self.tags:
            print_and_log('  tags: {}'.format(self.tags))
        for x in self.not_tags:
            print_and_log('  exclude: {}'.format(x))
        print_and_log()

        # filter tests to run
        tests = [x for x in tests if self._should_test(x)]
        self.test_count = len(tests)

        # run tests in parallel
        result = pool.map_async(run_one, [(self,test) for test in tests])
        while self.test_result.total_count() != self.test_count:
            self._process_one_msg()

    # print test result summary
    def print_summary(self):
        print_and_log('\n######## Logs for {} variant ########'\
                        .format(self.name))
        for folder, result in sorted(self.test_result.folders.items()):
            print_and_log('{}: {}'.format(folder, result))
        print_and_log("----------------------------")
        print_and_log('Total: {}'.format(self.test_result))

# global run one test function for multiprocessing, used by TestVariant
def run_one(data):
    try:
        variant, test = data
        variant.test_one(test)
    except Exception:
        print('ERROR: Unhandled exception!!!')
        traceback.print_exc()

# record folder/tags info from test_root/rlexedirs.xml
class FolderTags(object):
    def __init__(self):
        xmlpath = os.path.join(test_root, 'rlexedirs.xml')
        try:
            xml = ET.parse(xmlpath).getroot()
        except IOError:
            print_and_log('ERROR: failed to read {}'.format(xmlpath))
            exit(-1)

        self._folder_tags = {}
        for x in xml:
            d = x.find('default')
            key = d.find('files').text.lower() # avoid case mismatch
            tags = d.find('tags')
            self._folder_tags[key] = \
                split_tags(tags.text) if tags != None else _empty_set

    # check if should test a given folder
    def should_test(self, folder):
        key = os.path.basename(os.path.normpath(folder)).lower()
        ftags = self._folder_tags.get(key)

        # folder listed in rlexedirs.xml and not exlucded by global not_tags
        return ftags != None and ftags.isdisjoint(not_tags)


# load all tests in folder using rlexe.xml file
def load_tests(folder, file):
    try:
        xmlpath = os.path.join(folder, 'rlexe.xml')
        xml = ET.parse(xmlpath).getroot()
    except IOError:
        return []

    def test_override(condition, check_tag, check_value, test):
        target = condition.find(check_tag)
        if target != None and target.text == check_value:
            for override in condition.find('override'):
                test[override.tag] = override.text

    def load_test(testXml):
        test = Test(folder=folder)
        for c in testXml.find('default'):
            if c.tag == 'timeout':                       # timeout seconds
                test[c.tag] = int(c.text)
            elif c.tag == 'tags' and c.tag in test:      # merge multiple <tags>
                test[c.tag] = test[c.tag] + ',' + c.text
            else:
                test[c.tag] = c.text

        condition = testXml.find('condition')
        if condition != None:
            test_override(condition, 'target', arch_alias, test)

        return test

    tests = [load_test(x) for x in xml]
    if file != None:
        tests = [x for x in tests if x.files == file]
        if len(tests) == 0 and is_jsfile(file):
            tests = [Test(folder=folder, files=file, baseline='')]
    return tests

def is_jsfile(path):
    return os.path.splitext(path)[1] == '.js'

def main():
    # By default run all tests
    if len(args.folders) == 0:
        files = (os.path.join(test_root, x) for x in os.listdir(test_root))
        args.folders = [f for f in sorted(files) if not os.path.isfile(f)]

    # Set the right timezone, the tests need Pacific Standard Time
    os.environ['TZ'] = 'US/Pacific'
    time.tzset()

    # load all tests
    tests = []
    folder_tags = FolderTags()
    for path in args.folders:
        if os.path.isfile(path):
            folder, file = os.path.dirname(path), os.path.basename(path)
        else:
            folder, file = path, None
        if folder_tags.should_test(folder):
            tests += load_tests(folder, file)

    # test variants
    variants = [
        TestVariant('interpreted', [
            '-maxInterpretCount:1', '-maxSimpleJitRunCount:1', '-bgjit-'])
    ]

    # run each variant
    pool = Pool(1) # Use a multiprocessing process Pool -- TODO: forcing this to single threaded until we have support for sequential tag
    start_time = datetime.now()
    for variant in variants:
        variant.run(tests, pool)
    elapsed_time = datetime.now() - start_time

    # print summary
    for variant in variants:
        variant.print_summary()

    print()
    failed = any(variant.test_result.fail_count > 0 for variant in variants)
    print('[{}] {}'.format(
        str(elapsed_time), 'Success!' if not failed else 'Failed!'))

    return 1 if failed else 0

if __name__ == '__main__':
    sys.exit(main())
