#! /usr/bin/env python3
#
# Copyright 2021 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
'''
Pixel 6 (ARM based) specific script that measures the following for each restricted_trace:
- Wall time per frame
- GPU time per frame (if run with --gpu-time)
- CPU time per frame
- GPU power per frame
- CPU power per frame
- GPU memory per frame
- CPU instructions per frame (if run with --cpu-inst)

Setup:

  autoninja -C out/<config> angle_trace_perf_tests angle_apks

Recommended command to run:

  out/<config>/restricted_trace_perf --fixedtime 10 --power --memory --output-tag android.$(date '+%Y%m%d') --loop-count 5

Alternatively, you can pass the build directory and run from anywhere:

  python3 restricted_trace_perf.py --fixedtime 10 --power --output-tag android.$(date '+%Y%m%d') --loop-count 5 --build-dir ../../../out/<config>

- This will run through all the traces 5 times with the native driver, then 5 times with vulkan (via ANGLE)
- 10 second run time with one warmup loop

Output will be printed to the terminal as it is collected.

Of the 5 runs, the high and low for each data point will be dropped, average of the remaining three will be tracked in the summary spreadsheet.

If you only want to generate a summary from existing raw_data, you can invoke the script like this:

  vpython3 restricted_trace_perf.py --summarize-only raw_data.foo.csv summary.foo.csv

'''

import argparse
import contextlib
import copy
import csv
import fnmatch
import json
import logging
import os
import pathlib
import posixpath
import re
import statistics
import subprocess
import sys
import threading
import time

from collections import defaultdict, namedtuple
from datetime import datetime

PY_UTILS = str(pathlib.Path(__file__).resolve().parents[1] / 'py_utils')
if PY_UTILS not in sys.path:
    os.stat(PY_UTILS) and sys.path.insert(0, PY_UTILS)
import android_helper
import angle_path_util
import angle_test_util

DEFAULT_TEST_DIR = '.'
DEFAULT_LOG_LEVEL = 'info'
DEFAULT_ANGLE_PACKAGE = 'com.android.angle.test'

Result = namedtuple('Result', ['stdout', 'stderr', 'time'])


class _global(object):
    current_user = None
    storage_dir = None
    cache_dir = None


def init():
    _global.current_user = run_adb_shell_command('am get-current-user').strip()
    _global.storage_dir = '/data/user/' + _global.current_user + '/com.android.angle.test/files'
    _global.cache_dir = '/data/user_de/' + _global.current_user + '/com.android.angle.test/cache'
    logging.debug('Running with user %s, storage %s, cache %s', _global.current_user,
                  _global.storage_dir, _global.cache_dir)


def run_async_adb_shell_command(cmd):
    logging.debug('Kicking off subprocess %s' % (cmd))

    try:
        async_process = subprocess.Popen([android_helper.FindAdb(), 'shell', cmd],
                                         stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(
            e.cmd, e.returncode, e.output))

    logging.debug('Done launching subprocess')

    return async_process


def run_adb_command(args):
    return android_helper._AdbRun(args).decode()


def run_adb_shell_command(cmd):
    return android_helper._AdbShell(cmd).decode()


def run_adb_shell_command_with_run_as(cmd):
    return android_helper._AdbShellWithRunAs(cmd).decode()


def run_async_adb_command(args):
    return run_async_command('adb ' + args)


def cleanup():
    run_adb_shell_command_with_run_as('rm -f ' + _global.storage_dir + '/out.txt ' +
                                      _global.storage_dir + '/gpumem.txt')


def clear_blob_cache():
    run_adb_shell_command_with_run_as('rm -rf ' + _global.cache_dir)


def select_device(device_arg):
    # The output from 'adb devices' always includes a header and a new line at the end.
    result_dev_out = run_adb_command(['devices']).strip()

    result_header_end = result_dev_out.find('\n')
    result_dev_out = '' if result_header_end == -1 else result_dev_out[result_header_end:]
    result_dev_out = result_dev_out.split()[0::2]

    def print_device_list():
        print('\nList of available devices:\n{}'.format('\n'.join(result_dev_out)))

    num_connected_devices = len(result_dev_out)

    # Check the device arg first. If there is none, use the ANDROID SERIAL env var.
    android_serial_env = os.environ.get('ANDROID_SERIAL')
    device_serial = device_arg if device_arg != '' else android_serial_env

    # Check for device exceptions
    if num_connected_devices == 0:
        logging.error('DeviceError: No devices detected. Please connect a device and try again.')
        exit()

    if num_connected_devices > 1 and device_serial is None:
        logging.error(
            'DeviceError: More than one device detected. Please specify a target device\n'
            'through either the --device option or $ANDROID_SERIAL, and try again.')
        print_device_list()
        exit()

    if device_serial is not None and device_serial not in result_dev_out:
        logging.error('DeviceError: Device with serial {} not detected.'.format(device_serial))
        if device_arg != '':
            logging.error('Please update the --device input and try again.')
        else:
            logging.error('Please update $ANDROID_SERIAL and try again.')
        print_device_list()
        exit()

    # Select device
    if device_serial is not None:
        logging.info('Device with serial {} selected.'.format(device_serial))
        os.environ['ANDROID_SERIAL'] = device_serial

    else:
        logging.info('Default device ({}) selected.'.format(result_dev_out[0]))


def get_mode(args):
    mode = ''
    if args.vsync:
        mode = 'vsync'
    elif args.offscreen:
        mode = 'offscreen'

    return mode


def get_trace_width(mode):
    width = 40
    if mode == 'vsync':
        width += 5
    elif mode == 'offscreen':
        width += 10

    return width


def pull_screenshot(args, screenshot_device_dir, renderer):
    # We don't know the name of the screenshots, since they could have a KeyFrame
    # Rather than look that up and piece together a file name, we can pull the single screenshot in a generic fashion
    files = run_adb_shell_command('ls -1 %s' % screenshot_device_dir).split('\n')

    # There might not be a screenshot if the test was skipped
    files = list(filter(None, (f.strip() for f in files)))  # Remove empty strings and whitespace

    # We should only look for png files as tmp dir might contain other files too
    png_files = [f for f in files if f.lower().endswith('.png')]
    if png_files:
        assert len(png_files) == 1, 'Multiple PNG files(%s) in %s, expected 1: %s' % (
            len(png_files), screenshot_device_dir, png_files)

        # Grab the single screenshot
        src_file = png_files[0]

        # Rename the file to reflect renderer, since we force everything through the platform using "native"
        dst_file = src_file.replace("native", renderer)

        # Use CWD if no location provided
        screenshot_dst = args.screenshot_dir if args.screenshot_dir != '' else '.'

        logging.info('Pulling screenshot %s to %s' % (dst_file, screenshot_dst))
        run_adb_command([
            'pull',
            posixpath.join(screenshot_device_dir, src_file),
            posixpath.join(screenshot_dst, dst_file)
        ])


# This function changes to the target directory, then 'yield' passes execution to the inner part of
# the 'with' block that invoked it. The 'finally' block is executed at the end of the 'with' block,
# including when exceptions are raised.
@contextlib.contextmanager
def run_from_dir(dir):
    # If not set, just run the command and return
    if not dir:
        yield
        return
    # Otherwise, change directories
    cwd = os.getcwd()
    os.chdir(dir)
    try:
        yield
    finally:
        os.chdir(cwd)


def run_trace(trace, args, screenshot_device_dir):
    mode = get_mode(args)

    # Kick off a subprocess that collects peak gpu memory periodically
    # Note the 0.25 below is the delay (in seconds) between memory checks
    if args.memory:
        run_adb_command([
            'push',
            os.path.join(angle_path_util.ANGLE_ROOT_DIR, 'src', 'tests', 'restricted_traces',
                         'gpumem.sh'), '/data/local/tmp'
        ])
        memory_command = 'sh /data/local/tmp/gpumem.sh 0.25 ' + _global.storage_dir
        memory_process = run_async_adb_shell_command(memory_command)

    flags = [
        '--gtest_filter=TraceTest.' + trace, '--use-gl=native', '--verbose', '--verbose-logging'
    ]
    if mode != '':
        flags.append('--' + mode)
    if args.maxsteps != '':
        flags += ['--max-steps-performed', args.maxsteps]
    if args.run_to_key_frame:
        flags.append('--run-to-key-frame')
    if args.fixedtime != '':
        flags += ['--fixed-test-time-with-warmup', args.fixedtime]
    if args.minimizegpuwork:
        flags.append('--minimize-gpu-work')
    if args.skip_blit_in_offscreen:
        flags.append('--skip-blit-in-offscreen')
    if screenshot_device_dir != None:
        flags += ['--screenshot-dir', screenshot_device_dir]
    if args.screenshot_frame != '':
        flags += ['--screenshot-frame', args.screenshot_frame]
    if args.fps_limit != '':
        flags += ['--fps-limit', args.fps_limit]
    if args.fps_limit_uses_busy_wait:
        flags.append('--fps-limit-uses-busy-wait')
    if args.gpu_time:
        # Keep internal argument name the same for compatibility with the C++ executable
        flags.append('--track-gpu-time')
    if args.add_swap_into_gpu_time:
        flags.append('--add-swap-into-gpu-time')
    if args.add_swap_into_frame_wall_time:
        flags.append('--add-swap-into-frame-wall-time')

    # Build a command that can be run directly over ADB, for example:
    r'''
adb shell am instrument -w \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.StdoutFile /data/user/0/com.android.angle.test/files/out.txt \
    -e org.chromium.native_test.NativeTest.CommandLineFlags \
    "--gtest_filter=TraceTest.empires_and_puzzles\ --use-angle=vulkan\ --screenshot-dir\ /data/user/0/com.android.angle.test/files\ --screenshot-frame\ 2\ --max-steps-performed\ 2\ --no-warmup" \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.ShardNanoTimeout "1000000000000000000" \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.NativeTestActivity com.android.angle.test.AngleUnitTestActivity \
    com.android.angle.test/org.chromium.build.gtest_apk.NativeTestInstrumentationTestRunner
    '''
    shell_command = r'''
am instrument -w \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.StdoutFile {storage}/out.txt \
    -e org.chromium.native_test.NativeTest.CommandLineFlags "{flags}" \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.ShardNanoTimeout "1000000000000000000" \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.NativeTestActivity \
    com.android.angle.test.AngleUnitTestActivity \
    com.android.angle.test/org.chromium.build.gtest_apk.NativeTestInstrumentationTestRunner
    '''.format(
        flags=r' '.join(flags),
        storage=_global.storage_dir).strip()  # Note: space escaped due to subprocess shell=True

    start_time = time.time()
    result = run_adb_shell_command(shell_command)
    time_elapsed = time.time() - start_time

    if args.memory:
        logging.debug('Killing gpumem subprocess')
        memory_process.kill()

    return time_elapsed


def get_test_time():
    # Pull the results from the device and parse
    result = run_adb_shell_command_with_run_as('cat ' + _global.storage_dir +
                                               '/out.txt | grep -v Error | grep -v Frame')

    measured_time = None

    for line in result.splitlines():
        logging.debug('Checking line: %s' % line)

        # Look for this line and grab the second to last entry:
        #   Mean result time: 1.2793 ms
        if "Mean result time" in line:
            measured_time = line.split()[-2]
            break

        # Check for skipped tests
        if "Test skipped due to missing extension" in line:
            missing_ext = line.split()[-1]
            logging.debug('Skipping test due to missing extension: %s' % missing_ext)
            measured_time = missing_ext
            break

    if measured_time is None:
        if '[  PASSED  ]' in result:
            measured_time = 'missing'
        else:
            measured_time = 'crashed'

    return measured_time


def get_gpu_memory(trace_duration):
    # Pull the results from the device and parse
    result = run_adb_shell_command_with_run_as('cat ' + _global.storage_dir +
                                               '/gpumem.txt | awk "NF"')

    # The gpumem script grabs snapshots of memory per process
    # Output looks like this, repeated once per sleep_duration of the test:
    #
    # time_elapsed: 9
    # com.android.angle.test:test_process 16513
    # Memory snapshot for GPU 0:
    # Global total: 516833280
    # Proc 504 total: 170385408
    # Proc 1708 total: 33767424
    # Proc 2011 total: 17018880
    # Proc 16513 total: 348479488
    # Proc 27286 total: 20877312
    # Proc 27398 total: 1028096

    # Gather the memory at each snapshot
    time_elapsed = ''
    test_process = ''
    gpu_mem = []
    gpu_mem_sustained = []
    for line in result.splitlines():
        logging.debug('Checking line: %s' % line)

        if "time_elapsed" in line:
            time_elapsed = line.split()[-1]
            logging.debug('time_elapsed: %s' % time_elapsed)
            continue

        # Look for this line and grab the last entry:
        #   com.android.angle.test:test_process 31933
        if "com.android.angle.test" in line:
            test_process = line.split()[-1]
            logging.debug('test_process: %s' % test_process)
            continue

        # If we don't know test process yet, break
        if test_process == '':
            continue

        # If we made it this far, we have the process id

        # Look for this line and grab the last entry:
        #   Proc 31933 total: 235184128
        if test_process in line:
            gpu_mem_entry = line.split()[-1]
            logging.debug('Adding: %s to gpu_mem' % gpu_mem_entry)
            gpu_mem.append(int(gpu_mem_entry))
            # logging.debug('gpu_mem contains: %i' % ' '.join(gpu_mem))
            if safe_cast_float(time_elapsed) >= (safe_cast_float(trace_duration) / 2):
                # Start tracking sustained memory usage at the half way point
                logging.debug('Adding: %s to gpu_mem_sustained' % gpu_mem_entry)
                gpu_mem_sustained.append(int(gpu_mem_entry))
            continue

    gpu_mem_max = 0
    if len(gpu_mem) != 0:
        gpu_mem_max = max(gpu_mem)

    gpu_mem_average = 0
    if len(gpu_mem_sustained) != 0:
        gpu_mem_average = safe_cast_int(statistics.mean(gpu_mem_sustained))

    return gpu_mem_average, gpu_mem_max


def get_proc_memory():
    # Pull the results from the device and parse
    result = run_adb_shell_command_with_run_as('cat ' + _global.storage_dir + '/out.txt')
    memory_median = ''
    memory_max = ''

    for line in result.splitlines():
        # Look for "memory_max" in the line and grab the second to last entry:
        logging.debug('Checking line: %s' % line)

        if "memory_median" in line:
            memory_median = line.split()[-2]
            continue

        if "memory_max" in line:
            memory_max = line.split()[-2]
            continue

    return safe_cast_int(memory_max), safe_cast_int(memory_median)


def get_gpu_time():
    # Pull the results from the device and parse
    result = run_adb_shell_command_with_run_as('cat ' + _global.storage_dir + '/out.txt')
    gpu_time = '0'

    for line in result.splitlines():
        # Look for "gpu_time" in the line and grab the second to last entry:
        logging.debug('Checking line: %s' % line)

        if "gpu_time" in line:
            gpu_time = line.split()[-2]
            break

    return gpu_time


def get_frame_wall_time():
    # Pull the results from the device and parse
    result = run_adb_shell_command_with_run_as('cat ' + _global.storage_dir + '/out.txt')
    frame_wall_time = '0'

    for line in result.splitlines():
        # Look for "frame_wall_time" in the line and grab the second to last entry:
        logging.debug('Checking line: %s' % line)

        if "frame_wall_time" in line:
            frame_wall_time = line.split()[-2]
            break

    return frame_wall_time


def get_cpu_time():
    # Pull the results from the device and parse
    result = run_adb_shell_command_with_run_as('cat ' + _global.storage_dir + '/out.txt')
    cpu_time = '0'

    for line in result.splitlines():
        # Look for "cpu_time" in the line and grab the second to last entry:
        logging.debug('Checking line: %s' % line)

        if "cpu_time" in line:
            cpu_time = line.split()[-2]
            break

    return cpu_time


def get_frame_count():
    # Pull the results from the device and parse
    result = run_adb_shell_command_with_run_as('cat ' + _global.storage_dir +
                                               '/out.txt | grep -v Error | grep -v Frame')

    frame_count = 0

    for line in result.splitlines():
        logging.debug('Checking line: %s' % line)
        if "trial_steps" in line:
            frame_count = line.split()[-2]
            break

    logging.debug('Frame count: %s' % frame_count)
    return frame_count


class GPUPowerStats():

    # GPU power measured in uWs

    def __init__(self):
        self.power = {
            # GPU rails
            'gpu': 0,
            # CPU rails
            'big_cpu': 0,
            'mid_cpu': 0,
            'little_cpu': 0,
            # Infra rails
            'cpu_m': 0,
            'gpu_m': 0,
            'infra1': 0,
            'infra2': 0,
            'infra3': 0,
            'infra4': 0
        }

    def gpu_delta(self, starting):
        gpu = self.power['gpu'] - starting.power['gpu']
        gpu_m = self.power['gpu_m'] - starting.power['gpu_m']
        return gpu + gpu_m

    def cpu_delta(self, starting):
        big = self.power['big_cpu'] - starting.power['big_cpu']
        mid = self.power['mid_cpu'] - starting.power['mid_cpu']
        little = self.power['little_cpu'] - starting.power['little_cpu']
        cpu_m = self.power['cpu_m'] - starting.power['cpu_m']
        return big + mid + little + cpu_m

    def infra_delta(self, starting):
        infra1 = self.power['infra1'] - starting.power['infra1']
        infra2 = self.power['infra2'] - starting.power['infra2']
        infra3 = self.power['infra3'] - starting.power['infra3']
        infra4 = self.power['infra4'] - starting.power['infra4']
        return infra1 + infra2 + infra3 + infra4


    def get_power_data(self):
        energy_value_result = run_adb_shell_command(
            'cat /sys/bus/iio/devices/iio:device*/energy_value')
        # Output like this (ordering doesn't matter)
        #
        # t=251741617
        # CH0(T=251741617)[VSYS_PWR_WLAN_BT], 192612469095
        # CH1(T=251741617)[L2S_VDD_AOC_RET], 1393792850
        # CH2(T=251741617)[S9S_VDD_AOC], 16816975638
        # CH3(T=251741617)[S5S_VDDQ_MEM], 2720913855
        # CH4(T=251741617)[S10S_VDD2L], 3656592412
        # CH5(T=251741617)[S4S_VDD2H_MEM], 4597271837
        # CH6(T=251741617)[S2S_VDD_G3D], 3702041607
        # CH7(T=251741617)[L9S_GNSS_CORE], 88535064
        # t=16086645
        # CH0(T=16086645)[PPVAR_VSYS_PWR_DISP], 611687448
        # CH1(T=16086645)[VSYS_PWR_MODEM], 179646995
        # CH2(T=16086645)[VSYS_PWR_RFFE], 0
        # CH3(T=16086645)[S2M_VDD_CPUCL2], 124265856
        # CH4(T=16086645)[S3M_VDD_CPUCL1], 170096352
        # CH5(T=16086645)[S4M_VDD_CPUCL0], 289995530
        # CH6(T=16086645)[S5M_VDD_INT], 190164699
        # CH7(T=16086645)[S1M_VDD_MIF], 196512891

        id_map = {
            r'S2S_VDD_G3D\b|S2S_VDD_GPU\b': 'gpu',
            r'S\d+M_VDD_CPUCL2\b|S2M_VDD_CPU2\b': 'big_cpu',
            r'S\d+M_VDD_CPUCL1\b|S3M_VDD_CPU1\b': 'mid_cpu',
            r'S\d+M_VDD_CPUCL0\b|S4M_VDD_CPU\b': 'little_cpu',
            r'S6M_LLDO1\b': 'cpu_m',
            r'S10S_VDD_INFRA_MM_GPU_M\b': 'gpu_m',
            r'S4S_VDD2H_MEM\b': 'infra1',
            r'S5S_VDDQ_MEM\b': 'infra2',
            r'S8S_VDD_GMC\b': 'infra3',
            r'S9S_VDD_INFRA\b': 'infra4',
        }

        for m in id_map.values():
            self.power[m] = 0  # Set to 0 to check for missing values and dupes below

        for line in energy_value_result.splitlines():
            for mid, m in id_map.items():
                if re.search(mid, line):
                    value = int(line.split()[1])
                    logging.debug('Power metric %s (%s): %d', mid, m, value)
                    assert self.power[m] == 0, 'Duplicate power metric: %s (%s)' % (mid, m)
                    self.power[m] = value

        for mid, m in id_map.items():
            assert self.power[m] != 0, 'Power metric not found: %s (%s)' % (mid, m)


def wait_for_test_warmup(done_event):
    p = subprocess.Popen(['adb', 'logcat', '*:S', 'ANGLE:I'],
                         stdout=subprocess.PIPE,
                         text=True,
                         bufsize=1)  # line-buffered
    os.set_blocking(p.stdout.fileno(), False)

    start_time = time.time()
    while True:
        line = p.stdout.readline()  # non-blocking as per set_blocking above

        # Look for text logged by the harness when warmup is complete and a test is starting
        if 'running test name' in line:
            p.kill()
            break
        if done_event.is_set():
            logging.warning('Test finished without logging to logcat')
            p.kill()
            break

        time.sleep(0.05)

        p.poll()
        if p.returncode != None:
            logging.warning('Logcat terminated unexpectedly')
            return


def collect_cpu_inst(done_event, test_fixedtime, results):
    try:
        tmp_data_file = run_adb_shell_command('mktemp /data/local/tmp/tmp.XXXXXX').strip()

        # Starting point is post test warmup as there are spikes during warmup
        wait_for_test_warmup(done_event)

        if done_event.is_set():
            logging.debug('Test finished earlier than expected by collect_cpu_inst')
            return

        # Start simpleperf record right after warmup is done
        logging.debug('Starting cpu instruction count')

        # Collect cpu instructions of "com.android.angle.test:test_process"
        run_adb_shell_command(f'''simpleperf record \
            -o {tmp_data_file} \
            -e instructions:u \
            -f 4000 \
            --app com.android.angle.test''')

        # Get the start time of simple perf to filter simpleperf data file
        start_ns = run_adb_shell_command(f'''simpleperf report-sample \
            -i {tmp_data_file} \
            | sed -n '/^[[:space:]]*time/p;' | sort -n | head -n 1 ''')
        start_ns = safe_cast_int(start_ns.strip().split()[-1])

        done_event.wait(timeout=10)
        if not done_event.is_set():
            logging.debug('Test finished but progress is stuck')
            return

        # Calculate actual test running time based on frame count and wall time
        frame_count = get_frame_count()
        wall_time = get_test_time()
        # Compensate 0.5ms for the delay before starting test
        execution_time_ms = safe_cast_float(frame_count) * safe_cast_float(wall_time) + 0.5
        end_ns = safe_cast_int(start_ns + execution_time_ms * 1e6)

        # Filter simpleperf record within actual test running time
        temp_filter_file = run_adb_shell_command('mktemp /data/local/tmp/tmp.XXXXXX').strip()

        # Build filter content separately to avoid clang-format issues
        filter_content = ("CLOCK monotonic\n"
                          f"GLOBAL_BEGIN {start_ns}\n"
                          f"GLOBAL_END {end_ns}")
        run_adb_shell_command(f'echo "{filter_content}" > {temp_filter_file}')

        perf_output = run_adb_shell_command(f'''simpleperf report \
            --sort dso \
            --print-event-count \
            --filter-file {temp_filter_file} \
            -i {tmp_data_file}''')

        perf_output = perf_output.split('\n')

        for line in perf_output:
            if 'Event count:' in line:
                results['Total'] = safe_cast_float(line.split()[-1].strip())

            if re.search('libGLES.*_.*\.so', line):
                if ('angle' in line):
                    results['angle_lib'] = safe_cast_float(line.split()[-2].strip())
                else:
                    results['gles_lib'] = safe_cast_float(line.split()[-2].strip())

            if re.search('vulkan\..*\.so', line):
                results['vulkan_lib'] = safe_cast_float(line.split()[-2].strip())

            if 'libc.so' in line:
                results['libc_lib'] = safe_cast_float(line.split()[-2].strip())
    finally:
        run_adb_shell_command(f'rm -f {tmp_data_file}')
        run_adb_shell_command(f'rm -f {temp_filter_file}')
        logging.debug('Ending cpu instruction count')

def collect_power(done_event, test_fixedtime, results):
    # Starting point is post test warmup as there are spikes during warmup
    wait_for_test_warmup(done_event)

    starting_power = GPUPowerStats()
    starting_power.get_power_data()
    logging.debug('Starting power: %s' % starting_power.power)

    duration = test_fixedtime - 1.0  # Avoid measuring through test termination
    start_time = time.time()
    while time.time() - start_time < duration:
        if done_event.is_set():
            logging.warning('Test finished earlier than expected by collect_power')
            break
        time.sleep(0.05)

    ending_power = GPUPowerStats()
    ending_power.get_power_data()
    dt = time.time() - start_time
    logging.debug('Ending power: %s' % ending_power.power)

    results.update({
        # 1e6 for uW -> W
        'cpu': ending_power.cpu_delta(starting_power) / dt / 1e6,
        'gpu': ending_power.gpu_delta(starting_power) / dt / 1e6,
        'infra': ending_power.infra_delta(starting_power) / dt / 1e6,
    })


def get_thermal_info():
    out = run_adb_shell_command('dumpsys android.hardware.thermal.IThermal/default')
    result = []
    for line in out.splitlines():
        logging.debug('Checking line in get_thermal_info: %a', line)
        if 'ThrottlingStatus:' in line:
            name = re.search('Name: ([^ ]*)', line).group(1)
            if ('VIRTUAL-SKIN' in name and
                    '-CHARGE' not in name and  # only supposed to affect charging speed
                    'VIRTUAL-SKIN-EQ' not in name and  # inaccurate data
                    'MODEL' not in name.split('-')):  # different units and not used for throttling
                result.append(line)

    if not result:
        logging.error('Unexpected dumpsys IThermal response:\n%s', out)
        raise RuntimeError('Unexpected dumpsys IThermal response, logged above')
    return result


def set_vendor_thermal_control(disabled=None):
    if disabled:
        # When disabling, first wait for vendor throttling to end to reset all state
        waiting = True
        while waiting:
            waiting = False
            for line in get_thermal_info():
                if 'ThrottlingStatus: NONE' not in line:
                    logging.info('Waiting for vendor throttling to finish: %s', line.strip())
                    time.sleep(10)
                    waiting = True
                    break

    run_adb_shell_command('setprop persist.vendor.disable.thermal.control %d' % disabled)


def sleep_until_temps_below(limit_temp):
    waiting = True
    while waiting:
        waiting = False
        for line in get_thermal_info():
            v = float(re.search('CurrentValue: ([^ ]*)', line).group(1))
            if v > limit_temp:
                logging.info('Waiting for device temps below %.1f: %s', limit_temp, line.strip())
                time.sleep(5)
                waiting = True
                break


def sleep_until_temps_below_thermalservice(limit_temp):
    waiting = True
    while waiting:
        waiting = False
        lines = run_adb_shell_command('dumpsys thermalservice').splitlines()
        assert 'HAL Ready: true' in lines
        for l in lines[lines.index('Current temperatures from HAL:') + 1:]:
            if 'Temperature{' not in l:
                break
            if 'mName=thb_hda' in l:  # This value isn't meaningful
                break
            v = re.search(r'mValue=([^,}]+)', l).group(1)
            # Note: on some Pixel devices odd component temps are also reported here
            # but some other key ones are not (e.g. CPU ODPM controlling cpu freqs)
            if float(v) > limit_temp:
                logging.info('Waiting for device temps below %.1f: %s', limit_temp, l.strip())
                time.sleep(5)
                waiting = True
                break


def sleep_until_battery_level(min_battery_level):
    while True:
        level = int(run_adb_shell_command('dumpsys battery get level').strip())
        if level >= min_battery_level:
            break
        logging.info('Waiting for device battery level to reach %d. Current level: %d',
                     min_battery_level, level)
        time.sleep(10)


def drop_high_low_and_average(values):
    # Get rid of any unique outliers, requires high loop count (suggest 10)
    if len(values) > 5:
        values.remove(min(values))
        values.remove(max(values))

        # Drop values that are more than 50% off the fastest time.
        # Check against zero in case negative (error code) values come through
        values.sort()
        original_values = list(values)  # Keep a copy of the original values
        if values[0] >= 0:
            fastest_time = values[0]
            threshold = fastest_time * 1.5
            values = [v for v in values if v <= threshold]

        if len(original_values) != len(values):
            logging.info('Dropped unstable values. Original: %s, After: %s', original_values,
                         values)

    # Now drop high and low
    if len(values) >= 3:
        values.remove(min(values))
        values.remove(max(values))

    average = statistics.mean(values)

    variance = 0
    if len(values) >= 2 and average != 0:
        variance = statistics.stdev(values) / average

    print(average, variance)

    return float(average), float(variance)


def get_angle_version():
    angle_version = android_helper._Run('git rev-parse HEAD'.split(' ')).decode().strip()
    origin_main_version = android_helper._Run(
        'git rev-parse origin/main'.split(' ')).decode().strip()
    if origin_main_version != angle_version:
        angle_version += ' (origin/main %s)' % origin_main_version
    return angle_version


def safe_divide(x, y):
    if y == 0:
        return 0
    return x / y


def safe_cast_float(x):
    if x == '':
        return 0
    try:
        float_value = float(x)
        return float_value
    except ValueError:  # e.g. 'crashed'
        return -1

def safe_cast_int(x):
    if x == '':
        return 0
    return int(x)


def percent(x):
    return "%.2f%%" % (x * 100)


def logged_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-tag')
    _, extra_args = parser.parse_known_args()
    return ' '.join(extra_args)


def get_summary_name(args):
    if args.output_tag:
        return "summary." + args.output_tag + ".csv"
    else:
        return ''


def get_raw_data_name(args):
    if args.output_tag:
        return "raw_data." + args.output_tag + ".csv"
    else:
        return ''

def run_traces(args):
    # Load trace names
    test_json = os.path.join(args.build_dir, 'gen/trace_list.json')
    with open(os.path.join(DEFAULT_TEST_DIR, test_json)) as f:
        traces = json.loads(f.read())

    failures = []

    mode = get_mode(args)
    trace_width = get_trace_width(mode)

    # Select the target device
    select_device(args.device)

    # Add an underscore to the mode for use in loop below
    if mode != '':
        mode = mode + '_'

    # Create output CSV
    raw_data_filename = get_raw_data_name(args)
    output_file = ''
    output_writer = ''
    if raw_data_filename != '':
        output_file = open(raw_data_filename, 'w', newline='')
        output_writer = csv.writer(output_file)

    android_version = run_adb_shell_command('getprop ro.build.fingerprint').strip()
    angle_version = args.angle_version or get_angle_version()
    # test_time = run_command('date \"+%Y%m%d\"').stdout.read().strip()

    if output_writer:
        output_writer.writerow([
            "\"Android: " + android_version + "\n" + "ANGLE: " + angle_version + "\n" +
            #  "Date: " + test_time + "\n" +
            "Source: " + raw_data_filename + "\n" + "Args: " + logged_args() + "\""
        ])

    # Set some widths that allow easily reading the values, but fit on smaller monitors.
    column_width = {
        'trace': trace_width,
        'wall_time': 15,
        'gpu_time': 15,
        'frame_wall_time': 15,
        'cpu_time': 15,
        'gpu_power': 10,
        'cpu_power': 10,
        'infra_power': 10,
        'gpu_mem_sustained': 20,
        'gpu_mem_peak': 15,
        'proc_mem_median': 20,
        'proc_mem_peak': 15,
        'process_cpuinst': 20,
        'gfxlib_cpuinst': 20,
        'angle_cpuinst': 20,
        'vulkan_cpuinst': 20,
        'gles_cpuinst': 20,
        'libc_cpuinst': 20,
    }

    headers_map = {
        'trace': 'trace',
        'wall_time': 'wall_time(ms)',
        'gpu_time': 'gpu_time(ms)',
        'frame_wall_time': 'frame_wall_time(ms)',
        'cpu_time': 'cpu_time(ms)',
        'gpu_power': 'gpu_power(W)',
        'cpu_power': 'cpu_power(W)',
        'infra_power': 'infra_power(W)',
        'gpu_mem_sustained': 'gpu_mem_sustained',
        'gpu_mem_peak': 'gpu_mem_peak',
        'proc_mem_median': 'proc_mem_median',
        'proc_mem_peak': 'proc_mem_peak',
        'process_cpuinst': 'process_cpuinst',
        'gfxlib_cpuinst': 'gfxlib_cpuinst',
        'angle_cpuinst': 'angle_cpuinst',
        'vulkan_cpuinst': 'vulkan_cpuinst',
        'gles_cpuinst': 'gles_cpuinst',
        'libc_cpuinst': 'libc_cpuinst'
    }

    # Dynamically build the list of columns to output based on command line flags
    output_columns = ['trace']
    if args.walltimeonly:
        output_columns.append('wall_time')
    elif args.minimal_output:
        output_columns.append('wall_time')
        if args.gpu_time:
            output_columns.append('gpu_time')

        output_columns.extend(['frame_wall_time', 'cpu_time'])

        if args.power:
            output_columns.extend(['gpu_power', 'cpu_power', 'infra_power'])
        if args.memory:
            output_columns.extend(
                ['gpu_mem_sustained', 'gpu_mem_peak', 'proc_mem_median', 'proc_mem_peak'])
        if args.cpu_inst:
            output_columns.extend([
                'process_cpuinst', 'gfxlib_cpuinst', 'angle_cpuinst', 'vulkan_cpuinst',
                'gles_cpuinst', 'libc_cpuinst'
            ])
    else:
        # Output all columns when minimal-output is not requested
        output_columns.extend([
            'wall_time', 'gpu_time', 'frame_wall_time', 'cpu_time', 'gpu_power', 'cpu_power',
            'infra_power', 'gpu_mem_sustained', 'gpu_mem_peak', 'proc_mem_median', 'proc_mem_peak',
            'process_cpuinst', 'gfxlib_cpuinst', 'angle_cpuinst', 'vulkan_cpuinst', 'gles_cpuinst',
            'libc_cpuinst'
        ])

    # Format string that easily sizes itself to the dynamically loaded variables
    format_str = ' '.join(['%-*s'] * len(output_columns))

    print_header_args = []
    for col in output_columns:
        print_header_args.extend([column_width[col], col])

    print(format_str % tuple(print_header_args))

    if output_writer:
        csv_headers = [headers_map[col] for col in output_columns]
        output_writer.writerow(csv_headers)

    if args.power:
        starting_power = GPUPowerStats()
        ending_power = GPUPowerStats()

    renderers = []
    if args.renderer != "both":
        renderers.append(args.renderer)
    else:
        renderers = ("native", "vulkan")

    wall_times = defaultdict(dict)
    gpu_times = defaultdict(dict)
    frame_wall_times = defaultdict(dict)
    cpu_times = defaultdict(dict)
    gpu_powers = defaultdict(dict)
    cpu_powers = defaultdict(dict)
    infra_powers = defaultdict(dict)
    gpu_mem_sustaineds = defaultdict(dict)
    gpu_mem_peaks = defaultdict(dict)
    proc_mem_medians = defaultdict(dict)
    proc_mem_peaks = defaultdict(dict)
    process_cpuinsts = defaultdict(dict)
    gfxlib_cpuinsts = defaultdict(dict)
    angle_cpuinsts = defaultdict(dict)
    vulkan_cpuinsts = defaultdict(dict)
    gles_cpuinsts = defaultdict(dict)
    libc_cpuinsts = defaultdict(dict)

    # Organize the data for writing out
    rows = defaultdict(dict)

    with run_from_dir(args.build_dir):
        android_helper.PrepareTestSuite("angle_trace_tests")

    for trace in fnmatch.filter(traces, args.filter):

        print(
            "\nStarting run for %s loopcount %i with %s at %s\n" %
            (trace, int(args.loop_count), renderers, datetime.now().strftime('%Y-%m-%d %H:%M:%S')))

        with run_from_dir(args.build_dir):
            android_helper.PrepareRestrictedTraces([trace])

        # Start with clean data containers for each trace
        rows.clear()
        wall_times.clear()
        gpu_times.clear()
        frame_wall_times.clear()
        cpu_times.clear()
        gpu_powers.clear()
        cpu_powers.clear()
        infra_powers.clear()
        gpu_mem_sustaineds.clear()
        gpu_mem_peaks.clear()
        proc_mem_medians.clear()
        proc_mem_peaks.clear()
        process_cpuinsts.clear()
        gfxlib_cpuinsts.clear()
        angle_cpuinsts.clear()
        vulkan_cpuinsts.clear()
        gles_cpuinsts.clear()
        libc_cpuinsts.clear()

        for i in range(int(args.loop_count)):

            for renderer in renderers:

                if renderer == "native":
                    # Force the settings to native
                    run_adb_shell_command(
                        'settings put global angle_gl_driver_selection_pkgs com.android.angle.test'
                    )
                    run_adb_shell_command(
                        'settings put global angle_gl_driver_selection_values native')
                elif renderer == "vulkan":
                    # Force the settings to ANGLE
                    run_adb_shell_command(
                        'settings put global angle_gl_driver_selection_pkgs com.android.angle.test'
                    )
                    run_adb_shell_command(
                        'settings put global angle_gl_driver_selection_values angle')
                elif renderer == "default":
                    logging.info(
                        'Deleting Android settings for forcing selection of GLES driver, ' +
                        'allowing system to load the default')
                    run_adb_shell_command('settings delete global angle_debug_package')
                    run_adb_shell_command('settings delete global angle_gl_driver_all_angle')
                    run_adb_shell_command('settings delete global angle_gl_driver_selection_pkgs')
                    run_adb_shell_command(
                        'settings delete global angle_gl_driver_selection_values')
                else:
                    logging.error('Unsupported renderer {}'.format(renderer))
                    exit()

                if args.angle_package == 'system':
                    # Clear the debug package so ANGLE will be loaded from /system/lib64
                    run_adb_shell_command('settings delete global angle_debug_package')
                else:
                    # Otherwise, load ANGLE from the specified APK
                    run_adb_shell_command('settings put global angle_debug_package ' +
                                          args.angle_package)

                # Remove any previous perf results
                cleanup()
                # Clear blob cache to avoid post-warmup cache eviction b/298028816
                clear_blob_cache()

                test = trace.split(' ')[0]

                if args.cpu_inst or args.power:
                    run_adb_command(['logcat', '-c'])  # needed for wait_for_test_warmup
                    done_event = threading.Event()

                if args.cpu_inst:
                    assert args.fixedtime, '--cpu_inst requires --fixedtime'
                    cpu_inst_results = {
                        "Total": 0,
                        "gles_lib": 0,
                        "angle_lib": 0,
                        "vulkan_lib": 0,
                        "libc_lib": 0,
                    }  # output arg
                    cpu_inst_thread = threading.Thread(
                        target=collect_cpu_inst,
                        args=(done_event, float(args.fixedtime), cpu_inst_results))
                    cpu_inst_thread.daemon = True
                    cpu_inst_thread.start()

                if args.power:
                    assert args.fixedtime, '--power requires --fixedtime'
                    power_results = {}  # output arg
                    power_thread = threading.Thread(
                        target=collect_power,
                        args=(done_event, float(args.fixedtime), power_results))
                    power_thread.daemon = True
                    power_thread.start()

                # We scope the run_trace call so we can use a temp dir for screenshots that gets deleted
                with contextlib.ExitStack() as stack:
                    screenshot_device_dir = None
                    if args.screenshot_dir != '' or args.screenshot_frame != '':
                        temp_dir = stack.enter_context(android_helper._TempDeviceDir())
                        screenshot_device_dir = temp_dir

                    logging.debug('Running %s' % test)
                    test_time = run_trace(test, args, screenshot_device_dir)

                    if screenshot_device_dir:
                        pull_screenshot(args, screenshot_device_dir, renderer)

                if args.cpu_inst or args.power:
                    done_event.set()

                process_cpuinst = 0
                gfxlib_cpuinst = 0
                angle_cpuinst = 0
                vulkan_cpuinst = 0
                gles_cpuinst = 0
                libc_cpuinst = 0
                if args.cpu_inst:
                    cpu_inst_thread.join(timeout=10)
                    if cpu_inst_thread.is_alive():
                        logging.warning('collect_cpu_inst thread did not terminate')
                    else:
                        # Calculate per frame cpu instruction count
                        frame_count = safe_cast_float(get_frame_count())
                        process_cpuinst = cpu_inst_results["Total"]
                        process_cpuinst = safe_divide(process_cpuinst, frame_count)

                        gfxlib_cpuinst = cpu_inst_results["gles_lib"]
                        gfxlib_cpuinst += cpu_inst_results["angle_lib"]
                        gfxlib_cpuinst += cpu_inst_results["vulkan_lib"]
                        gfxlib_cpuinst = safe_divide(gfxlib_cpuinst, frame_count)

                        angle_cpuinst = cpu_inst_results["angle_lib"]
                        angle_cpuinst = safe_divide(angle_cpuinst, frame_count)

                        vulkan_cpuinst = cpu_inst_results["vulkan_lib"]
                        vulkan_cpuinst = safe_divide(vulkan_cpuinst, frame_count)

                        gles_cpuinst = cpu_inst_results["gles_lib"]
                        gles_cpuinst = safe_divide(gles_cpuinst, frame_count)

                        libc_cpuinst = cpu_inst_results["libc_lib"]
                        libc_cpuinst = safe_divide(libc_cpuinst, frame_count)

                gpu_power, cpu_power, infra_power = 0, 0, 0

                if args.power:
                    power_thread.join(timeout=2)
                    if power_thread.is_alive():
                        logging.warning('collect_power thread did not terminate')
                    else:
                        gpu_power = power_results['gpu']
                        cpu_power = power_results['cpu']
                        infra_power = power_results['infra']

                wall_time = get_test_time()

                gpu_time = get_gpu_time() if args.gpu_time else '0'

                frame_wall_time = get_frame_wall_time()

                cpu_time = get_cpu_time()

                gpu_mem_sustained, gpu_mem_peak = 0, 0
                proc_mem_peak, proc_mem_median = 0, 0
                if args.memory:
                    gpu_mem_sustained, gpu_mem_peak = get_gpu_memory(test_time)
                    logging.debug(
                        '%s = %i, %s = %i' %
                        ('gpu_mem_sustained', gpu_mem_sustained, 'gpu_mem_peak', gpu_mem_peak))

                    proc_mem_peak, proc_mem_median = get_proc_memory()

                trace_name = mode + renderer + '_' + test

                if len(wall_times[test]) == 0:
                    wall_times[test] = defaultdict(list)
                wall_times[test][renderer].append(safe_cast_float(wall_time))

                if len(gpu_times[test]) == 0:
                    gpu_times[test] = defaultdict(list)
                gpu_times[test][renderer].append(safe_cast_float(gpu_time))

                if len(frame_wall_times[test]) == 0:
                    frame_wall_times[test] = defaultdict(list)
                frame_wall_times[test][renderer].append(safe_cast_float(frame_wall_time))

                if len(cpu_times[test]) == 0:
                    cpu_times[test] = defaultdict(list)
                cpu_times[test][renderer].append(safe_cast_float(cpu_time))

                if len(gpu_powers[test]) == 0:
                    gpu_powers[test] = defaultdict(list)
                gpu_powers[test][renderer].append(safe_cast_float(gpu_power))

                if len(cpu_powers[test]) == 0:
                    cpu_powers[test] = defaultdict(list)
                cpu_powers[test][renderer].append(safe_cast_float(cpu_power))

                if len(infra_powers[test]) == 0:
                    infra_powers[test] = defaultdict(list)
                infra_powers[test][renderer].append(safe_cast_float(infra_power))

                if len(gpu_mem_sustaineds[test]) == 0:
                    gpu_mem_sustaineds[test] = defaultdict(list)
                gpu_mem_sustaineds[test][renderer].append(safe_cast_int(gpu_mem_sustained))

                if len(gpu_mem_peaks[test]) == 0:
                    gpu_mem_peaks[test] = defaultdict(list)
                gpu_mem_peaks[test][renderer].append(safe_cast_int(gpu_mem_peak))

                if len(proc_mem_medians[test]) == 0:
                    proc_mem_medians[test] = defaultdict(list)
                proc_mem_medians[test][renderer].append(safe_cast_int(proc_mem_median))

                if len(proc_mem_peaks[test]) == 0:
                    proc_mem_peaks[test] = defaultdict(list)
                proc_mem_peaks[test][renderer].append(safe_cast_int(proc_mem_peak))

                if len(process_cpuinsts[test]) == 0:
                    process_cpuinsts[test] = defaultdict(list)
                process_cpuinsts[test][renderer].append(safe_cast_float(process_cpuinst))

                if len(gfxlib_cpuinsts[test]) == 0:
                    gfxlib_cpuinsts[test] = defaultdict(list)
                gfxlib_cpuinsts[test][renderer].append(safe_cast_float(gfxlib_cpuinst))

                if len(angle_cpuinsts[test]) == 0:
                    angle_cpuinsts[test] = defaultdict(list)
                angle_cpuinsts[test][renderer].append(safe_cast_float(angle_cpuinst))

                if len(vulkan_cpuinsts[test]) == 0:
                    vulkan_cpuinsts[test] = defaultdict(list)
                vulkan_cpuinsts[test][renderer].append(safe_cast_float(vulkan_cpuinst))

                if len(gles_cpuinsts[test]) == 0:
                    gles_cpuinsts[test] = defaultdict(list)
                gles_cpuinsts[test][renderer].append(safe_cast_float(gles_cpuinst))

                if len(libc_cpuinsts[test]) == 0:
                    libc_cpuinsts[test] = defaultdict(list)
                libc_cpuinsts[test][renderer].append(safe_cast_float(libc_cpuinst))

                # Maps column names to formatting for output
                val_map = {
                    'trace': trace_name,
                    'wall_time': wall_time,
                    'gpu_time': gpu_time,
                    'frame_wall_time': frame_wall_time,
                    'cpu_time': cpu_time,
                    'gpu_power': '%.3f' % gpu_power,
                    'cpu_power': '%.3f' % cpu_power,
                    'infra_power': '%.3f' % infra_power,
                    'gpu_mem_sustained': gpu_mem_sustained,
                    'gpu_mem_peak': gpu_mem_peak,
                    'proc_mem_median': proc_mem_median,
                    'proc_mem_peak': proc_mem_peak,
                    'process_cpuinst': process_cpuinst,
                    'gfxlib_cpuinst': gfxlib_cpuinst,
                    'angle_cpuinst': angle_cpuinst,
                    'vulkan_cpuinst': vulkan_cpuinst,
                    'gles_cpuinst': gles_cpuinst,
                    'libc_cpuinst': libc_cpuinst
                }

                # Maps column names to raw values for CSV
                csv_val_map = {
                    'trace': trace_name,
                    'wall_time': wall_time,
                    'gpu_time': gpu_time,
                    'frame_wall_time': frame_wall_time,
                    'cpu_time': cpu_time,
                    'gpu_power': gpu_power,
                    'cpu_power': cpu_power,
                    'infra_power': infra_power,
                    'gpu_mem_sustained': gpu_mem_sustained,
                    'gpu_mem_peak': gpu_mem_peak,
                    'proc_mem_median': proc_mem_median,
                    'proc_mem_peak': proc_mem_peak,
                    'process_cpuinst': process_cpuinst,
                    'gfxlib_cpuinst': gfxlib_cpuinst,
                    'angle_cpuinst': angle_cpuinst,
                    'vulkan_cpuinst': vulkan_cpuinst,
                    'gles_cpuinst': gles_cpuinst,
                    'libc_cpuinst': libc_cpuinst
                }

                print_val_args = []
                for col in output_columns:
                    print_val_args.extend([column_width[col], val_map[col]])

                # Execute Print based on the dynamically set formatting and variable list
                print(format_str % tuple(print_val_args))

                if output_writer:
                    csv_row = [csv_val_map[col] for col in output_columns]
                    output_writer.writerow(csv_row)

                # Early exit for testing
                #exit()

                # Depending on workload, sleeps might be needed to dissipate heat or recharge battery
                if args.sleep != 0:
                    time.sleep(args.sleep)

                if args.custom_throttling_temp:
                    sleep_until_temps_below(args.custom_throttling_temp)

                if args.custom_throttling_thermalservice_temp:
                    sleep_until_temps_below_thermalservice(
                        args.custom_throttling_thermalservice_temp)

                if args.min_battery_level:
                    sleep_until_battery_level(args.min_battery_level)

            print()  # New line for readability

    if output_file:
        output_file.close()


def generate_summary(raw_data_filename, summary_filename):

    wall_times = defaultdict(lambda: defaultdict(list))
    gpu_times = defaultdict(lambda: defaultdict(list))
    frame_wall_times = defaultdict(lambda: defaultdict(list))
    cpu_times = defaultdict(lambda: defaultdict(list))
    gpu_powers = defaultdict(lambda: defaultdict(list))
    cpu_powers = defaultdict(lambda: defaultdict(list))
    infra_powers = defaultdict(lambda: defaultdict(list))
    gpu_mem_sustaineds = defaultdict(lambda: defaultdict(list))
    gpu_mem_peaks = defaultdict(lambda: defaultdict(list))
    proc_mem_medians = defaultdict(lambda: defaultdict(list))
    proc_mem_peaks = defaultdict(lambda: defaultdict(list))
    process_cpuinsts = defaultdict(lambda: defaultdict(list))
    gfxlib_cpuinsts = defaultdict(lambda: defaultdict(list))
    angle_cpuinsts = defaultdict(lambda: defaultdict(list))
    vulkan_cpuinsts = defaultdict(lambda: defaultdict(list))
    gles_cpuinsts = defaultdict(lambda: defaultdict(list))
    libc_cpuinsts = defaultdict(lambda: defaultdict(list))

    renderers = set()
    has_gpu_time = False
    has_power = False
    has_memory = False
    has_cpuinst = False

    try:
        with open(raw_data_filename, 'r') as f:

            android_version = f.readline().strip()
            angle_version = f.readline().strip()
            # skip source file
            bogus_source = f.readline().strip()
            args_used = f.readline().strip()

            reader = csv.DictReader(f, delimiter=',', skipinitialspace=True)
            fieldnames = reader.fieldnames or []

            if 'gpu_time(ms)' in fieldnames:
                has_gpu_time = True
            if 'gpu_power(W)' in fieldnames:
                has_power = True
            if 'gpu_mem_sustained' in fieldnames:
                has_memory = True
            if 'process_cpuinst' in fieldnames:
                has_cpuinst = True

            for row in reader:
                test = row['trace']
                trace = test.split('_', 1)[1]
                renderer = test.split('_')[0]  # potential issue if mode is present
                renderers.add(renderer)

                wall_times[trace][renderer].append(safe_cast_float(row.get('wall_time(ms)', '')))
                gpu_times[trace][renderer].append(safe_cast_float(row.get('gpu_time(ms)', '')))
                frame_wall_times[trace][renderer].append(
                    safe_cast_float(row.get('frame_wall_time(ms)', '')))
                cpu_times[trace][renderer].append(safe_cast_float(row.get('cpu_time(ms)', '')))
                gpu_powers[trace][renderer].append(safe_cast_float(row.get('gpu_power(W)', '')))
                cpu_powers[trace][renderer].append(safe_cast_float(row.get('cpu_power(W)', '')))
                infra_powers[trace][renderer].append(
                    safe_cast_float(row.get('infra_power(W)', '')))
                gpu_mem_sustaineds[trace][renderer].append(
                    safe_cast_int(row.get('gpu_mem_sustained', '')))
                gpu_mem_peaks[trace][renderer].append(safe_cast_int(row.get('gpu_mem_peak', '')))
                proc_mem_medians[trace][renderer].append(
                    safe_cast_int(row.get('proc_mem_median', '')))
                proc_mem_peaks[trace][renderer].append(safe_cast_int(row.get('proc_mem_peak', '')))
                process_cpuinsts[trace][renderer].append(
                    safe_cast_float(row.get('process_cpuinst', '')))
                gfxlib_cpuinsts[trace][renderer].append(
                    safe_cast_float(row.get('gfxlib_cpuinst', '')))
                angle_cpuinsts[trace][renderer].append(
                    safe_cast_float(row.get('angle_cpuinst', '')))
                vulkan_cpuinsts[trace][renderer].append(
                    safe_cast_float(row.get('vulkan_cpuinst', '')))
                gles_cpuinsts[trace][renderer].append(safe_cast_float(row.get('gles_cpuinst', '')))
                libc_cpuinsts[trace][renderer].append(safe_cast_float(row.get('libc_cpuinst', '')))

    except FileNotFoundError:
        logging.error(f"Error: Raw data file '{raw_data_filename}' not found.")
        return

    rows = defaultdict(dict)

    def populate_summary_row(rows, name, results):
        if len(rows[name]) == 0:
            rows[name] = defaultdict(list)
        for renderer, wtimes in results.items():
            average, variance = drop_high_low_and_average(wtimes)
            rows[name][renderer].append(average)
            rows[name][renderer].append(variance)

    # Generate the SUMMARY output
    summary_file = open(summary_filename, 'w', newline='')

    summary_file.write(android_version + "\n")
    summary_file.write(angle_version + "\n")
    summary_file.write(raw_data_filename + "\n")
    summary_file.write(args_used + "\n")

    summary_writer = csv.writer(summary_file)

    trace_number = 0

    if (len(renderers) == 1) and "both" not in renderers:
        renderer_name = list(renderers)[0]
        header_row = [
            "#", "\"Trace\"", f"\"{renderer_name}\nwall\ntime\nper\nframe\n(ms)\"",
            f"\"{renderer_name}\nwall\ntime\nvariance\""
        ]
        if has_gpu_time:
            header_row.extend([
                f"\"{renderer_name}\nGPU\ntime\nper\nframe\n(ms)\"",
                f"\"{renderer_name}\nGPU\ntime\nvariance\""
            ])
        header_row.extend([
            f"\"{renderer_name}\nFrame\nwall\ntime\nper\nframe\n(ms)\"",
            f"\"{renderer_name}\nFrame\nwall\ntime\nvariance\"",
            f"\"{renderer_name}\nCPU\ntime\nper\nframe\n(ms)\"",
            f"\"{renderer_name}\nCPU\ntime\nvariance\""
        ])

        if has_power:
            header_row.extend([
                f"\"{renderer_name}\nGPU\npower\n(W)\"",
                f"\"{renderer_name}\nGPU\npower\nvariance\"",
                f"\"{renderer_name}\nCPU\npower\n(W)\"",
                f"\"{renderer_name}\nCPU\npower\nvariance\"",
                f"\"{renderer_name}\nInfra\npower\n(W)\"",
                f"\"{renderer_name}\nInfra\npower\nvariance\""
            ])
        if has_memory:
            header_row.extend([
                f"\"{renderer_name}\nGPU\nmem\n(B)\"", f"\"{renderer_name}\nGPU\nmem\nvariance\"",
                f"\"{renderer_name}\npeak\nGPU\nmem\n(B)\"",
                f"\"{renderer_name}\npeak\nGPU\nmem\nvariance\"",
                f"\"{renderer_name}\nprocess\nmem\n(B)\"",
                f"\"{renderer_name}\nprocess\nmem\nvariance\"",
                f"\"{renderer_name}\npeak\nprocess\nmem\n(B)\"",
                f"\"{renderer_name}\npeak\nprocess\nmem\nvariance\""
            ])
        if has_cpuinst:
            header_row.extend([
                f"\"{renderer_name}\nprocess\ncpu\ninst\n\"",
                f"\"{renderer_name}\nprocess\ncpu\ninst\nvariance\"",
                f"\"{renderer_name}\ngfxlib\ncpu\ninst\n\"",
                f"\"{renderer_name}\ngfxlib\ncpu\ninst\nvariance\"",
                f"\"{renderer_name}\nangle\ncpu\ninst\n\"",
                f"\"{renderer_name}\nangle\ncpu\ninst\nvariance\"",
                f"\"{renderer_name}\nvulkan\ncpu\ninst\n\"",
                f"\"{renderer_name}\nvulkan\ncpu\ninst\nvariance\"",
                f"\"{renderer_name}\ngles\ncpu\ninst\n\"",
                f"\"{renderer_name}\ngles\ncpu\ninst\nvariance\"",
                f"\"{renderer_name}\nlibc\ncpu\ninst\n\"",
                f"\"{renderer_name}\nlibc\ncpu\ninst\nvariance\""
            ])
        summary_writer.writerow(header_row)
    else:
        header_row = [
            "#", "\"Trace\"", "\"Native\nwall\ntime\nper\nframe\n(ms)\"",
            "\"Native\nwall\ntime\nvariance\"", "\"ANGLE\nwall\ntime\nper\nframe\n(ms)\"",
            "\"ANGLE\nwall\ntime\nvariance\"", "\"wall\ntime\ncompare\""
        ]
        if has_gpu_time:
            header_row.extend([
                "\"Native\nGPU\ntime\nper\nframe\n(ms)\"", "\"Native\nGPU\ntime\nvariance\"",
                "\"ANGLE\nGPU\ntime\nper\nframe\n(ms)\"", "\"ANGLE\nGPU\ntime\nvariance\"",
                "\"GPU\ntime\ncompare\""
            ])
        header_row.extend([
            "\"Native\nFrame\nwall\ntime\nper\nframe\n(ms)\"",
            "\"Native\nFrame\nwall\ntime\nvariance\"",
            "\"ANGLE\nFrame\nwall\ntime\nper\nframe\n(ms)\"",
            "\"ANGLE\nFrame\nwall\ntime\nvariance\"", "\"Frame\nwall\ntime\ncompare\"",
            "\"Native\nCPU\ntime\nper\nframe\n(ms)\"", "\"Native\nCPU\ntime\nvariance\"",
            "\"ANGLE\nCPU\ntime\nper\nframe\n(ms)\"", "\"ANGLE\nCPU\ntime\nvariance\"",
            "\"CPU\ntime\ncompare\""
        ])

        if has_power:
            header_row.extend([
                "\"Native\nGPU\npower\n(W)\"", "\"Native\nGPU\npower\nvariance\"",
                "\"ANGLE\nGPU\npower\n(W)\"", "\"ANGLE\nGPU\npower\nvariance\"",
                "\"GPU\npower\ncompare\"", "\"Native\nCPU\npower\n(W)\"",
                "\"Native\nCPU\npower\nvariance\"", "\"ANGLE\nCPU\npower\n(W)\"",
                "\"ANGLE\nCPU\npower\nvariance\"", "\"CPU\npower\ncompare\"",
                "\"Native\nInfra\npower\n(W)\"", "\"Native\nInfra\npower\nvariance\"",
                "\"ANGLE\nInfra\npower\n(W)\"", "\"ANGLE\nInfra\npower\nvariance\"",
                "\"Infra\npower\ncompare\""
            ])
        if has_memory:
            header_row.extend([
                "\"Native\nGPU\nmem\n(B)\"", "\"Native\nGPU\nmem\nvariance\"",
                "\"ANGLE\nGPU\nmem\n(B)\"", "\"ANGLE\nGPU\nmem\nvariance\"",
                "\"GPU\nmem\ncompare\"", "\"Native\npeak\nGPU\nmem\n(B)\"",
                "\"Native\npeak\nGPU\nmem\nvariance\"", "\"ANGLE\npeak\nGPU\nmem\n(B)\"",
                "\"ANGLE\npeak\nGPU\nmem\nvariance\"", "\"GPU\npeak\nmem\ncompare\"",
                "\"Native\nprocess\nmem\n(B)\"", "\"Native\nprocess\nmem\nvariance\"",
                "\"ANGLE\nprocess\nmem\n(B)\"", "\"ANGLE\nprocess\nmem\nvariance\"",
                "\"process\nmem\ncompare\"", "\"Native\npeak\nprocess\nmem\n(B)\"",
                "\"Native\npeak\nprocess\nmem\nvariance\"", "\"ANGLE\npeak\nprocess\nmem\n(B)\"",
                "\"ANGLE\npeak\nprocess\nmem\nvariance\"", "\"process\npeak\nmem\ncompare\""
            ])
        if has_cpuinst:
            header_row.extend([
                "\"Native\nprocess\ncpu\ninst\"", "\"Native\nprocess\ncpu\ninst\nvariance\"",
                "\"ANGLE\nprocess\ncpu\ninst\"", "\"ANGLE\nprocess\ncpu\ninst\nvariance\"",
                "\"process\ncpu\ninst\ncompare\"", "\"Native\ngfxlib\ncpu\ninst\"",
                "\"Native\ngfxlib\ncpu\ninst\nvariance\"", "\"ANGLE\ngfxlib\ncpu\ninst\"",
                "\"ANGLE\ngfxlib\ncpu\ninst\nvariance\"", "\"gfxlib\ncpu\ninst\ncompare\"",
                "\"Native\nangle\ncpu\ninst\"", "\"Native\nangle\ncpu\ninst\nvariance\"",
                "\"ANGLE\nangle\ncpu\ninst\"", "\"ANGLE\nangle\ncpu\ninst\nvariance\"",
                "\"angle\ncpu\ninst\ncompare\"", "\"Native\nvulkan\ncpu\ninst\"",
                "\"Native\nvulkan\ncpu\ninst\nvariance\"", "\"ANGLE\nvulkan\ncpu\ninst\"",
                "\"ANGLE\nvulkan\ncpu\ninst\nvariance\"", "\"vulkan\ncpu\ninst\ncompare\"",
                "\"Native\ngles\ncpu\ninst\"", "\"Native\ngles\ncpu\ninst\nvariance\"",
                "\"ANGLE\ngles\ncpu\ninst\"", "\"ANGLE\ngles\ncpu\ninst\nvariance\"",
                "\"gles\ncpu\ninst\ncompare\"", "\"Native\nlibc\ncpu\ninst\"",
                "\"Native\nlibc\ncpu\ninst\nvariance\"", "\"ANGLE\nlibc\ncpu\ninst\"",
                "\"ANGLE\nlibc\ncpu\ninst\nvariance\"", "\"libc\ncpu\ninst\ncompare\""
            ])
        summary_writer.writerow(header_row)

    for name, results in wall_times.items():
        populate_summary_row(rows, name, results)

    for name, results in gpu_times.items():
        populate_summary_row(rows, name, results)

    for name, results in frame_wall_times.items():
        populate_summary_row(rows, name, results)

    for name, results in cpu_times.items():
        populate_summary_row(rows, name, results)

    for name, results in gpu_powers.items():
        populate_summary_row(rows, name, results)

    for name, results in cpu_powers.items():
        populate_summary_row(rows, name, results)

    for name, results in infra_powers.items():
        populate_summary_row(rows, name, results)

    for name, results in gpu_mem_sustaineds.items():
        populate_summary_row(rows, name, results)

    for name, results in gpu_mem_peaks.items():
        populate_summary_row(rows, name, results)

    for name, results in proc_mem_medians.items():
        populate_summary_row(rows, name, results)

    for name, results in proc_mem_peaks.items():
        populate_summary_row(rows, name, results)

    for name, results in process_cpuinsts.items():
        populate_summary_row(rows, name, results)

    for name, results in gfxlib_cpuinsts.items():
        populate_summary_row(rows, name, results)

    for name, results in angle_cpuinsts.items():
        populate_summary_row(rows, name, results)

    for name, results in vulkan_cpuinsts.items():
        populate_summary_row(rows, name, results)

    for name, results in gles_cpuinsts.items():
        populate_summary_row(rows, name, results)

    for name, results in libc_cpuinsts.items():
        populate_summary_row(rows, name, results)

    if (len(renderers) == 1) and "both" not in renderers:
        renderer_name = list(renderers)[0]
        for name, data in rows.items():
            trace_number += 1
            data_row = [
                trace_number,
                name,
                # wall_time
                "%.3f" % data[renderer_name][0],
                percent(data[renderer_name][1])
            ]
            if has_gpu_time:
                # GPU time
                data_row.extend(["%.3f" % data[renderer_name][2], percent(data[renderer_name][3])])
            data_row.extend([
                # Frame wall time
                "%.3f" % data[renderer_name][4],
                percent(data[renderer_name][5]),
                # CPU time
                "%.3f" % data[renderer_name][6],
                percent(data[renderer_name][7])
            ])

            if has_power:
                data_row.extend([
                    # GPU power
                    "%.3f" % data[renderer_name][8],
                    percent(data[renderer_name][9]),
                    # CPU power
                    "%.3f" % data[renderer_name][10],
                    percent(data[renderer_name][11]),
                    # Infra power
                    "%.3f" % data[renderer_name][12],
                    percent(data[renderer_name][13])
                ])
            if has_memory:
                data_row.extend([
                    # GPU mem
                    int(data[renderer_name][14]),
                    percent(data[renderer_name][15]),
                    # GPU peak mem
                    int(data[renderer_name][16]),
                    percent(data[renderer_name][17]),
                    # process mem
                    int(data[renderer_name][18]),
                    percent(data[renderer_name][19]),
                    # process peak mem
                    int(data[renderer_name][20]),
                    percent(data[renderer_name][21])
                ])
            if has_cpuinst:
                data_row.extend([
                    # process cpuinst
                    "%.3f" % data[renderer_name][22],
                    percent(data[renderer_name][23]),
                    # gfxlib cpuinst
                    "%.3f" % data[renderer_name][24],
                    percent(data[renderer_name][25]),
                    # angle cpuinst
                    "%.3f" % data[renderer_name][26],
                    percent(data[renderer_name][27]),
                    # vulkan cpuinst
                    "%.3f" % data[renderer_name][28],
                    percent(data[renderer_name][29]),
                    # gles cpuinst
                    "%.3f" % data[renderer_name][30],
                    percent(data[renderer_name][31]),
                    # libc cpuinst
                    "%.3f" % data[renderer_name][32],
                    percent(data[renderer_name][33])
                ])
            summary_writer.writerow(data_row)
    else:
        for name, data in rows.items():
            trace_number += 1
            data_row = [
                trace_number,
                name,
                # wall_time
                "%.3f" % data["native"][0],
                percent(data["native"][1]),
                "%.3f" % data["vulkan"][0],
                percent(data["vulkan"][1]),
                percent(safe_divide(data["native"][0], data["vulkan"][0]))
            ]
            if has_gpu_time:
                data_row.extend([
                    # GPU time
                    "%.3f" % data["native"][2],
                    percent(data["native"][3]),
                    "%.3f" % data["vulkan"][2],
                    percent(data["vulkan"][3]),
                    percent(safe_divide(data["native"][2], data["vulkan"][2]))
                ])
            data_row.extend([
                # Frame wall_time
                "%.3f" % data["native"][4],
                percent(data["native"][5]),
                "%.3f" % data["vulkan"][4],
                percent(data["vulkan"][5]),
                percent(safe_divide(data["native"][4], data["vulkan"][4])),
                # CPU time
                "%.3f" % data["native"][6],
                percent(data["native"][7]),
                "%.3f" % data["vulkan"][6],
                percent(data["vulkan"][7]),
                percent(safe_divide(data["native"][6], data["vulkan"][6]))
            ])

            if has_power:
                data_row.extend([
                    # GPU power
                    "%.3f" % data["native"][8],
                    percent(data["native"][9]),
                    "%.3f" % data["vulkan"][8],
                    percent(data["vulkan"][9]),
                    percent(safe_divide(data["native"][8], data["vulkan"][8])),
                    # CPU power
                    "%.3f" % data["native"][10],
                    percent(data["native"][11]),
                    "%.3f" % data["vulkan"][10],
                    percent(data["vulkan"][11]),
                    percent(safe_divide(data["native"][10], data["vulkan"][10])),
                    # Infra power
                    "%.3f" % data["native"][12],
                    percent(data["native"][13]),
                    "%.3f" % data["vulkan"][12],
                    percent(data["vulkan"][13]),
                    percent(safe_divide(data["native"][12], data["vulkan"][12]))
                ])
            if has_memory:
                data_row.extend([
                    # GPU mem
                    int(data["native"][14]),
                    percent(data["native"][15]),
                    int(data["vulkan"][14]),
                    percent(data["vulkan"][15]),
                    percent(safe_divide(data["native"][14], data["vulkan"][14])),
                    # GPU peak mem
                    int(data["native"][16]),
                    percent(data["native"][17]),
                    int(data["vulkan"][16]),
                    percent(data["vulkan"][17]),
                    percent(safe_divide(data["native"][16], data["vulkan"][16])),
                    # process mem
                    int(data["native"][18]),
                    percent(data["native"][19]),
                    int(data["vulkan"][18]),
                    percent(data["vulkan"][19]),
                    percent(safe_divide(data["native"][18], data["vulkan"][18])),
                    # process peak mem
                    int(data["native"][20]),
                    percent(data["native"][21]),
                    int(data["vulkan"][20]),
                    percent(data["vulkan"][21]),
                    percent(safe_divide(data["native"][20], data["vulkan"][20]))
                ])
            if has_cpuinst:
                data_row.extend([
                    # process cpuinst
                    "%.3f" % data["native"][22],
                    percent(data["native"][23]),
                    "%.3f" % data["vulkan"][22],
                    percent(data["vulkan"][23]),
                    percent(safe_divide(data["native"][22], data["vulkan"][22])),
                    # gfxlib cpuinst
                    "%.3f" % data["native"][24],
                    percent(data["native"][25]),
                    "%.3f" % data["vulkan"][24],
                    percent(data["vulkan"][25]),
                    percent(safe_divide(data["native"][24], data["vulkan"][24])),
                    # angle cpuinst
                    "%.3f" % data["native"][26],
                    percent(data["native"][27]),
                    "%.3f" % data["vulkan"][26],
                    percent(data["vulkan"][27]),
                    percent(safe_divide(data["native"][26], data["vulkan"][26])),
                    # vulkan cpuinst
                    "%.3f" % data["native"][28],
                    percent(data["native"][29]),
                    "%.3f" % data["vulkan"][28],
                    percent(data["vulkan"][29]),
                    percent(safe_divide(data["native"][28], data["vulkan"][28])),
                    # gles cpuinst
                    "%.3f" % data["native"][30],
                    percent(data["native"][31]),
                    "%.3f" % data["vulkan"][30],
                    percent(data["vulkan"][31]),
                    percent(safe_divide(data["native"][30], data["vulkan"][30])),
                    # libc cpuinst
                    "%.3f" % data["native"][32],
                    percent(data["native"][33]),
                    "%.3f" % data["vulkan"][32],
                    percent(data["vulkan"][33]),
                    percent(safe_divide(data["native"][32], data["vulkan"][32]))
                ])
            summary_writer.writerow(data_row)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-f', '--filter', help='Trace filter. Defaults to all.', default='*')
    parser.add_argument('-l', '--log', help='Logging level.', default=DEFAULT_LOG_LEVEL)
    parser.add_argument(
        '--renderer',
        help='Which renderer to use: native, vulkan (via ANGLE), or default (' +
        'GLES driver selected by system). Providing no option will run twice, native and vulkan',
        default='both')
    parser.add_argument(
        '--walltimeonly',
        help='Limit output to just wall time',
        action='store_true',
        default=False)
    parser.add_argument(
        '--minimal-output',
        help='Only output the explicitly requested columns. Otherwise, all columns will be output.',
        action='store_true',
        default=False)
    parser.add_argument(
        '--power', help='Include CPU/GPU power used per trace', action='store_true', default=False)
    parser.add_argument(
        '--cpu-inst',
        help='Include cpu instruction count of gpu user mode driver per trace',
        action='store_true',
        default=False)
    parser.add_argument(
        '--memory',
        help='Include CPU/GPU memory used per trace',
        action='store_true',
        default=False)
    parser.add_argument('--maxsteps', help='Run for fixed set of frames', default='')
    parser.add_argument(
        '--run-to-key-frame', help='Run to key-frame', action='store_true', default=False)
    parser.add_argument('--fixedtime', help='Run for fixed set of time', default='')
    parser.add_argument(
        '--minimizegpuwork',
        help='Whether to run with minimized GPU work',
        action='store_true',
        default=False)
    parser.add_argument(
        '--skip-blit-in-offscreen',
        help='skip blit operation in offscreen mode',
        action='store_true',
        default=False)
    parser.add_argument('--output-tag', help='Tag for output files.')
    parser.add_argument('--angle-version', help='Specify ANGLE version string.', default='')
    parser.add_argument(
        '--loop-count', help='How many times to loop through the traces', default=5)
    parser.add_argument(
        '--device', help='Which device to run the tests on (use serial)', default='')
    parser.add_argument(
        '--sleep', help='Add a sleep of this many seconds between each test)', type=int, default=0)
    parser.add_argument(
        '--custom-throttling-temp',
        help='Custom thermal throttling with limit set to this temperature (off by default)',
        type=float)
    parser.add_argument(
        '--custom-throttling-thermalservice-temp',
        help='Custom thermal throttling (thermalservice) with limit set to this temperature (off by default)',
        type=float)
    parser.add_argument(
        '--min-battery-level',
        help='Sleep between tests if battery level drops below this value (off by default)',
        type=int)
    parser.add_argument(
        '--angle-package',
        help='Where to load ANGLE libraries from. This will load from the test APK by default, ' +
        'but you can point to any APK that contains ANGLE. Specify \'system\' to use libraries ' +
        'already on the device',
        default=DEFAULT_ANGLE_PACKAGE)
    parser.add_argument(
        '--build-dir',
        help='Where to find the APK on the host, i.e. out/Android. If unset, it is assumed you ' +
        'are running from the build dir already, or are using the wrapper script ' +
        'out/<config>/restricted_trace_perf.',
        default='')
    parser.add_argument(
        '--screenshot-dir',
        help='Host (local) directory to store screenshots of keyframes, which is frame 1 unless ' +
        'the trace JSON file contains \'KeyFrames\'.',
        default='')
    parser.add_argument(
        '--screenshot-frame',
        help='Specify a specific frame to screenshot. Uses --screenshot-dir if provied.',
        default='')
    parser.add_argument(
        '--fps-limit', help='Limit replay framerate to specified value', default='')

    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        '--vsync',
        help='Whether to run the trace in vsync mode',
        action='store_true',
        default=False)
    group.add_argument(
        '--offscreen',
        help='Whether to run the trace in offscreen mode',
        action='store_true',
        default=False)
    parser.add_argument(
        '--fps-limit-uses-busy-wait',
        help='Use busy wait instead of sleep to limit the framerate.',
        action='store_true',
        default=False)
    parser.add_argument(
        '--gpu-time', help='Enables GPU time tracking', action='store_true', default=False)
    parser.add_argument(
        '--add-swap-into-gpu-time',
        help='Adds swap/offscreen blit into the gpu_time tracking',
        action='store_true',
        default=False)
    parser.add_argument(
        '--add-swap-into-frame-wall-time',
        help='Adds swap/offscreen blit into the frame_wall_time tracking',
        action='store_true',
        default=False)
    parser.add_argument(
        '--summarize-only',
        nargs=2,
        metavar=('raw_data', 'summary'),
        help='Generates summary from raw_data CSV. Takes exactly two arguments - raw_data filename followed by summary filename.'
    )

    args = parser.parse_args()

    angle_test_util.SetupLogging(args.log.upper())

    if args.summarize_only:

        # Ensure the user knows we're just generating a summary
        other_flags = [a for a in sys.argv[1:] if a.startswith('-') and a != '--summarize-only']
        if other_flags:
            parser.error(
                f"The following flags are not allowed with --summarize-only: {', '.join(other_flags)}"
            )

        raw_data, summary = args.summarize_only
        print(f"raw_data: {raw_data}, summary: {summary}")

        generate_summary(raw_data, summary)

    else:

        with run_from_dir(args.build_dir):
            android_helper.Initialize("angle_trace_tests")  # includes adb root

        # Determine some starting parameters
        init()

        try:
            if args.custom_throttling_temp:
                set_vendor_thermal_control(disabled=1)
            run_traces(args)
            if args.output_tag:
                generate_summary(get_raw_data_name(args), get_summary_name(args))
        finally:
            if args.custom_throttling_temp:
                set_vendor_thermal_control(disabled=0)
            # Clean up settings, including in case of exceptions (including Ctrl-C)
            run_adb_shell_command('settings delete global angle_debug_package')
            run_adb_shell_command('settings delete global angle_gl_driver_selection_pkgs')
            run_adb_shell_command('settings delete global angle_gl_driver_selection_values')

    return 0


if __name__ == '__main__':
    sys.exit(main())
