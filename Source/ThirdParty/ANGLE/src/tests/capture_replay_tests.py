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
# Command line arguments:
# --capture_build_dir: specifies capture build directory relative to angle folder.
# Default is out/CaptureDebug
# --replay_build_dir: specifies replay build directory relative to angle folder.
# Default is out/ReplayDebug
# --use_goma: uses goma for compiling and linking test. Off by default
# --gtest_filter: same as gtest_filter of Google's test framework. Default is */ES2_Vulkan
# --test_suite: test suite to execute on. Default is angle_end2end_tests

import argparse
import multiprocessing
import os
import shlex
import shutil
import subprocess
import sys
import time

from sys import platform

DEFAULT_CAPTURE_BUILD_DIR = "out/CaptureTest"  # relative to angle folder
DEFAULT_REPLAY_BUILD_DIR = "out/ReplayTest"  # relative to angle folder
DEFAULT_FILTER = "*/ES2_Vulkan"
DEFAULT_TEST_SUITE = "angle_end2end_tests"
REPLAY_SAMPLE_FOLDER = "src/tests/capture_replay_tests"  # relative to angle folder


class SubProcess():

    def __init__(self, command, to_main_stdout):
        parsed_command = shlex.split(command)
        # shell=False so that only 1 subprocess is spawned.
        # if shell=True, a shell probess is spawned, which in turn spawns the process running
        # the command. Since we do not have a handle to the 2nd process, we cannot terminate it.
        if not to_main_stdout:
            self.proc_handle = subprocess.Popen(
                parsed_command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=False)
        else:
            self.proc_handle = subprocess.Popen(parsed_command, shell=False)

    def BlockingRun(self):
        try:
            output = self.proc_handle.communicate()[0]
            return self.proc_handle.returncode, output
        except Exception as e:
            self.Kill()
            return -1, str(e)

    def Kill(self):
        self.proc_handle.kill()
        self.proc_handle.wait()


def CreateGnGenSubProcess(gn_path, build_dir, arguments, to_main_stdout=False):
    command = '"' + gn_path + '"' + ' gen --args="'
    is_first_argument = True
    for argument in arguments:
        if is_first_argument:
            is_first_argument = False
        else:
            command += ' '
        command += argument[0]
        command += '='
        command += argument[1]
    command += '" '
    command += build_dir
    return SubProcess(command, to_main_stdout)


def CreateAutoninjaSubProcess(autoninja_path, build_dir, target, to_main_stdout=False):
    command = '"' + autoninja_path + '" '
    command += target
    command += " -C "
    command += build_dir
    return SubProcess(command, to_main_stdout)


def GetGnAndAutoninjaAbsolutePaths():
    # get gn/autoninja absolute path because subprocess with shell=False doesn't look
    # into the PATH environment variable on Windows
    depot_tools_name = "depot_tools"
    paths = os.environ["PATH"].split(";")
    for path in paths:
        if path.endswith(depot_tools_name):
            if platform == "win32":
                return os.path.join(path, "gn.bat"), os.path.join(path, "autoninja.bat")
            else:
                return os.path.join(path, "gn"), os.path.join(path, "autoninja")
    return "", ""

# return a list of tests and their params in the form
# [(test1, params1), (test2, params2),...]
def GetTestNamesAndParams(test_exec_path, filter="*"):
    command = '"' + test_exec_path + '" --gtest_list_tests --gtest_filter=' + filter
    p = SubProcess(command, False)
    returncode, output = p.BlockingRun()

    if returncode != 0:
        print(output)
        return []
    output_lines = output.splitlines()
    tests = []
    last_testcase_name = ""
    test_name_splitter = "# GetParam() ="
    for line in output_lines:
        if test_name_splitter in line:
            # must be a test name line
            test_name_and_params = line.split(test_name_splitter)
            tests.append((last_testcase_name + test_name_and_params[0].strip(), \
                test_name_and_params[1].strip()))
        else:
            # gtest_list_tests returns the test in this format
            # test case
            #    test name1
            #    test name2
            # Need to remember the last test case name to append to the test name
            last_testcase_name = line
    return tests



class Test():

    def __init__(self, full_test_name, params, use_goma):
        self.full_test_name = full_test_name
        self.params = params
        self.use_goma = use_goma
        self.capture_proc = None
        self.gn_proc = None
        self.autoninja_proc = None
        self.replay_proc = None

    def __str__(self):
        return self.full_test_name + " Params: " + self.params

    def Run(self, test_exe_path, trace_folder_path):
        ClearFolderContent(trace_folder_path)
        os.environ["ANGLE_CAPTURE_ENABLED"] = "1"
        command = '"' + test_exe_path + '" --gtest_filter=' + self.full_test_name
        self.capture_proc = SubProcess(command, False)
        return self.capture_proc.BlockingRun()

    def BuildReplay(self, gn_path, autoninja_path, build_dir, trace_dir, replay_exec):
        if not os.path.isfile(os.path.join(build_dir, "args.gn")):
            self.gn_proc = CreateGnGenSubProcess(
                gn_path, build_dir,
                [("use_goma", self.use_goma), ("angle_build_capture_replay_tests", "true"),
                 ("angle_capture_replay_test_trace_dir", '\\"' + trace_dir + '\\"'),
                 ("angle_with_capture_by_default", "false")])
            returncode, output = self.gn_proc.BlockingRun()
            if returncode != 0:
                return returncode, output

        self.autoninja_proc = CreateAutoninjaSubProcess(autoninja_path, build_dir, replay_exec)
        returncode, output = self.autoninja_proc.BlockingRun()
        if returncode != 0:
            return returncode, output
        return 0, "Built replay of " + self.full_test_name

    def RunReplay(self, replay_exe_path):
        os.environ["ANGLE_CAPTURE_ENABLED"] = "0"
        command = '"' + replay_exe_path + '"'
        self.replay_proc = SubProcess(command, False)
        return self.replay_proc.BlockingRun()

    def TerminateSubprocesses(self):
        if self.capture_proc and self.capture_proc.proc_handle.poll() == None:
            self.capture_proc.Kill()
        if self.gn_proc and self.gn_proc.proc_handle.poll() == None:
            self.gn_proc.Kill()
        if self.autoninja_proc and self.autoninja_proc.proc_handle.poll() == None:
            self.autoninja_proc.Kill()
        if self.replay_proc and self.replay_proc.proc_handle.poll() == None:
            self.replay_proc.Kill()


def ClearFolderContent(path):
    all_files = []
    for f in os.listdir(path):
        if os.path.isfile(os.path.join(path, f)) and f.startswith("angle_capture_context"):
            os.remove(os.path.join(path, f))


def CanRunReplay(path):
    required_trace_files = {
        "angle_capture_context1.h", "angle_capture_context1.cpp",
        "angle_capture_context1_files.txt"
    }
    required_trace_files_count = 0
    frame_files_count = 0
    for f in os.listdir(path):
        if not os.path.isfile(os.path.join(path, f)):
            continue
        if f in required_trace_files:
            required_trace_files_count += 1
        elif f.startswith("angle_capture_context1_frame"):
            frame_files_count += 1
        elif f.startswith("angle_capture_context") and not f.startswith("angle_capture_context1"):
            # if trace_files of another context exists, then the test creates multiple contexts
            return False
    # angle_capture_context1.angledata.gz can be missing
    return required_trace_files_count == len(required_trace_files) and frame_files_count >= 1


def SetCWDToAngleFolder():
    angle_folder = "angle"
    cwd = os.path.dirname(os.path.abspath(__file__))
    cwd = cwd.split(angle_folder)[0] + angle_folder
    os.chdir(cwd)
    return cwd


# RunTest gets run on each spawned subprocess.
# See https://chromium.googlesource.com/angle/angle/+/refs/heads/master/doc/CaptureAndReplay.md#testing for architecture
def RunTest(job_queue, gn_path, autoninja_path, capture_build_dir, replay_build_dir, test_exec,
            replay_exec, trace_dir, result_list):
    trace_folder_path = os.path.join(REPLAY_SAMPLE_FOLDER, trace_dir)
    test_exec_path = os.path.join(capture_build_dir, test_exec)
    replay_exec_path = os.path.join(replay_build_dir, replay_exec)

    os.environ["ANGLE_CAPTURE_OUT_DIR"] = trace_folder_path
    while not job_queue.empty():
        test = None
        try:
            test = job_queue.get()
            print("Running " + test.full_test_name)
            sys.stdout.flush()
            # stage 1 from the diagram linked above: capture run
            returncode, output = test.Run(test_exec_path, trace_folder_path)
            if returncode != 0 or not CanRunReplay(trace_folder_path):
                result_list.append((test.full_test_name, "Skipped",
                "Skipping replay since capture didn't produce appropriate files or has crashed. " \
                + "Error message: " + output))
                continue
            # stage 2 from the diagram linked above: replay build
            returncode, output = test.BuildReplay(gn_path, autoninja_path, replay_build_dir,
                                                  trace_dir, replay_exec)
            if returncode != 0:
                result_list.append(
                    (test.full_test_name, "Skipped",
                     "Skipping replay since failing to build replay. Error message: " + output))
                continue
            # stage 2 from the diagram linked above: replay run
            returncode, output = test.RunReplay(replay_exec_path)
            if returncode != 0:
                result_list.append((test.full_test_name, "Failed", output))
            else:
                result_list.append((test.full_test_name, "Passed", ""))
        except Exception:
            if test:
                test.TerminateSubprocesses()


def CreateReplayBuildFolders(folder_num, replay_build_dir):
    for i in range(folder_num):
        replay_build_dir_name = replay_build_dir + str(i)
        if os.path.isdir(replay_build_dir_name):
            shutil.rmtree(replay_build_dir_name)
        os.makedirs(replay_build_dir_name)


def SafeDeleteFolder(folder_name):
    while os.path.isdir(folder_name):
        try:
            shutil.rmtree(folder_name)
        except Exception:
            pass


def DeleteReplayBuildFolders(folder_num, replay_build_dir, trace_folder):
    for i in range(folder_num):
        folder_name = replay_build_dir + str(i)
        if os.path.isdir(folder_name):
            SafeDeleteFolder(folder_name)


def CreateTraceFolders(folder_num, trace_folder):
    for i in range(folder_num):
        folder_name = trace_folder + str(i)
        folder_path = os.path.join(REPLAY_SAMPLE_FOLDER, folder_name)
        if os.path.isdir(folder_path):
            shutil.rmtree(folder_path)
        os.makedirs(folder_path)


def DeleteTraceFolders(folder_num, trace_folder):
    for i in range(folder_num):
        folder_name = trace_folder + str(i)
        folder_path = os.path.join(REPLAY_SAMPLE_FOLDER, folder_name)
        if os.path.isdir(folder_path):
            SafeDeleteFolder(folder_path)


def main(capture_build_dir, replay_build_dir, use_goma, gtest_filter, test_exec):
    start_time = time.time()
    # set the number of workers to be cpu_count - 1 (since the main process already takes up a CPU
    # core). Whenever a worker is available, it grabs the next job from the job queue and runs it.
    # The worker closes down when there is no more job.
    worker_count = multiprocessing.cpu_count() - 1
    cwd = SetCWDToAngleFolder()
    trace_folder = "traces"
    if not os.path.isdir(capture_build_dir):
        os.makedirs(capture_build_dir)
    CreateReplayBuildFolders(worker_count, replay_build_dir)
    CreateTraceFolders(worker_count, trace_folder)

    replay_exec = "capture_replay_tests"
    if platform == "win32":
        test_exec += ".exe"
        replay_exec += ".exe"
    gn_path, autoninja_path = GetGnAndAutoninjaAbsolutePaths()
    if gn_path == "" or autoninja_path == "":
        print("No gn or autoninja found on system")
        return
    # generate gn files
    gn_proc = CreateGnGenSubProcess(gn_path, capture_build_dir,
                                    [("use_goma", use_goma),
                                     ("angle_with_capture_by_default", "true")], True)
    returncode, output = gn_proc.BlockingRun()
    if returncode != 0:
        return
    autoninja_proc = CreateAutoninjaSubProcess(autoninja_path, capture_build_dir, test_exec, True)
    returncode, output = autoninja_proc.BlockingRun()
    if returncode != 0:
        return
    # get a list of tests
    test_names_and_params = GetTestNamesAndParams(
        os.path.join(capture_build_dir, test_exec), gtest_filter)

    # objects created by manager can be shared by multiple processes. We use it to create
    # collections that are shared by multiple processes such as job queue or result list.
    manager = multiprocessing.Manager()
    job_queue = manager.Queue()
    for test_name_and_params in test_names_and_params:
        job_queue.put(Test(test_name_and_params[0], test_name_and_params[1], use_goma))

    environment_vars = [("ANGLE_CAPTURE_FRAME_END", "100"), ("ANGLE_CAPTURE_SERIALIZE_STATE", "1")]
    for environment_var in environment_vars:
        os.environ[environment_var[0]] = environment_var[1]

    passed_count = 0
    failed_count = 0
    skipped_count = 0
    failed_tests = []

    # result list is created by manager and can be shared by multiple processes. Each subprocess
    # populates the result list with the results of its test runs. After all subprocesses finish,
    # the main process processes the results in the result list.
    # An item in the result list is a tuple with 3 values (testname, result, output).
    # The "result" can take 3 values "Passed", "Failed", "Skipped". The output is the stdout and
    # the stderr of the test appended together.
    result_list = manager.list()
    workers = []
    for i in range(worker_count):
        proc = multiprocessing.Process(
            target=RunTest,
            args=(job_queue, gn_path, autoninja_path, capture_build_dir, replay_build_dir + str(i),
                  test_exec, replay_exec, trace_folder + str(i), result_list))
        workers.append(proc)
        proc.start()

    for worker in workers:
        worker.join()

    for environment_var in environment_vars:
        del os.environ[environment_var[0]]
    end_time = time.time()

    print("\n\n\n")
    print("Results:")
    for result in result_list:
        output_string = result[1] + ": " + result[0] + ". "
        if result[1] == "Skipped":
            output_string += result[2]
            skipped_count += 1
        elif result[1] == "Failed":
            output_string += result[2]
            failed_tests.append(result[0])
            failed_count += 1
        else:
            passed_count += 1
        print(output_string)

    print("\n\n")
    print("Elapsed time: " + str(end_time - start_time) + " seconds")
    print("Passed: "+ str(passed_count) + " Failed: " + str(failed_count) + \
    " Skipped: " + str(skipped_count))
    print("Failed tests:")
    for failed_test in failed_tests:
        print("\t" + failed_test)
    DeleteTraceFolders(worker_count, trace_folder)
    DeleteReplayBuildFolders(worker_count, replay_build_dir, trace_folder)
    if os.path.isdir(capture_build_dir):
        SafeDeleteFolder(capture_build_dir)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--capture_build_dir', default=DEFAULT_CAPTURE_BUILD_DIR)
    parser.add_argument('--replay_build_dir', default=DEFAULT_REPLAY_BUILD_DIR)
    parser.add_argument('--use_goma', default="false")
    parser.add_argument('--gtest_filter', default=DEFAULT_FILTER)
    parser.add_argument('--test_suite', default=DEFAULT_TEST_SUITE)
    args = parser.parse_args()
    main(args.capture_build_dir, args.replay_build_dir, args.use_goma, args.gtest_filter,
         args.test_suite)
