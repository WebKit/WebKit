#! /usr/bin/env python3
#
# Copyright 2021 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
'''
Pixel 6 (ARM based) specific script that measures the following for each restricted_trace:
- Wall time per frame
- GPU time per frame (if run with --vsync)
- CPU time per frame
- GPU power per frame
- CPU power per frame
- GPU memory per frame

Setup:

  autoninja -C out/<config> angle_trace_perf_tests angle_apks
  adb install -r --force-queryable ./out/<config>/apks/AngleLibraries.apk
  adb install -r out/<config>/angle_trace_tests_apk/angle_trace_tests-debug.apk
  (cd out/<config>; ../../src/tests/run_angle_android_test.py angle_trace_tests \
   --verbose --local-output --verbose-logging --max-steps-performed 1 --log=debug)

Recommended command to run:

  python3 restricted_trace_perf.py --fixedtime 10 --power --output-tag android.$(date '+%Y%m%d') --loop-count 5

- This will run through all the traces 5 times with the native driver, then 5 times with vulkan (via ANGLE)
- 10 second run time with one warmup loop

Output will be printed to the terminal as it is collected.

Of the 5 runs, the high and low for each data point will be dropped, average of the remaining three will be tracked in the summary spreadsheet.
'''

import argparse
import copy
import csv
import fcntl
import fnmatch
import json
import logging
import os
import re
import statistics
import subprocess
import sys
import threading
import time

from collections import defaultdict, namedtuple
from datetime import datetime
from psutil import process_iter

DEFAULT_TEST_DIR = '.'
DEFAULT_TEST_JSON = 'restricted_traces.json'
DEFAULT_LOG_LEVEL = 'info'

Result = namedtuple('Result', ['stdout', 'stderr', 'time'])


def run_command(args):
    logging.debug('Running %s' % args)

    start_time = time.time()

    try:
        process = subprocess.Popen(
            args,
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True)
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(
            e.cmd, e.returncode, e.output))

    stdout, stderr = process.communicate()

    time_elapsed = time.time() - start_time

    return Result(stdout, stderr, time_elapsed)


def run_async_command(args):
    logging.debug('Kicking off subprocess %s' % (args))

    try:
        async_process = subprocess.Popen(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            shell=True,
            universal_newlines=True)
    except subprocess.CalledProcessError as e:
        raise RuntimeError("command '{}' return with error (code {}): {}".format(
            e.cmd, e.returncode, e.output))

    logging.debug('Done launching subprocess')

    return async_process


def run_adb_command(args):
    return run_command('adb ' + args)


def run_async_adb_command(args):
    return run_async_command('adb ' + args)


def cleanup():
    run_adb_command('shell rm -f /sdcard/Download/out.txt /sdcard/Download/gpumem.txt')


def clear_blob_cache():
    run_adb_command(
        'shell run-as com.android.angle.test rm -rf /data/user_de/0/com.android.angle.test/cache')


def select_device(device_arg):
    # The output from 'adb devices' always includes a header and a new line at the end.
    result_dev = run_command('adb devices')
    result_dev_out = result_dev.stdout.strip()

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


def run_trace(trace, args):
    mode = get_mode(args)

    # Kick off a subprocess that collects peak gpu memory periodically
    # Note the 0.25 below is the delay (in seconds) between memory checks
    if args.memory:
        run_adb_command('push gpumem.sh /data/local/tmp')
        memory_command = 'shell sh /data/local/tmp/gpumem.sh 0.25'
        memory_process = run_async_adb_command(memory_command)

    flags = [
        '--gtest_filter=TraceTest.' + trace, '--use-gl=native', '--verbose', '--verbose-logging'
    ]
    if mode != '':
        flags.append('--' + mode)
    if args.maxsteps != '':
        flags += ['--max-steps-performed', args.maxsteps]
    if args.fixedtime != '':
        flags += ['--fixed-test-time-with-warmup', args.fixedtime]
    if args.minimizegpuwork:
        flags.append('--minimize-gpu-work')

    # Build a command that can be run directly over ADB, for example:
    r'''
adb shell am instrument -w \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.StdoutFile /sdcard/Download/out.txt \
    -e org.chromium.native_test.NativeTest.CommandLineFlags \
    "--gtest_filter=TraceTest.empires_and_puzzles\ --use-angle=vulkan\ --screenshot-dir\ /sdcard\ --screenshot-frame\ 2\ --max-steps-performed\ 2\ --no-warmup" \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.ShardNanoTimeout "1000000000000000000" \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.NativeTestActivity com.android.angle.test.AngleUnitTestActivity \
    com.android.angle.test/org.chromium.build.gtest_apk.NativeTestInstrumentationTestRunner
    '''
    adb_command = r'''
shell am instrument -w \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.StdoutFile /sdcard/Download/out.txt \
    -e org.chromium.native_test.NativeTest.CommandLineFlags "{flags}" \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.ShardNanoTimeout "1000000000000000000" \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.NativeTestActivity \
    com.android.angle.test.AngleUnitTestActivity \
    com.android.angle.test/org.chromium.build.gtest_apk.NativeTestInstrumentationTestRunner
    '''.format(flags=r'\ '.join(flags)).strip()  # Note: space escaped due to subprocess shell=True

    result = run_adb_command(adb_command)

    if args.memory:
        logging.debug('Killing gpumem subprocess')
        memory_process.kill()

    return result.time


def get_test_time():
    # Pull the results from the device and parse
    result = run_adb_command('shell cat /sdcard/Download/out.txt | grep -v Error | grep -v Frame')

    measured_time = None

    for line in result.stdout.splitlines():
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
        if '[  PASSED  ]' in result.stdout:
            measured_time = 'missing'
        else:
            measured_time = 'crashed'

    return measured_time


def get_gpu_memory(trace_duration):
    # Pull the results from the device and parse
    result = run_adb_command('shell cat /sdcard/Download/gpumem.txt | awk "NF"')

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
    for line in result.stdout.splitlines():
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
        gpu_mem_average = statistics.mean(gpu_mem_sustained)

    return gpu_mem_average, gpu_mem_max


def get_proc_memory():
    # Pull the results from the device and parse
    result = run_adb_command('shell cat /sdcard/Download/out.txt')
    memory_median = ''
    memory_max = ''

    for line in result.stdout.splitlines():
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
    result = run_adb_command('shell cat /sdcard/Download/out.txt')
    gpu_time = '0'

    for line in result.stdout.splitlines():
        # Look for "gpu_time" in the line and grab the second to last entry:
        logging.debug('Checking line: %s' % line)

        if "gpu_time" in line:
            gpu_time = line.split()[-2]
            break

    return gpu_time


def get_cpu_time():
    # Pull the results from the device and parse
    result = run_adb_command('shell cat /sdcard/Download/out.txt')
    cpu_time = '0'

    for line in result.stdout.splitlines():
        # Look for "cpu_time" in the line and grab the second to last entry:
        logging.debug('Checking line: %s' % line)

        if "cpu_time" in line:
            cpu_time = line.split()[-2]
            break

    return cpu_time


def get_frame_count():
    # Pull the results from the device and parse
    result = run_adb_command('shell cat /sdcard/Download/out.txt | grep -v Error | grep -v Frame')

    frame_count = 0

    for line in result.stdout.splitlines():
        logging.debug('Checking line: %s' % line)
        if "trial_steps" in line:
            frame_count = line.split()[-2]
            break

    logging.debug('Frame count: %s' % frame_count)
    return frame_count


class GPUPowerStats():

    # GPU power measured in uWs

    def __init__(self):
        self.power = {'gpu': 0, 'big_cpu': 0, 'mid_cpu': 0, 'little_cpu': 0}

    def gpu_delta(self, starting):
        return self.power['gpu'] - starting.power['gpu']

    def cpu_delta(self, starting):
        big = self.power['big_cpu'] - starting.power['big_cpu']
        mid = self.power['mid_cpu'] - starting.power['mid_cpu']
        little = self.power['little_cpu'] - starting.power['little_cpu']
        return big + mid + little

    def get_power_data(self):
        energy_value_result = run_adb_command(
            'shell cat /sys/bus/iio/devices/iio:device*/energy_value')
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
            r'\bS\d+S_VDD_G3D\b': 'gpu',
            r'\bS\d+M_VDD_CPUCL2\b': 'big_cpu',
            r'\bS\d+M_VDD_CPUCL1\b': 'mid_cpu',
            r'\bS\d+M_VDD_CPUCL0\b': 'little_cpu',
        }

        for m in id_map.values():
            self.power[m] = 0  # Set to 0 to check for missing values and dupes below

        for line in energy_value_result.stdout.splitlines():
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
    })


def get_thermal_info():
    out = run_adb_command('shell dumpsys android.hardware.thermal.IThermal/default').stdout
    result = [l for l in out.splitlines() if ('VIRTUAL-SKIN' in l and 'ThrottlingStatus:' in l)]
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
                is_charge = 'VIRTUAL-SKIN-CHARGE-' in line  # Only supposed to affect charging speed
                if 'ThrottlingStatus: NONE' not in line and not is_charge:
                    logging.info('Waiting for vendor throttling to finish: %s', line.strip())
                    time.sleep(10)
                    waiting = True
                    break

    run_adb_command('shell setprop persist.vendor.disable.thermal.control %d' % disabled)


def sleep_until_temps_below(limit_temp):
    waiting = True
    while waiting:
        waiting = False
        for line in get_thermal_info():
            v = float(line.split('CurrentValue:')[1].strip().split(' ')[0])
            if v > limit_temp:
                logging.info('Waiting for device temps below %.1f: %s', limit_temp, line.strip())
                time.sleep(5)
                waiting = True
                break


def sleep_until_battery_level(min_battery_level):
    while True:
        level = int(run_adb_command('shell dumpsys battery get level').stdout.strip())
        if level >= min_battery_level:
            break
        logging.info('Waiting for device battery level to reach %d. Current level: %d',
                     min_battery_level, level)
        time.sleep(10)


def drop_high_low_and_average(values):
    if len(values) >= 3:
        values.remove(min(values))
        values.remove(max(values))

    average = statistics.mean(values)

    variance = 0
    if len(values) >= 2 and average != 0:
        variance = statistics.stdev(values) / average

    print(average, variance)

    return float(average), float(variance)


def safe_divide(x, y):
    if y == 0:
        return 0
    return x / y


def safe_cast_float(x):
    if x == '':
        return 0
    return float(x)


def safe_cast_int(x):
    if x == '':
        return 0
    return int(x)


def percent(x):
    return "%.2f%%" % (x * 100)


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
        '--power', help='Include CPU/GPU power used per trace', action='store_true', default=False)
    parser.add_argument(
        '--memory',
        help='Include CPU/GPU memory used per trace',
        action='store_true',
        default=False)
    parser.add_argument('--maxsteps', help='Run for fixed set of frames', default='')
    parser.add_argument('--fixedtime', help='Run for fixed set of time', default='')
    parser.add_argument(
        '--minimizegpuwork',
        help='Whether to run with minimized GPU work',
        action='store_true',
        default=False)
    parser.add_argument('--output-tag', help='Tag for output files.', required=True)
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
        '--min-battery-level',
        help='Sleep between tests if battery level drops below this value (off by default)',
        type=int)

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

    args = parser.parse_args()

    logging.basicConfig(level=args.log.upper())

    run_adb_command('root')

    try:
        if args.custom_throttling_temp:
            set_vendor_thermal_control(disabled=1)
        run_traces(args)
    finally:
        if args.custom_throttling_temp:
            set_vendor_thermal_control(disabled=0)
        # Clean up settings, including in case of exceptions (including Ctrl-C)
        run_adb_command('shell settings delete global angle_debug_package')
        run_adb_command('shell settings delete global angle_gl_driver_selection_pkgs')
        run_adb_command('shell settings delete global angle_gl_driver_selection_values')

    return 0


def logged_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-tag')
    _, extra_args = parser.parse_known_args()
    return ' '.join(extra_args)


def run_traces(args):
    # Load trace names
    with open(os.path.join(DEFAULT_TEST_DIR, DEFAULT_TEST_JSON)) as f:
        traces = json.loads(f.read())

    # Have to split the 'trace version' thing up
    trace_and_version = traces['traces']
    traces = [i.split(' ',)[0] for i in trace_and_version]

    failures = []

    mode = get_mode(args)
    trace_width = get_trace_width(mode)

    # Select the target device
    select_device(args.device)

    # Add an underscore to the mode for use in loop below
    if mode != '':
        mode = mode + '_'

    # Create output CSV
    raw_data_filename = "raw_data." + args.output_tag + ".csv"
    output_file = open(raw_data_filename, 'w', newline='')
    output_writer = csv.writer(output_file)

    # Set some widths that allow easily reading the values, but fit on smaller monitors.
    column_width = {
        'trace': trace_width,
        'wall_time': 15,
        'gpu_time': 15,
        'cpu_time': 15,
        'gpu_power': 10,
        'cpu_power': 10,
        'gpu_mem_sustained': 20,
        'gpu_mem_peak': 15,
        'proc_mem_median': 20,
        'proc_mem_peak': 15
    }

    if args.walltimeonly:
        print('%-*s' % (trace_width, 'wall_time_per_frame'))
    else:
        print('%-*s %-*s %-*s %-*s %-*s %-*s %-*s %-*s %-*s %-*s' %
              (column_width['trace'], 'trace', column_width['wall_time'], 'wall_time',
               column_width['gpu_time'], 'gpu_time', column_width['cpu_time'], 'cpu_time',
               column_width['gpu_power'], 'gpu_power', column_width['cpu_power'], 'cpu_power',
               column_width['gpu_mem_sustained'], 'gpu_mem_sustained',
               column_width['gpu_mem_peak'], 'gpu_mem_peak', column_width['proc_mem_median'],
               'proc_mem_median', column_width['proc_mem_peak'], 'proc_mem_peak'))
        output_writer.writerow([
            'trace', 'wall_time(ms)', 'gpu_time(ms)', 'cpu_time(ms)', 'gpu_power(W)',
            'cpu_power(W)', 'gpu_mem_sustained', 'gpu_mem_peak', 'proc_mem_median', 'proc_mem_peak'
        ])

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
    cpu_times = defaultdict(dict)
    gpu_powers = defaultdict(dict)
    cpu_powers = defaultdict(dict)
    gpu_mem_sustaineds = defaultdict(dict)
    gpu_mem_peaks = defaultdict(dict)
    proc_mem_medians = defaultdict(dict)
    proc_mem_peaks = defaultdict(dict)

    for renderer in renderers:

        if renderer == "native":
            # Force the settings to native
            run_adb_command('shell settings put global angle_debug_package org.chromium.angle')
            run_adb_command(
                'shell settings put global angle_gl_driver_selection_pkgs com.android.angle.test')
            run_adb_command('shell settings put global angle_gl_driver_selection_values native')
        elif renderer == "vulkan":
            # Force the settings to ANGLE
            run_adb_command('shell settings put global angle_debug_package org.chromium.angle')
            run_adb_command(
                'shell settings put global angle_gl_driver_selection_pkgs com.android.angle.test')
            run_adb_command('shell settings put global angle_gl_driver_selection_values angle')
        elif renderer == "default":
            logging.info('Deleting Android settings for forcing selection of GLES driver, ' +
                         'allowing system to load the default')
            run_adb_command('shell settings delete global angle_debug_package')
            run_adb_command('shell settings delete global angle_gl_driver_selection_pkgs')
            run_adb_command('shell settings delete global angle_gl_driver_selection_values')
        else:
            logging.error('Unsupported renderer {}'.format(renderer))
            exit()

        for i in range(int(args.loop_count)):
            print("\nStarting run %i with %s at %s\n" %
                  (i + 1, renderer, datetime.now().strftime('%Y-%m-%d %H:%M:%S')))
            for trace in fnmatch.filter(traces, args.filter):
                # Remove any previous perf results
                cleanup()
                # Clear blob cache to avoid post-warmup cache eviction b/298028816
                clear_blob_cache()

                test = trace.split(' ')[0]

                if args.power:
                    assert args.fixedtime, '--power requires --fixedtime'
                    done_event = threading.Event()
                    run_adb_command('logcat -c')  # needed for wait_for_test_warmup
                    power_results = {}  # output arg
                    power_thread = threading.Thread(
                        target=collect_power,
                        args=(done_event, float(args.fixedtime), power_results))
                    power_thread.daemon = True
                    power_thread.start()

                logging.debug('Running %s' % test)
                test_time = run_trace(test, args)

                gpu_power, cpu_power = 0, 0
                if args.power:
                    done_event.set()
                    power_thread.join(timeout=2)
                    if power_thread.is_alive():
                        logging.warning('collect_power thread did not terminate')
                    else:
                        gpu_power = power_results['gpu']
                        cpu_power = power_results['cpu']

                wall_time = get_test_time()

                gpu_time = get_gpu_time() if args.vsync else '0'

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
                try:
                    wt = safe_cast_float(wall_time)
                except ValueError:  # e.g. 'crashed'
                    wt = -1
                wall_times[test][renderer].append(wt)

                if len(gpu_times[test]) == 0:
                    gpu_times[test] = defaultdict(list)
                gpu_times[test][renderer].append(safe_cast_float(gpu_time))

                if len(cpu_times[test]) == 0:
                    cpu_times[test] = defaultdict(list)
                cpu_times[test][renderer].append(safe_cast_float(cpu_time))

                if len(gpu_powers[test]) == 0:
                    gpu_powers[test] = defaultdict(list)
                gpu_powers[test][renderer].append(safe_cast_float(gpu_power))

                if len(cpu_powers[test]) == 0:
                    cpu_powers[test] = defaultdict(list)
                cpu_powers[test][renderer].append(safe_cast_float(cpu_power))

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

                if args.walltimeonly:
                    print('%-*s' % (trace_width, wall_time))
                else:
                    print(
                        '%-*s %-*s %-*s %-*s %-*s %-*s %-*i %-*i %-*i %-*i' %
                        (column_width['trace'], trace_name, column_width['wall_time'], wall_time,
                         column_width['gpu_time'], gpu_time, column_width['cpu_time'], cpu_time,
                         column_width['gpu_power'], '%.3f' % gpu_power, column_width['cpu_power'],
                         '%.3f' % cpu_power, column_width['gpu_mem_sustained'], gpu_mem_sustained,
                         column_width['gpu_mem_peak'], gpu_mem_peak,
                         column_width['proc_mem_median'], proc_mem_median,
                         column_width['proc_mem_peak'], proc_mem_peak))
                    output_writer.writerow([
                        mode + renderer + '_' + test, wall_time, gpu_time, cpu_time, gpu_power,
                        cpu_power, gpu_mem_sustained, gpu_mem_peak, proc_mem_median, proc_mem_peak
                    ])


                # Early exit for testing
                #exit()

                # Depending on workload, sleeps might be needed to dissipate heat or recharge battery
                if args.sleep != 0:
                    time.sleep(args.sleep)

                if args.custom_throttling_temp:
                    sleep_until_temps_below(args.custom_throttling_temp)

                if args.min_battery_level:
                    sleep_until_battery_level(args.min_battery_level)

    # Organize the data for writing out
    rows = defaultdict(dict)

    def populate_row(rows, name, results):
        if len(rows[name]) == 0:
            rows[name] = defaultdict(list)
        for renderer, wtimes in results.items():
            average, variance = drop_high_low_and_average(wtimes)
            rows[name][renderer].append(average)
            rows[name][renderer].append(variance)

    for name, results in wall_times.items():
        populate_row(rows, name, results)

    for name, results in gpu_times.items():
        populate_row(rows, name, results)

    for name, results in cpu_times.items():
        populate_row(rows, name, results)

    for name, results in gpu_powers.items():
        populate_row(rows, name, results)

    for name, results in cpu_powers.items():
        populate_row(rows, name, results)

    for name, results in gpu_mem_sustaineds.items():
        populate_row(rows, name, results)

    for name, results in gpu_mem_peaks.items():
        populate_row(rows, name, results)

    for name, results in proc_mem_medians.items():
        populate_row(rows, name, results)

    for name, results in proc_mem_peaks.items():
        populate_row(rows, name, results)

    # Generate the SUMMARY output
    summary_file = open("summary." + args.output_tag + ".csv", 'w', newline='')
    summary_writer = csv.writer(summary_file)

    android_version = run_adb_command('shell getprop ro.build.fingerprint').stdout.strip()
    angle_version = run_command('git rev-parse HEAD').stdout.strip()
    # test_time = run_command('date \"+%Y%m%d\"').stdout.read().strip()

    summary_writer.writerow([
        "\"Android: " + android_version + "\n" + "ANGLE: " + angle_version + "\n" +
        #  "Date: " + test_time + "\n" +
        "Source: " + raw_data_filename + "\n" + "Args: " + logged_args() + "\""
    ])

    # Write the summary file
    trace_number = 0

    if (len(renderers) == 1) and (renderers[0] != "both"):
        renderer_name = renderers[0]
        summary_writer.writerow([
            "#", "\"Trace\"", f"\"{renderer_name}\nwall\ntime\nper\nframe\n(ms)\"",
            f"\"{renderer_name}\nwall\ntime\nvariance\"",
            f"\"{renderer_name}\nGPU\ntime\nper\nframe\n(ms)\"",
            f"\"{renderer_name}\nGPU\ntime\nvariance\"",
            f"\"{renderer_name}\nCPU\ntime\nper\nframe\n(ms)\"",
            f"\"{renderer_name}\nCPU\ntime\nvariance\"", f"\"{renderer_name}\nGPU\npower\n(W)\"",
            f"\"{renderer_name}\nGPU\npower\nvariance\"", f"\"{renderer_name}\nCPU\npower\n(W)\"",
            f"\"{renderer_name}\nCPU\npower\nvariance\"", f"\"{renderer_name}\nGPU\nmem\n(B)\"",
            f"\"{renderer_name}\nGPU\nmem\nvariance\"",
            f"\"{renderer_name}\npeak\nGPU\nmem\n(B)\"",
            f"\"{renderer_name}\npeak\nGPU\nmem\nvariance\"",
            f"\"{renderer_name}\nprocess\nmem\n(B)\"",
            f"\"{renderer_name}\nprocess\nmem\nvariance\"",
            f"\"{renderer_name}\npeak\nprocess\nmem\n(B)\"",
            f"\"{renderer_name}\npeak\nprocess\nmem\nvariance\""
        ])

        for name, data in rows.items():
            trace_number += 1
            summary_writer.writerow([
                trace_number,
                name,
                # wall_time
                "%.3f" % data[renderer_name][0],
                percent(data[renderer_name][1]),
                # GPU time
                "%.3f" % data[renderer_name][2],
                percent(data[renderer_name][3]),
                # CPU time
                "%.3f" % data[renderer_name][4],
                percent(data[renderer_name][5]),
                # GPU power
                "%.3f" % data[renderer_name][6],
                percent(data[renderer_name][7]),
                # CPU power
                "%.3f" % data[renderer_name][8],
                percent(data[renderer_name][9]),
                # GPU mem
                int(data[renderer_name][10]),
                percent(data[renderer_name][11]),
                # GPU peak mem
                int(data[renderer_name][12]),
                percent(data[renderer_name][13]),
                # process mem
                int(data[renderer_name][14]),
                percent(data[renderer_name][15]),
                # process peak mem
                int(data[renderer_name][16]),
                percent(data[renderer_name][17]),
            ])
    else:
        summary_writer.writerow([
            "#", "\"Trace\"", "\"Native\nwall\ntime\nper\nframe\n(ms)\"",
            "\"Native\nwall\ntime\nvariance\"", "\"ANGLE\nwall\ntime\nper\nframe\n(ms)\"",
            "\"ANGLE\nwall\ntime\nvariance\"", "\"wall\ntime\ncompare\"",
            "\"Native\nGPU\ntime\nper\nframe\n(ms)\"", "\"Native\nGPU\ntime\nvariance\"",
            "\"ANGLE\nGPU\ntime\nper\nframe\n(ms)\"", "\"ANGLE\nGPU\ntime\nvariance\"",
            "\"GPU\ntime\ncompare\"", "\"Native\nCPU\ntime\nper\nframe\n(ms)\"",
            "\"Native\nCPU\ntime\nvariance\"", "\"ANGLE\nCPU\ntime\nper\nframe\n(ms)\"",
            "\"ANGLE\nCPU\ntime\nvariance\"", "\"CPU\ntime\ncompare\"",
            "\"Native\nGPU\npower\n(W)\"", "\"Native\nGPU\npower\nvariance\"",
            "\"ANGLE\nGPU\npower\n(W)\"", "\"ANGLE\nGPU\npower\nvariance\"",
            "\"GPU\npower\ncompare\"", "\"Native\nCPU\npower\n(W)\"",
            "\"Native\nCPU\npower\nvariance\"", "\"ANGLE\nCPU\npower\n(W)\"",
            "\"ANGLE\nCPU\npower\nvariance\"", "\"CPU\npower\ncompare\"",
            "\"Native\nGPU\nmem\n(B)\"", "\"Native\nGPU\nmem\nvariance\"",
            "\"ANGLE\nGPU\nmem\n(B)\"", "\"ANGLE\nGPU\nmem\nvariance\"", "\"GPU\nmem\ncompare\"",
            "\"Native\npeak\nGPU\nmem\n(B)\"", "\"Native\npeak\nGPU\nmem\nvariance\"",
            "\"ANGLE\npeak\nGPU\nmem\n(B)\"", "\"ANGLE\npeak\nGPU\nmem\nvariance\"",
            "\"GPU\npeak\nmem\ncompare\"", "\"Native\nprocess\nmem\n(B)\"",
            "\"Native\nprocess\nmem\nvariance\"", "\"ANGLE\nprocess\nmem\n(B)\"",
            "\"ANGLE\nprocess\nmem\nvariance\"", "\"process\nmem\ncompare\"",
            "\"Native\npeak\nprocess\nmem\n(B)\"", "\"Native\npeak\nprocess\nmem\nvariance\"",
            "\"ANGLE\npeak\nprocess\nmem\n(B)\"", "\"ANGLE\npeak\nprocess\nmem\nvariance\"",
            "\"process\npeak\nmem\ncompare\""
        ])

        for name, data in rows.items():
            trace_number += 1
            summary_writer.writerow([
                trace_number,
                name,
                # wall_time
                "%.3f" % data["native"][0],
                percent(data["native"][1]),
                "%.3f" % data["vulkan"][0],
                percent(data["vulkan"][1]),
                percent(safe_divide(data["native"][0], data["vulkan"][0])),
                # GPU time
                "%.3f" % data["native"][2],
                percent(data["native"][3]),
                "%.3f" % data["vulkan"][2],
                percent(data["vulkan"][3]),
                percent(safe_divide(data["native"][2], data["vulkan"][2])),
                # CPU time
                "%.3f" % data["native"][4],
                percent(data["native"][5]),
                "%.3f" % data["vulkan"][4],
                percent(data["vulkan"][5]),
                percent(safe_divide(data["native"][4], data["vulkan"][4])),
                # GPU power
                "%.3f" % data["native"][6],
                percent(data["native"][7]),
                "%.3f" % data["vulkan"][6],
                percent(data["vulkan"][7]),
                percent(safe_divide(data["native"][6], data["vulkan"][6])),
                # CPU power
                "%.3f" % data["native"][8],
                percent(data["native"][9]),
                "%.3f" % data["vulkan"][8],
                percent(data["vulkan"][9]),
                percent(safe_divide(data["native"][8], data["vulkan"][8])),
                # GPU mem
                int(data["native"][10]),
                percent(data["native"][11]),
                int(data["vulkan"][10]),
                percent(data["vulkan"][11]),
                percent(safe_divide(data["native"][10], data["vulkan"][10])),
                # GPU peak mem
                int(data["native"][12]),
                percent(data["native"][13]),
                int(data["vulkan"][12]),
                percent(data["vulkan"][13]),
                percent(safe_divide(data["native"][12], data["vulkan"][12])),
                # process mem
                int(data["native"][14]),
                percent(data["native"][15]),
                int(data["vulkan"][14]),
                percent(data["vulkan"][15]),
                percent(safe_divide(data["native"][14], data["vulkan"][14])),
                # process peak mem
                int(data["native"][16]),
                percent(data["native"][17]),
                int(data["vulkan"][16]),
                percent(data["vulkan"][17]),
                percent(safe_divide(data["native"][16], data["vulkan"][16]))
            ])


if __name__ == '__main__':
    sys.exit(main())
