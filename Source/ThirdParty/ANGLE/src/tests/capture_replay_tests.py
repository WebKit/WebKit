#! /usr/bin/env vpython3
#
# Copyright 2020 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
"""
Script testing capture_replay with angle_end2end_tests
"""

# Automation script will:
# 1. Build all tests in angle_end2end with frame capture enabled
# 2. Run each test with frame capture
# 3. Build CaptureReplayTest with cpp trace files
# 4. Run CaptureReplayTest
# 5. Output the number of test successes and failures. A test succeeds if no error occurs during
# its capture and replay, and the GL states at the end of two runs match. Any unexpected failure
# will return non-zero exit code

# Run this script with Python to test capture replay on angle_end2end tests
# python path/to/capture_replay_tests.py
# Command line arguments: run with --help for a full list.

import argparse
import difflib
import distutils.util
import fnmatch
import json
import logging
import math
import multiprocessing
import os
import psutil
import queue
import re
import shutil
import subprocess
import sys
import tempfile
import time
import traceback

PIPE_STDOUT = True
DEFAULT_OUT_DIR = "out/CaptureReplayTest"  # relative to angle folder
DEFAULT_FILTER = "*/ES2_Vulkan_SwiftShader"
DEFAULT_TEST_SUITE = "angle_end2end_tests"
REPLAY_SAMPLE_FOLDER = "src/tests/capture_replay_tests"  # relative to angle folder
DEFAULT_BATCH_COUNT = 8  # number of tests batched together
TRACE_FILE_SUFFIX = "_context"  # because we only deal with 1 context right now
RESULT_TAG = "*RESULT"
STATUS_MESSAGE_PERIOD = 20  # in seconds
SUBPROCESS_TIMEOUT = 600  # in seconds
DEFAULT_RESULT_FILE = "results.txt"
DEFAULT_LOG_LEVEL = "info"
DEFAULT_MAX_JOBS = 8
DEFAULT_MAX_NINJA_JOBS = 3
REPLAY_BINARY = "capture_replay_tests"
if sys.platform == "win32":
    REPLAY_BINARY += ".exe"
TRACE_FOLDER = "traces"

EXIT_SUCCESS = 0
EXIT_FAILURE = 1
REPLAY_INITIALIZATION_FAILURE = -1
REPLAY_SERIALIZATION_FAILURE = -2

switch_case_without_return_template = """\
        case {case}:
            {namespace}::{call}({params});
            break;
"""

switch_case_with_return_template = """\
        case {case}:
            return {namespace}::{call}({params});
"""

default_case_without_return_template = """\
        default:
            break;"""
default_case_with_return_template = """\
        default:
            return {default_val};"""


def winext(name, ext):
    return ("%s.%s" % (name, ext)) if sys.platform == "win32" else name


def AutodetectGoma():
    for p in psutil.process_iter():
        try:
            if winext('compiler_proxy', 'exe') == p.name():
                return True
        except:
            pass
    return False


class SubProcess():

    def __init__(self, command, logger, env=os.environ, pipe_stdout=PIPE_STDOUT):
        # shell=False so that only 1 subprocess is spawned.
        # if shell=True, a shell process is spawned, which in turn spawns the process running
        # the command. Since we do not have a handle to the 2nd process, we cannot terminate it.
        if pipe_stdout:
            self.proc_handle = subprocess.Popen(
                command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=False)
        else:
            self.proc_handle = subprocess.Popen(command, env=env, shell=False)
        self._logger = logger

    def Join(self, timeout):
        self._logger.debug('Joining with subprocess %d, timeout %s' % (self.Pid(), str(timeout)))
        output = self.proc_handle.communicate(timeout=timeout)[0]
        if output:
            output = output.decode('utf-8')
        else:
            output = ''
        return self.proc_handle.returncode, output

    def Pid(self):
        return self.proc_handle.pid

    def Kill(self):
        self.proc_handle.terminate()
        self.proc_handle.wait()


# class that manages all child processes of a process. Any process thats spawns subprocesses
# should have this. This object is created inside the main process, and each worker process.
class ChildProcessesManager():

    @classmethod
    def _GetGnAndNinjaAbsolutePaths(self):
        path = os.path.join('third_party', 'depot_tools')
        return os.path.join(path, winext('gn', 'bat')), os.path.join(path, winext('ninja', 'exe'))

    def __init__(self, args, logger, ninja_lock):
        # a dictionary of Subprocess, with pid as key
        self.subprocesses = {}
        # list of Python multiprocess.Process handles
        self.workers = []

        self._gn_path, self._ninja_path = self._GetGnAndNinjaAbsolutePaths()
        self._use_goma = AutodetectGoma()
        self._logger = logger
        self._ninja_lock = ninja_lock
        self.runtimes = {}
        self._args = args

    def RunSubprocess(self, command, env=None, pipe_stdout=True, timeout=None):
        proc = SubProcess(command, self._logger, env, pipe_stdout)
        self._logger.debug('Created subprocess: %s with pid %d' % (' '.join(command), proc.Pid()))
        self.subprocesses[proc.Pid()] = proc
        start_time = time.time()
        try:
            returncode, output = self.subprocesses[proc.Pid()].Join(timeout)
            elapsed_time = time.time() - start_time
            cmd_name = os.path.basename(command[0])
            self.runtimes.setdefault(cmd_name, 0.0)
            self.runtimes[cmd_name] += elapsed_time
            self.RemoveSubprocess(proc.Pid())
            if returncode != 0:
                return -1, output
            return returncode, output
        except KeyboardInterrupt:
            raise
        except subprocess.TimeoutExpired as e:
            self.RemoveSubprocess(proc.Pid())
            return -2, str(e)
        except Exception as e:
            self.RemoveSubprocess(proc.Pid())
            return -1, str(e)

    def RemoveSubprocess(self, subprocess_id):
        assert subprocess_id in self.subprocesses
        self.subprocesses[subprocess_id].Kill()
        del self.subprocesses[subprocess_id]

    def AddWorker(self, worker):
        self.workers.append(worker)

    def KillAll(self):
        for subprocess_id in self.subprocesses:
            self.subprocesses[subprocess_id].Kill()
        for worker in self.workers:
            worker.terminate()
            worker.join()
            worker.close()  # to release file descriptors immediately
        self.subprocesses = {}
        self.workers = []

    def JoinWorkers(self):
        for worker in self.workers:
            worker.join()
            worker.close()
        self.workers = []

    def IsAnyWorkerAlive(self):
        return any([worker.is_alive() for worker in self.workers])

    def GetRemainingWorkers(self):
        count = 0
        for worker in self.workers:
            if worker.is_alive():
                count += 1
        return count

    def RunGNGen(self, build_dir, pipe_stdout, extra_gn_args=[]):
        gn_args = [('angle_with_capture_by_default', 'true')] + extra_gn_args
        if self._use_goma:
            gn_args.append(('use_goma', 'true'))
            if self._args.goma_dir:
                gn_args.append(('goma_dir', '"%s"' % self._args.goma_dir))
        if not self._args.debug:
            gn_args.append(('is_debug', 'false'))
            gn_args.append(('symbol_level', '1'))
            gn_args.append(('angle_assert_always_on', 'true'))
        if self._args.asan:
            gn_args.append(('is_asan', 'true'))
        args_str = ' '.join(['%s=%s' % (k, v) for (k, v) in gn_args])
        cmd = [self._gn_path, 'gen', '--args=%s' % args_str, build_dir]
        self._logger.info(' '.join(cmd))
        return self.RunSubprocess(cmd, pipe_stdout=pipe_stdout)

    def RunNinja(self, build_dir, target, pipe_stdout):
        cmd = [self._ninja_path]

        # This code is taken from depot_tools/autoninja.py
        if self._use_goma:
            num_cores = multiprocessing.cpu_count()
            cmd.append('-j')
            core_multiplier = 40
            j_value = num_cores * core_multiplier

            if sys.platform.startswith('win'):
                # On windows, j value higher than 1000 does not improve build performance.
                j_value = min(j_value, 1000)
            elif sys.platform == 'darwin':
                # On Mac, j value higher than 500 causes 'Too many open files' error
                # (crbug.com/936864).
                j_value = min(j_value, 500)

            cmd.append('%d' % j_value)
        else:
            cmd.append('-l')
            cmd.append('%d' % os.cpu_count())

        cmd += ['-C', build_dir, target]
        with self._ninja_lock:
            self._logger.info(' '.join(cmd))
            return self.RunSubprocess(cmd, pipe_stdout=pipe_stdout)


def GetTestsListForFilter(args, test_path, filter, logger):
    cmd = GetRunCommand(args, test_path) + ["--list-tests", "--gtest_filter=%s" % filter]
    logger.info('Getting test list from "%s"' % " ".join(cmd))
    return subprocess.check_output(cmd, text=True)


def ParseTestNamesFromTestList(output, test_expectation, also_run_skipped_for_capture_tests,
                               logger):
    output_lines = output.splitlines()
    tests = []
    seen_start_of_tests = False
    disabled = 0
    for line in output_lines:
        l = line.strip()
        if l == 'Tests list:':
            seen_start_of_tests = True
        elif l == 'End tests list.':
            break
        elif not seen_start_of_tests:
            pass
        elif not test_expectation.TestIsSkippedForCapture(l) or also_run_skipped_for_capture_tests:
            tests.append(l)
        else:
            disabled += 1

    logger.info('Found %s tests and %d disabled tests.' % (len(tests), disabled))
    return tests


def GetRunCommand(args, command):
    if args.xvfb:
        return ['vpython', 'testing/xvfb.py', command]
    else:
        return [command]


class GroupedResult():
    Passed = "Pass"
    Failed = "Fail"
    TimedOut = "Timeout"
    Crashed = "Crashed"
    CompileFailed = "CompileFailed"
    Skipped = "Skipped"
    FailedToTrace = "FailedToTrace"

    ResultTypes = [Passed, Failed, TimedOut, Crashed, CompileFailed, Skipped, FailedToTrace]

    def __init__(self, resultcode, message, output, tests):
        self.resultcode = resultcode
        self.message = message
        self.output = output
        self.tests = []
        for test in tests:
            self.tests.append(test)


class TestBatchResult():

    display_output_lines = 20

    def __init__(self, grouped_results, verbose):
        self.results = {}
        for result_type in GroupedResult.ResultTypes:
            self.results[result_type] = []

        for grouped_result in grouped_results:
            for test in grouped_result.tests:
                self.results[grouped_result.resultcode].append(test.full_test_name)

        self.repr_str = ""
        self.GenerateRepresentationString(grouped_results, verbose)

    def __str__(self):
        return self.repr_str

    def GenerateRepresentationString(self, grouped_results, verbose):
        for grouped_result in grouped_results:
            self.repr_str += grouped_result.resultcode + ": " + grouped_result.message + "\n"
            for test in grouped_result.tests:
                self.repr_str += "\t" + test.full_test_name + "\n"
            if verbose:
                self.repr_str += grouped_result.output
            else:
                if grouped_result.resultcode == GroupedResult.CompileFailed:
                    self.repr_str += TestBatchResult.ExtractErrors(grouped_result.output)
                elif grouped_result.resultcode != GroupedResult.Passed:
                    self.repr_str += TestBatchResult.GetAbbreviatedOutput(grouped_result.output)

    def ExtractErrors(output):
        lines = output.splitlines()
        error_lines = []
        for i in range(len(lines)):
            if ": error:" in lines[i]:
                error_lines.append(lines[i] + "\n")
                if i + 1 < len(lines):
                    error_lines.append(lines[i + 1] + "\n")
        return "".join(error_lines)

    def GetAbbreviatedOutput(output):
        # Get all lines after and including the last occurance of "Run".
        lines = output.splitlines()
        line_count = 0
        for line_index in reversed(range(len(lines))):
            line_count += 1
            if "[ RUN      ]" in lines[line_index]:
                break

        return '\n' + '\n'.join(lines[-line_count:]) + '\n'


class Test():

    def __init__(self, test_name):
        self.full_test_name = test_name
        self.params = test_name.split('/')[1]
        self.context_id = 0
        self.test_index = -1  # index of test within a test batch
        self._label = self.full_test_name.replace(".", "_").replace("/", "_")
        self.skipped_by_suite = False

    def __str__(self):
        return self.full_test_name + " Params: " + self.params

    def GetLabel(self):
        return self._label

    def CanRunReplay(self, trace_folder_path):
        test_files = []
        label = self.GetLabel()
        assert (self.context_id == 0)
        for f in os.listdir(trace_folder_path):
            if os.path.isfile(os.path.join(trace_folder_path, f)) and f.startswith(label):
                test_files.append(f)
        frame_files_count = 0
        context_header_count = 0
        context_source_count = 0
        source_json_count = 0
        context_id = 0
        for f in test_files:
            # TODO: Consolidate. http://anglebug.com/7753
            if "_001.cpp" in f or "_001.c" in f:
                frame_files_count += 1
            elif f.endswith(".json"):
                source_json_count += 1
            elif f.endswith(".h"):
                context_header_count += 1
                if TRACE_FILE_SUFFIX in f:
                    context = f.split(TRACE_FILE_SUFFIX)[1][:-2]
                    context_id = int(context)
            # TODO: Consolidate. http://anglebug.com/7753
            elif f.endswith(".cpp") or f.endswith(".c"):
                context_source_count += 1
        can_run_replay = frame_files_count >= 1 and context_header_count >= 1 \
            and context_source_count >= 1 and source_json_count == 1
        if not can_run_replay:
            return False
        self.context_id = context_id
        return True


def _FormatEnv(env):
    return ' '.join(['%s=%s' % (k, v) for (k, v) in env.items()])


class TestBatch():

    CAPTURE_FRAME_END = 100

    def __init__(self, args, logger):
        self.args = args
        self.tests = []
        self.results = []
        self.logger = logger

    def SetWorkerId(self, worker_id):
        self.trace_dir = "%s%d" % (TRACE_FOLDER, worker_id)
        self.trace_folder_path = os.path.join(REPLAY_SAMPLE_FOLDER, self.trace_dir)

    def RunWithCapture(self, args, child_processes_manager):
        test_exe_path = os.path.join(args.out_dir, 'Capture', args.test_suite)

        extra_env = {
            'ANGLE_CAPTURE_SERIALIZE_STATE': '1',
            'ANGLE_FEATURE_OVERRIDES_ENABLED': 'forceRobustResourceInit:forceInitShaderVariables',
            'ANGLE_CAPTURE_ENABLED': '1',
            'ANGLE_CAPTURE_OUT_DIR': self.trace_folder_path,
        }

        if args.mec > 0:
            extra_env['ANGLE_CAPTURE_FRAME_START'] = '{}'.format(args.mec)
            extra_env['ANGLE_CAPTURE_FRAME_END'] = '{}'.format(args.mec + 1)
        else:
            extra_env['ANGLE_CAPTURE_FRAME_END'] = '{}'.format(self.CAPTURE_FRAME_END)

        if args.expose_nonconformant_features:
            extra_env[
                'ANGLE_FEATURE_OVERRIDES_ENABLED'] += ':exposeNonConformantExtensionsAndVersions'

        env = {**os.environ.copy(), **extra_env}

        if not self.args.keep_temp_files:
            ClearFolderContent(self.trace_folder_path)
        filt = ':'.join([test.full_test_name for test in self.tests])

        cmd = GetRunCommand(args, test_exe_path)
        results_file = tempfile.mktemp()
        cmd += [
            '--gtest_filter=%s' % filt,
            '--angle-per-test-capture-label',
            '--results-file=' + results_file,
        ]
        self.logger.info('%s %s' % (_FormatEnv(extra_env), ' '.join(cmd)))

        returncode, output = child_processes_manager.RunSubprocess(
            cmd, env, timeout=SUBPROCESS_TIMEOUT)

        if args.show_capture_stdout:
            self.logger.info("Capture stdout: %s" % output)

        if returncode == -1:
            self.results.append(GroupedResult(GroupedResult.Crashed, "", output, self.tests))
            return False
        elif returncode == -2:
            self.results.append(GroupedResult(GroupedResult.TimedOut, "", "", self.tests))
            return False

        with open(results_file) as f:
            test_results = json.load(f)
        os.unlink(results_file)
        for test in self.tests:
            test_result = test_results['tests'][test.full_test_name]
            if test_result['actual'] == 'SKIP':
                test.skipped_by_suite = True

        return True

    def RemoveTestsThatDoNotProduceAppropriateTraceFiles(self):
        continued_tests = []
        skipped_tests = []
        failed_to_trace_tests = []
        for test in self.tests:
            if not test.CanRunReplay(self.trace_folder_path):
                if test.skipped_by_suite:
                    skipped_tests.append(test)
                else:
                    failed_to_trace_tests.append(test)
            else:
                continued_tests.append(test)
        if len(skipped_tests) > 0:
            self.results.append(
                GroupedResult(GroupedResult.Skipped, "Skipping replay since test skipped by suite",
                              "", skipped_tests))
        if len(failed_to_trace_tests) > 0:
            self.results.append(
                GroupedResult(GroupedResult.FailedToTrace,
                              "Test not skipped but failed to produce trace files", "",
                              failed_to_trace_tests))

        return continued_tests

    def BuildReplay(self, replay_build_dir, composite_file_id, tests, child_processes_manager):
        # write gni file that holds all the traces files in a list
        self.CreateTestNamesFile(composite_file_id, tests)

        gn_args = [('angle_build_capture_replay_tests', 'true'),
                   ('angle_capture_replay_test_trace_dir', '"%s"' % self.trace_dir),
                   ('angle_capture_replay_composite_file_id', str(composite_file_id))]
        returncode, output = child_processes_manager.RunGNGen(replay_build_dir, True, gn_args)
        if returncode != 0:
            self.logger.warning('GN failure output: %s' % output)
            self.results.append(
                GroupedResult(GroupedResult.CompileFailed, "Build replay failed at gn generation",
                              output, tests))
            return False
        returncode, output = child_processes_manager.RunNinja(replay_build_dir, REPLAY_BINARY,
                                                              True)
        if returncode != 0:
            self.logger.warning('Ninja failure output: %s' % output)
            self.results.append(
                GroupedResult(GroupedResult.CompileFailed, "Build replay failed at ninja", output,
                              tests))
            return False
        return True

    def RunReplay(self, args, replay_build_dir, replay_exe_path, child_processes_manager, tests):
        extra_env = {}
        if args.expose_nonconformant_features:
            extra_env[
                'ANGLE_FEATURE_OVERRIDES_ENABLED'] = 'exposeNonConformantExtensionsAndVersions'

        env = {**os.environ.copy(), **extra_env}

        run_cmd = GetRunCommand(self.args, replay_exe_path)
        self.logger.info('%s %s' % (_FormatEnv(extra_env), ' '.join(run_cmd)))

        for test in tests:
            self.UnlinkContextStateJsonFilesIfPresent(replay_build_dir, test.GetLabel())

        returncode, output = child_processes_manager.RunSubprocess(
            run_cmd, env, timeout=SUBPROCESS_TIMEOUT)
        if returncode == -1:
            cmd = replay_exe_path
            self.results.append(
                GroupedResult(GroupedResult.Crashed, "Replay run crashed (%s)" % cmd, output,
                              tests))
            return
        elif returncode == -2:
            self.results.append(
                GroupedResult(GroupedResult.TimedOut, "Replay run timed out", output, tests))
            return

        if args.show_replay_stdout:
            self.logger.info("Replay stdout: %s" % output)

        output_lines = output.splitlines()
        passes = []
        fails = []
        count = 0
        for output_line in output_lines:
            words = output_line.split(" ")
            if len(words) == 3 and words[0] == RESULT_TAG:
                test_name = self.FindTestByLabel(words[1])
                result = int(words[2])
                if result == 0:
                    passes.append(test_name)
                elif result == REPLAY_INITIALIZATION_FAILURE:
                    fails.append(test_name)
                    self.logger.info("Initialization failure: %s" % test_name)
                elif result == REPLAY_SERIALIZATION_FAILURE:
                    fails.append(test_name)
                    self.logger.info("Context comparison failed: %s" % test_name)
                    self.PrintContextDiff(replay_build_dir, words[1])
                else:
                    fails.append(test_name)
                    self.logger.error("Unknown test result code: %s -> %d" % (test_name, result))
                count += 1

        if len(passes) > 0:
            self.results.append(GroupedResult(GroupedResult.Passed, "", output, passes))
        if len(fails) > 0:
            self.results.append(GroupedResult(GroupedResult.Failed, "", output, fails))

    def UnlinkContextStateJsonFilesIfPresent(self, replay_build_dir, test_name):
        frame = 1
        while True:
            capture_file = "{}/{}_ContextCaptured{}.json".format(replay_build_dir, test_name,
                                                                 frame)
            replay_file = "{}/{}_ContextReplayed{}.json".format(replay_build_dir, test_name, frame)
            if os.path.exists(capture_file):
                os.unlink(capture_file)
            if os.path.exists(replay_file):
                os.unlink(replay_file)

            if frame > self.CAPTURE_FRAME_END:
                break
            frame = frame + 1

    def PrintContextDiff(self, replay_build_dir, test_name):
        frame = 1
        found = False
        while True:
            capture_file = "{}/{}_ContextCaptured{}.json".format(replay_build_dir, test_name,
                                                                 frame)
            replay_file = "{}/{}_ContextReplayed{}.json".format(replay_build_dir, test_name, frame)
            if os.path.exists(capture_file) and os.path.exists(replay_file):
                found = True
                captured_context = open(capture_file, "r").readlines()
                replayed_context = open(replay_file, "r").readlines()
                for line in difflib.unified_diff(
                        captured_context, replayed_context, fromfile=capture_file,
                        tofile=replay_file):
                    print(line, end="")
            else:
                if frame > self.CAPTURE_FRAME_END:
                    break
            frame = frame + 1
        if not found:
            self.logger.error("Could not find serialization diff files for %s" % test_name)

    def FindTestByLabel(self, label):
        for test in self.tests:
            if test.GetLabel() == label:
                return test
        return None

    def AddTest(self, test):
        assert len(self.tests) <= self.args.batch_count
        test.index = len(self.tests)
        self.tests.append(test)

    def CreateTestNamesFile(self, composite_file_id, tests):
        data = {'traces': [test.GetLabel() for test in tests]}
        names_path = os.path.join(self.trace_folder_path, 'test_names_%d.json' % composite_file_id)
        with open(names_path, 'w') as f:
            f.write(json.dumps(data))

    def __str__(self):
        repr_str = "TestBatch:\n"
        for test in self.tests:
            repr_str += ("\t" + str(test) + "\n")
        return repr_str

    def __getitem__(self, index):
        assert index < len(self.tests)
        return self.tests[index]

    def __iter__(self):
        return iter(self.tests)

    def GetResults(self):
        return TestBatchResult(self.results, self.args.verbose)


class TestExpectation():
    # tests that must not be run as list
    skipped_for_capture_tests = {}
    skipped_for_capture_tests_re = {}

    # test expectations for tests that do not pass
    non_pass_results = {}

    # tests that must run in a one-test batch
    run_single = {}
    run_single_re = {}

    flaky_tests = []

    non_pass_re = {}

    # yapf: disable
    # we want each pair on one line
    result_map = { "FAIL" : GroupedResult.Failed,
                   "TIMEOUT" : GroupedResult.TimedOut,
                   "CRASH" : GroupedResult.Crashed,
                   "COMPILE_FAIL" : GroupedResult.CompileFailed,
                   "NOT_RUN" : GroupedResult.Skipped,
                   "SKIP_FOR_CAPTURE" : GroupedResult.Skipped,
                   "PASS" : GroupedResult.Passed}
    # yapf: enable

    def __init__(self, args):
        expected_results_filename = "capture_replay_expectations.txt"
        expected_results_path = os.path.join(REPLAY_SAMPLE_FOLDER, expected_results_filename)
        self._asan = args.asan
        with open(expected_results_path, "rt") as f:
            for line in f:
                l = line.strip()
                if l != "" and not l.startswith("#"):
                    self.ReadOneExpectation(l, args.debug)

    def _CheckTagsWithConfig(self, tags, config_tags):
        for tag in tags:
            if tag not in config_tags:
                return False
        return True

    def ReadOneExpectation(self, line, is_debug):
        (testpattern, result) = line.split('=')
        (test_info_string, test_name_string) = testpattern.split(':')
        test_name = test_name_string.strip()
        test_info = test_info_string.strip().split()
        result_stripped = result.strip()

        tags = []
        if len(test_info) > 1:
            tags = test_info[1:]

        config_tags = [GetPlatformForSkip()]
        if self._asan:
            config_tags += ['ASAN']
        if is_debug:
            config_tags += ['DEBUG']

        if self._CheckTagsWithConfig(tags, config_tags):
            test_name_regex = re.compile('^' + test_name.replace('*', '.*') + '$')
            if result_stripped == 'CRASH' or result_stripped == 'COMPILE_FAIL':
                self.run_single[test_name] = self.result_map[result_stripped]
                self.run_single_re[test_name] = test_name_regex
            if result_stripped == 'SKIP_FOR_CAPTURE' or result_stripped == 'TIMEOUT':
                self.skipped_for_capture_tests[test_name] = self.result_map[result_stripped]
                self.skipped_for_capture_tests_re[test_name] = test_name_regex
            elif result_stripped == 'FLAKY':
                self.flaky_tests.append(test_name_regex)
            else:
                self.non_pass_results[test_name] = self.result_map[result_stripped]
                self.non_pass_re[test_name] = test_name_regex

    def TestIsSkippedForCapture(self, test_name):
        for p in self.skipped_for_capture_tests_re.values():
            m = p.match(test_name)
            if m is not None:
                return True
        return False

    def TestNeedsToRunSingle(self, test_name):
        for p in self.run_single_re.values():
            m = p.match(test_name)
            if m is not None:
                return True
            for p in self.skipped_for_capture_tests_re.values():
                m = p.match(test_name)
                if m is not None:
                    return True
        return False

    def Filter(self, test_list, run_all_tests):
        result = {}
        for t in test_list:
            for key in self.non_pass_results.keys():
                if self.non_pass_re[key].match(t) is not None:
                    result[t] = self.non_pass_results[key]
            for key in self.run_single.keys():
                if self.run_single_re[key].match(t) is not None:
                    result[t] = self.run_single[key]
            if run_all_tests:
                for [key, r] in self.skipped_for_capture_tests.items():
                    if self.skipped_for_capture_tests_re[key].match(t) is not None:
                        result[t] = r
        return result

    def IsFlaky(self, test_name):
        for flaky in self.flaky_tests:
            if flaky.match(test_name) is not None:
                return True
        return False


def ClearFolderContent(path):
    all_files = []
    for f in os.listdir(path):
        if os.path.isfile(os.path.join(path, f)):
            os.remove(os.path.join(path, f))

def SetCWDToAngleFolder():
    cwd = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    os.chdir(cwd)
    return cwd


def RunTests(args, worker_id, job_queue, result_list, message_queue, logger, ninja_lock):
    replay_build_dir = os.path.join(args.out_dir, 'Replay%d' % worker_id)
    replay_exec_path = os.path.join(replay_build_dir, REPLAY_BINARY)

    child_processes_manager = ChildProcessesManager(args, logger, ninja_lock)
    # used to differentiate between multiple composite files when there are multiple test batchs
    # running on the same worker and --deleted_trace is set to False
    composite_file_id = 1
    while not job_queue.empty():
        try:
            test_batch = job_queue.get()
            logger.info('Starting {} tests on worker {}. Unstarted jobs: {}'.format(
                len(test_batch.tests), worker_id, job_queue.qsize()))

            test_batch.SetWorkerId(worker_id)

            success = test_batch.RunWithCapture(args, child_processes_manager)
            if not success:
                result_list.append(test_batch.GetResults())
                logger.info(str(test_batch.GetResults()))
                continue
            continued_tests = test_batch.RemoveTestsThatDoNotProduceAppropriateTraceFiles()
            if len(continued_tests) == 0:
                result_list.append(test_batch.GetResults())
                logger.info(str(test_batch.GetResults()))
                continue
            success = test_batch.BuildReplay(replay_build_dir, composite_file_id, continued_tests,
                                             child_processes_manager)
            if args.keep_temp_files:
                composite_file_id += 1
            if not success:
                result_list.append(test_batch.GetResults())
                logger.info(str(test_batch.GetResults()))
                continue
            test_batch.RunReplay(args, replay_build_dir, replay_exec_path, child_processes_manager,
                                 continued_tests)
            result_list.append(test_batch.GetResults())
            logger.info(str(test_batch.GetResults()))
        except KeyboardInterrupt:
            child_processes_manager.KillAll()
            raise
        except queue.Empty:
            child_processes_manager.KillAll()
            break
        except Exception as e:
            logger.error('RunTestsException: %s\n%s' % (repr(e), traceback.format_exc()))
            child_processes_manager.KillAll()
            pass
    message_queue.put(child_processes_manager.runtimes)
    child_processes_manager.KillAll()


def SafeDeleteFolder(folder_name):
    while os.path.isdir(folder_name):
        try:
            shutil.rmtree(folder_name)
        except KeyboardInterrupt:
            raise
        except PermissionError:
            pass


def DeleteReplayBuildFolders(folder_num, replay_build_dir, trace_folder):
    for i in range(folder_num):
        folder_name = replay_build_dir + str(i)
        if os.path.isdir(folder_name):
            SafeDeleteFolder(folder_name)


def CreateTraceFolders(folder_num):
    for i in range(folder_num):
        folder_name = TRACE_FOLDER + str(i)
        folder_path = os.path.join(REPLAY_SAMPLE_FOLDER, folder_name)
        if os.path.isdir(folder_path):
            shutil.rmtree(folder_path)
        os.makedirs(folder_path)


def DeleteTraceFolders(folder_num):
    for i in range(folder_num):
        folder_name = TRACE_FOLDER + str(i)
        folder_path = os.path.join(REPLAY_SAMPLE_FOLDER, folder_name)
        if os.path.isdir(folder_path):
            SafeDeleteFolder(folder_path)


def GetPlatformForSkip():
    # yapf: disable
    # we want each pair on one line
    platform_map = { 'win32' : 'WIN',
                     'linux' : 'LINUX' }
    # yapf: enable
    return platform_map.get(sys.platform, 'UNKNOWN')


def main(args):
    logger = multiprocessing.log_to_stderr()
    logger.setLevel(level=args.log.upper())

    ninja_lock = multiprocessing.Semaphore(args.max_ninja_jobs)
    child_processes_manager = ChildProcessesManager(args, logger, ninja_lock)
    try:
        start_time = time.time()
        # set the number of workers to be cpu_count - 1 (since the main process already takes up a
        # CPU core). Whenever a worker is available, it grabs the next job from the job queue and
        # runs it. The worker closes down when there is no more job.
        worker_count = min(multiprocessing.cpu_count() - 1, args.max_jobs)
        cwd = SetCWDToAngleFolder()

        CreateTraceFolders(worker_count)
        capture_build_dir = os.path.normpath(r'%s/Capture' % args.out_dir)
        returncode, output = child_processes_manager.RunGNGen(capture_build_dir, False)
        if returncode != 0:
            logger.error(output)
            child_processes_manager.KillAll()
            return EXIT_FAILURE
        # run ninja to build all tests
        returncode, output = child_processes_manager.RunNinja(capture_build_dir, args.test_suite,
                                                              False)
        if returncode != 0:
            logger.error(output)
            child_processes_manager.KillAll()
            return EXIT_FAILURE
        # get a list of tests
        test_path = os.path.join(capture_build_dir, args.test_suite)
        test_list = GetTestsListForFilter(args, test_path, args.filter, logger)
        test_expectation = TestExpectation(args)
        test_names = ParseTestNamesFromTestList(test_list, test_expectation,
                                                args.also_run_skipped_for_capture_tests, logger)
        test_expectation_for_list = test_expectation.Filter(
            test_names, args.also_run_skipped_for_capture_tests)
        # objects created by manager can be shared by multiple processes. We use it to create
        # collections that are shared by multiple processes such as job queue or result list.
        manager = multiprocessing.Manager()
        job_queue = manager.Queue()
        test_batch_num = 0

        num_tests = len(test_names)
        test_index = 0

        # Put the tests into batches and these into the job queue; jobs that areexpected to crash,
        # timeout, or fail compilation will be run in batches of size one, because a crash or
        # failing to compile brings down the whole batch, so that we would give false negatives if
        # such a batch contains jobs that would otherwise poss or fail differently.
        while test_index < num_tests:
            batch = TestBatch(args, logger)

            while test_index < num_tests and len(batch.tests) < args.batch_count:
                test_name = test_names[test_index]
                test_obj = Test(test_name)

                if test_expectation.TestNeedsToRunSingle(test_name):
                    single_batch = TestBatch(args, logger)
                    single_batch.AddTest(test_obj)
                    job_queue.put(single_batch)
                    test_batch_num += 1
                else:
                    batch.AddTest(test_obj)

                test_index += 1

            if len(batch.tests) > 0:
                job_queue.put(batch)
                test_batch_num += 1

        passed_count = 0
        failed_count = 0
        timedout_count = 0
        crashed_count = 0
        compile_failed_count = 0
        skipped_count = 0

        unexpected_count = {}
        unexpected_test_results = {}

        for type in GroupedResult.ResultTypes:
            unexpected_count[type] = 0
            unexpected_test_results[type] = []

        # result list is created by manager and can be shared by multiple processes. Each
        # subprocess populates the result list with the results of its test runs. After all
        # subprocesses finish, the main process processes the results in the result list.
        # An item in the result list is a tuple with 3 values (testname, result, output).
        # The "result" can take 3 values "Passed", "Failed", "Skipped". The output is the
        # stdout and the stderr of the test appended together.
        result_list = manager.list()
        message_queue = manager.Queue()
        # so that we do not spawn more processes than we actually need
        worker_count = min(worker_count, test_batch_num)
        # spawning and starting up workers
        for worker_id in range(worker_count):
            proc = multiprocessing.Process(
                target=RunTests,
                args=(args, worker_id, job_queue, result_list, message_queue, logger, ninja_lock))
            child_processes_manager.AddWorker(proc)
            proc.start()

        # print out periodic status messages
        while child_processes_manager.IsAnyWorkerAlive():
            logger.info('%d workers running, %d jobs left.' %
                        (child_processes_manager.GetRemainingWorkers(), (job_queue.qsize())))
            # If only a few tests are run it is likely that the workers are finished before
            # the STATUS_MESSAGE_PERIOD has passed, and the tests script sits idle for the
            # reminder of the wait time. Therefore, limit waiting by the number of
            # unfinished jobs.
            unfinished_jobs = job_queue.qsize() + child_processes_manager.GetRemainingWorkers()
            time.sleep(min(STATUS_MESSAGE_PERIOD, unfinished_jobs))

        child_processes_manager.JoinWorkers()
        end_time = time.time()

        summed_runtimes = child_processes_manager.runtimes
        while not message_queue.empty():
            runtimes = message_queue.get()
            for k, v in runtimes.items():
                summed_runtimes.setdefault(k, 0.0)
                summed_runtimes[k] += v

        # print out results
        logger.info('')
        logger.info('Results:')

        flaky_results = []

        regression_error_log = []

        for test_batch in result_list:
            test_batch_result = test_batch.results
            logger.debug(str(test_batch_result))

            batch_has_regression = False

            passed_count += len(test_batch_result[GroupedResult.Passed])
            failed_count += len(test_batch_result[GroupedResult.Failed])
            timedout_count += len(test_batch_result[GroupedResult.TimedOut])
            crashed_count += len(test_batch_result[GroupedResult.Crashed])
            compile_failed_count += len(test_batch_result[GroupedResult.CompileFailed])
            skipped_count += len(test_batch_result[GroupedResult.Skipped])

            for real_result, test_list in test_batch_result.items():
                for test in test_list:
                    if test_expectation.IsFlaky(test):
                        flaky_results.append('{} ({})'.format(test, real_result))
                        continue

                    # Passing tests are not in the list
                    if test not in test_expectation_for_list.keys():
                        if real_result != GroupedResult.Passed:
                            batch_has_regression = True
                            unexpected_count[real_result] += 1
                            unexpected_test_results[real_result].append(
                                '{} {} (expected Pass or is new test)'.format(test, real_result))
                    else:
                        expected_result = test_expectation_for_list[test]
                        if real_result != expected_result:
                            if real_result != GroupedResult.Passed:
                                batch_has_regression = True
                            unexpected_count[real_result] += 1
                            unexpected_test_results[real_result].append(
                                '{} {} (expected {})'.format(test, real_result, expected_result))
            if batch_has_regression:
                regression_error_log.append(str(test_batch))

        if len(regression_error_log) > 0:
            logger.info('Logging output of test batches with regressions')
            logger.info(
                '==================================================================================================='
            )
            for log in regression_error_log:
                logger.info(log)
                logger.info(
                    '---------------------------------------------------------------------------------------------------'
                )
                logger.info('')

        logger.info('')
        logger.info('Elapsed time: %.2lf seconds' % (end_time - start_time))
        logger.info('')
        logger.info('Runtimes by process:\n%s' %
                    '\n'.join('%s: %.2lf seconds' % (k, v) for (k, v) in summed_runtimes.items()))

        if len(flaky_results):
            logger.info("Flaky test(s):")
            for line in flaky_results:
                logger.info("    {}".format(line))
            logger.info("")

        logger.info(
            'Summary: Passed: %d, Comparison Failed: %d, Crashed: %d, CompileFailed %d, Skipped: %d, Timeout: %d'
            % (passed_count, failed_count, crashed_count, compile_failed_count, skipped_count,
               timedout_count))

        retval = EXIT_SUCCESS

        unexpected_test_results_count = 0
        for result, count in unexpected_count.items():
            if result != GroupedResult.Skipped:  # Suite skipping tests is ok
                unexpected_test_results_count += count

        if unexpected_test_results_count > 0:
            retval = EXIT_FAILURE
            logger.info('')
            logger.info('Failure: Obtained {} results that differ from expectation:'.format(
                unexpected_test_results_count))
            logger.info('')
            for result, count in unexpected_count.items():
                if count > 0 and result != GroupedResult.Skipped:
                    logger.info("Unexpected '{}' ({}):".format(result, count))
                    for test_result in unexpected_test_results[result]:
                        logger.info('     {}'.format(test_result))
                    logger.info('')

        logger.info('')

        # delete generated folders if --keep-temp-files flag is set to false
        if args.purge:
            DeleteTraceFolders(worker_count)
            if os.path.isdir(args.out_dir):
                SafeDeleteFolder(args.out_dir)

        # Try hard to ensure output is finished before ending the test.
        logging.shutdown()
        sys.stdout.flush()
        time.sleep(2.0)
        return retval

    except KeyboardInterrupt:
        child_processes_manager.KillAll()
        return EXIT_FAILURE


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--out-dir',
        default=DEFAULT_OUT_DIR,
        help='Where to build ANGLE for capture and replay. Relative to the ANGLE folder. Default is "%s".'
        % DEFAULT_OUT_DIR)
    parser.add_argument(
        '-f',
        '--filter',
        '--gtest_filter',
        default=DEFAULT_FILTER,
        help='Same as GoogleTest\'s filter argument. Default is "%s".' % DEFAULT_FILTER)
    parser.add_argument(
        '--test-suite',
        default=DEFAULT_TEST_SUITE,
        help='Test suite binary to execute. Default is "%s".' % DEFAULT_TEST_SUITE)
    parser.add_argument(
        '--batch-count',
        default=DEFAULT_BATCH_COUNT,
        type=int,
        help='Number of tests in a batch. Default is %d.' % DEFAULT_BATCH_COUNT)
    parser.add_argument(
        '--keep-temp-files',
        action='store_true',
        help='Whether to keep the temp files and folders. Off by default')
    parser.add_argument('--purge', help='Purge all build directories on exit.')
    parser.add_argument(
        '--goma-dir',
        default='',
        help='Set custom goma directory. Uses the goma in path by default.')
    parser.add_argument(
        '--output-to-file',
        action='store_true',
        help='Whether to write output to a result file. Off by default')
    parser.add_argument(
        '--result-file',
        default=DEFAULT_RESULT_FILE,
        help='Name of the result file in the capture_replay_tests folder. Default is "%s".' %
        DEFAULT_RESULT_FILE)
    parser.add_argument('-v', '--verbose', action='store_true', help='Shows full test output.')
    parser.add_argument(
        '-l',
        '--log',
        default=DEFAULT_LOG_LEVEL,
        help='Controls the logging level. Default is "%s".' % DEFAULT_LOG_LEVEL)
    parser.add_argument(
        '-j',
        '--max-jobs',
        default=DEFAULT_MAX_JOBS,
        type=int,
        help='Maximum number of test processes. Default is %d.' % DEFAULT_MAX_JOBS)
    parser.add_argument(
        '-M',
        '--mec',
        default=0,
        type=int,
        help='Enable mid execution capture starting at specified frame, (default: 0 = normal capture)'
    )
    parser.add_argument(
        '-a',
        '--also-run-skipped-for-capture-tests',
        action='store_true',
        help='Also run tests that are disabled in the expectations by SKIP_FOR_CAPTURE')
    parser.add_argument(
        '--max-ninja-jobs',
        type=int,
        default=DEFAULT_MAX_NINJA_JOBS,
        help='Maximum number of concurrent ninja jobs to run at once.')
    parser.add_argument('--xvfb', action='store_true', help='Run with xvfb.')
    parser.add_argument('--asan', action='store_true', help='Build with ASAN.')
    parser.add_argument(
        '-E',
        '--expose-nonconformant-features',
        action='store_true',
        help='Expose non-conformant features to advertise GLES 3.2')
    parser.add_argument(
        '--show-capture-stdout', action='store_true', help='Print test stdout during capture.')
    parser.add_argument(
        '--show-replay-stdout', action='store_true', help='Print test stdout during replay.')
    parser.add_argument('--debug', action='store_true', help='Debug builds (default is Release).')
    args = parser.parse_args()
    if args.debug and (args.out_dir == DEFAULT_OUT_DIR):
        args.out_dir = args.out_dir + "Debug"

    if sys.platform == "win32":
        args.test_suite += ".exe"
    if args.output_to_file:
        logging.basicConfig(level=args.log.upper(), filename=args.result_file)
    else:
        logging.basicConfig(level=args.log.upper())

    sys.exit(main(args))
