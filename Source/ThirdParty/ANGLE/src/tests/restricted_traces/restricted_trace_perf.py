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
import subprocess
import sys
import time
import statistics

from collections import defaultdict, namedtuple
from datetime import datetime
from psutil import process_iter

DEFAULT_TEST_DIR = '.'
DEFAULT_TEST_JSON = 'restricted_traces.json'
DEFAULT_LOG_LEVEL = 'info'
SELECTED_DEVICE = ''

Result = namedtuple('Result', ['process', 'time'])

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

    process.wait()

    time_elapsed = time.time() - start_time

    return Result(process, time_elapsed)


def run_async_command(args):
    logging.debug('Kicking off gpumem subprocess %s' % (args))

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


def get_adb_dev():
    return 'adb {} '.format(SELECTED_DEVICE)


def run_adb_command(args):
    return run_command(get_adb_dev() + args)


def run_async_adb_command(args):
    return run_async_command(get_adb_dev() + args)


def cleanup():
    run_adb_command('shell rm -f /sdcard/Download/out.txt /sdcard/Download/gpumem.txt')


def select_device(device_arg):
    # The output from 'adb devices' always includes a header and a new line at the end.
    global SELECTED_DEVICE
    result_dev = run_command('adb devices')
    result_dev_out = result_dev.process.stdout.read().strip()

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
        SELECTED_DEVICE = '-s {}'.format(device_serial)

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
    if mode != '':
        mode = '_' + mode

    # Kick off a subprocess that collects peak gpu memory periodically
    # Note the 0.25 below is the delay (in seconds) between memory checks
    run_adb_command('push gpumem.sh /data/local/tmp')
    memory_command = 'shell sh /data/local/tmp/gpumem.sh 0.25'
    memory_process = run_async_adb_command(memory_command)

    adb_command = 'shell am instrument -w '
    adb_command += '-e org.chromium.native_test.NativeTestInstrumentationTestRunner.StdoutFile /sdcard/Download/out.txt '
    adb_command += '-e org.chromium.native_test.NativeTest.CommandLineFlags "--gtest_filter=TraceTest.' + trace + '\ '
    adb_command += '--use-gl=native\ '
    if args.maxsteps != '':
        adb_command += '--max-steps-performed\ ' + args.maxsteps + '\ '
    if args.fixedtime != '':
        adb_command += '--fixed-test-time\ ' + args.fixedtime + '\ '
    if args.minimizegpuwork:
        adb_command += '--minimize-gpu-work\ '
    adb_command += '--verbose\ '
    adb_command += '--verbose-logging\ '
    adb_command += '--warmup-trials\ 1\"\ '
    adb_command += '-e org.chromium.native_test.NativeTestInstrumentationTestRunner.ShardNanoTimeout "1000000000000000000" '
    adb_command += '-e org.chromium.native_test.NativeTestInstrumentationTestRunner.NativeTestActivity com.android.angle.test.AngleUnitTestActivity '
    adb_command += 'com.android.angle.test/org.chromium.build.gtest_apk.NativeTestInstrumentationTestRunner'

    result = run_adb_command(adb_command)

    logging.debug('Killing gpumem subprocess')
    memory_process.kill()

    return result.time


def get_test_time():
    # Pull the results from the device and parse
    result = run_adb_command('shell cat /sdcard/Download/out.txt | grep -v Error | grep -v Frame')

    measured_time = ''

    while True:
        line = result.process.stdout.readline()
        logging.debug('Checking line: %s' % line)
        if not line:
            break

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
    while True:
        line = result.process.stdout.readline()
        logging.debug('Checking line: %s' % line)
        if not line:
            break

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

    while True:
        # Look for "memory_max" in the line and grab the second to last entry:
        line = result.process.stdout.readline()
        logging.debug('Checking line: %s' % line)
        if not line:
            break

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
    gpu_time = ''

    while True:
        # Look for "gpu_time" in the line and grab the second to last entry:
        line = result.process.stdout.readline()
        logging.debug('Checking line: %s' % line)
        if not line:
            break

        if "gpu_time" in line:
            gpu_time = line.split()[-2]
            break

    return gpu_time


def get_cpu_time():
    # Pull the results from the device and parse
    result = run_adb_command('shell cat /sdcard/Download/out.txt')
    cpu_time = ''

    while True:
        # Look for "cpu_time" in the line and grab the second to last entry:
        line = result.process.stdout.readline()
        logging.debug('Checking line: %s' % line)
        if not line:
            break

        if "cpu_time" in line:
            cpu_time = line.split()[-2]
            break

    return cpu_time


def get_frame_count():
    # Pull the results from the device and parse
    result = run_adb_command('shell cat /sdcard/Download/out.txt | grep -v Error | grep -v Frame')

    frame_count = 0

    while True:
        line = result.process.stdout.readline()
        logging.debug('Checking line: %s' % line)
        if not line:
            break
        if "trial_steps" in line:
            frame_count = line.split()[-2]
            break

    logging.debug('Frame count: %s' % frame_count)
    return frame_count


class GPUPowerStats():

    # GPU power measured in uWs

    def __init__(self):
        self.gpu_power = 0
        self.big_cpu_power = 0
        self.mid_cpu_power = 0
        self.little_cpu_power = 0

    def get_gpu_power(self, device_number):
        gpu_power_command = 'shell "'
        gpu_power_command += 'cat /sys/bus/iio/devices/iio:device'
        gpu_power_command += device_number
        gpu_power_command += '/energy_value'
        gpu_power_command += '"'

        logging.debug("gpu_power_command %s" % gpu_power_command)

        gpu_result = run_adb_command(gpu_power_command)

        # Read the last value from this line:
        # CH6(T=251741617)[S2S_VDD_G3D], 3702041607
        #
        # Out of output like this:
        # t=251741617
        # CH0(T=251741617)[VSYS_PWR_WLAN_BT], 192612469095
        # CH1(T=251741617)[L2S_VDD_AOC_RET], 1393792850
        # CH2(T=251741617)[S9S_VDD_AOC], 16816975638
        # CH3(T=251741617)[S5S_VDDQ_MEM], 2720913855
        # CH4(T=251741617)[S10S_VDD2L], 3656592412
        # CH5(T=251741617)[S4S_VDD2H_MEM], 4597271837
        # CH6(T=251741617)[S2S_VDD_G3D], 3702041607
        # CH7(T=251741617)[L9S_GNSS_CORE], 88535064

        # Read the starting power
        while True:
            line = gpu_result.process.stdout.readline()
            logging.debug('Checking line: %s' % line)
            if not line:
                break
            if "S2S_VDD_G3D" in line:
                self.gpu_power = line.split()[1]
                break

        logging.debug("self.gpu_power %s" % self.gpu_power)

    def get_cpu_power(self, device_number):
        # Also grab the sum of CPU powers
        cpu_power_command = 'shell "'
        cpu_power_command += 'cat /sys/bus/iio/devices/iio:device'
        cpu_power_command += device_number
        cpu_power_command += '/energy_value'
        cpu_power_command += '"'

        cpu_result = run_adb_command(cpu_power_command)

        # Output like this
        # t=16086645
        # CH0(T=16086645)[PPVAR_VSYS_PWR_DISP], 611687448
        # CH1(T=16086645)[VSYS_PWR_MODEM], 179646995
        # CH2(T=16086645)[VSYS_PWR_RFFE], 0
        # CH3(T=16086645)[S2M_VDD_CPUCL2], 124265856
        # CH4(T=16086645)[S3M_VDD_CPUCL1], 170096352
        # CH5(T=16086645)[S4M_VDD_CPUCL0], 289995530
        # CH6(T=16086645)[S5M_VDD_INT], 190164699
        # CH7(T=16086645)[S1M_VDD_MIF], 196512891

        # Sum up the CPU parts
        while True:
            line = cpu_result.process.stdout.readline()
            logging.debug('Checking line: %s' % line)
            if not line:
                break
            if "S2M_VDD_CPUCL2" in line:
                self.big_cpu_power = line.split()[1]
                break
        logging.debug("self.big_cpu_power %s" % self.big_cpu_power)

        while True:
            line = cpu_result.process.stdout.readline()
            logging.debug('Checking line: %s' % line)
            if not line:
                break
            if "S3M_VDD_CPUCL1" in line:
                self.mid_cpu_power = line.split()[1]
                break
        logging.debug("self.mid_cpu_power %s" % self.mid_cpu_power)

        while True:
            line = cpu_result.process.stdout.readline()
            logging.debug('Checking line: %s' % line)
            if not line:
                break
            if "S2M_VDD_CPUCL0" in line:
                self.little_cpu_power = line.split()[1]
                break
        logging.debug("self.little_cpu_power %s" % self.little_cpu_power)

    def get_power_data(self):

        logging.debug('Checking where CPU and GPU data are mapped on enabled_rails')
        enabled_rails_command = 'shell "'
        enabled_rails_command += 'cat /sys/bus/iio/devices/iio:device0/enabled_rails'
        enabled_rails_command += '"'

        enabled_rails_result = run_adb_command(enabled_rails_command)

        # See which rails are on device 0
        rails0 = ''
        while True:
            line = enabled_rails_result.process.stdout.readline()
            logging.debug('Checking line: %s' % line)
            if not line:
                break
            if "S2S_VDD_G3D" in line:
                rails0 = 'GPU'
                break
            if "S2M_VDD_CPUCL2" in line:
                rails0 = 'CPU'
                break

        if "CPU" in rails0:
            logging.debug('CPU is rail 0, GPU is rail 1')
            self.get_cpu_power('0')
            self.get_gpu_power('1')
        elif "GPU" in rails0:
            logging.debug('GPU is rail 0, CPU is rail 1')
            self.get_cpu_power('1')
            self.get_gpu_power('0')
        else:
            logging.error('Failed to select device in get_power_data')
            exit()


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
        '--renderer', help='Which renderer to use: native, vulkan, or both.', default='both')
    parser.add_argument(
        '--walltimeonly',
        help='Limit output to just wall time',
        action='store_true',
        default=False)
    parser.add_argument(
        '--power', help='Include GPU power used per trace', action='store_true', default=False)
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
    elif args.power:
        print('%-*s %-*s %-*s %-*s %-*s %-*s %-*s %-*s %-*s %-*s' %
              (column_width['trace'], 'trace', column_width['wall_time'], 'wall_time',
               column_width['gpu_time'], 'gpu_time', column_width['cpu_time'], 'cpu_time',
               column_width['gpu_power'], 'gpu_power', column_width['cpu_power'], 'cpu_power',
               column_width['gpu_mem_sustained'], 'gpu_mem_sustained',
               column_width['gpu_mem_peak'], 'gpu_mem_peak', column_width['proc_mem_median'],
               'proc_mem_median', column_width['proc_mem_peak'], 'proc_mem_peak'))
        output_writer.writerow([
            'trace', 'wall_time(ms)', 'gpu_time(ms)', 'cpu_time(ms)', 'gpu_power(uWs)',
            'cpu_power(uWs)', 'gpu_mem_sustained', 'gpu_mem_peak', 'proc_mem_median',
            'proc_mem_peak'
        ])
    else:
        print('%-*s %-*s %-*s %-*s %-*s %-*s %-*s %-*s' %
              (column_width['trace'], 'trace', column_width['wall_time'], 'wall_time',
               column_width['gpu_time'], 'gpu_time', column_width['cpu_time'], 'cpu_time',
               column_width['gpu_mem_sustained'], 'gpu_mem_sustained',
               column_width['gpu_mem_peak'], 'gpu_mem_peak', column_width['proc_mem_median'],
               'proc_mem_median', column_width['proc_mem_peak'], 'proc_mem_peak'))

    run_adb_command('root')

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
    gpu_power_per_frames = defaultdict(dict)
    cpu_power_per_frames = defaultdict(dict)
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
        else:
            # Force the settings to ANGLE
            run_adb_command('shell settings put global angle_debug_package org.chromium.angle')
            run_adb_command(
                'shell settings put global angle_gl_driver_selection_pkgs com.android.angle.test')
            run_adb_command('shell settings put global angle_gl_driver_selection_values angle')

        for i in range(int(args.loop_count)):
            print("\nStarting run %i with %s at %s\n" %
                  (i + 1, renderer, datetime.now().strftime('%Y-%m-%d %H:%M:%S')))
            for trace in fnmatch.filter(traces, args.filter):
                # Remove any previous perf results
                cleanup()

                test = trace.split(' ')[0]

                if args.power:
                    starting_power.get_power_data()
                    logging.debug('Starting GPU power: %i' % int(starting_power.gpu_power))
                    logging.debug('Starting big CPU power: %i' % int(starting_power.big_cpu_power))
                    logging.debug('Starting mid CPU power: %i' % int(starting_power.mid_cpu_power))
                    logging.debug('Starting little CPU power: %i' %
                                  int(starting_power.little_cpu_power))

                logging.debug('Running %s' % test)
                test_time = run_trace(test, args)

                if args.power:
                    ending_power.get_power_data()
                    logging.debug('Ending GPU power: %i' % int(ending_power.gpu_power))
                    logging.debug('Ending big CPU power: %i' % int(ending_power.big_cpu_power))
                    logging.debug('Ending mid CPU power: %i' % int(ending_power.mid_cpu_power))
                    logging.debug('Ending little CPU power: %i' %
                                  int(ending_power.little_cpu_power))

                wall_time = get_test_time()

                gpu_time = get_gpu_time() if args.vsync else '0'

                cpu_time = get_cpu_time()

                gpu_power_per_frame = 0
                cpu_power_per_frame = 0

                if args.power:
                    # Report the difference between the start and end power
                    frame_count = int(get_frame_count())
                    if frame_count > 0:
                        consumed_gpu_power = int(ending_power.gpu_power) - int(
                            starting_power.gpu_power)
                        gpu_power_per_frame = consumed_gpu_power / int(frame_count)
                        consumed_big_cpu_power = int(ending_power.big_cpu_power) - int(
                            starting_power.big_cpu_power)
                        consumed_mid_cpu_power = int(ending_power.mid_cpu_power) - int(
                            starting_power.mid_cpu_power)
                        consumed_little_cpu_power = int(ending_power.little_cpu_power) - int(
                            starting_power.little_cpu_power)
                        consumed_cpu_power = consumed_big_cpu_power + consumed_mid_cpu_power + consumed_little_cpu_power
                        cpu_power_per_frame = consumed_cpu_power / int(frame_count)

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

                if len(cpu_times[test]) == 0:
                    cpu_times[test] = defaultdict(list)
                cpu_times[test][renderer].append(safe_cast_float(cpu_time))

                if len(gpu_power_per_frames[test]) == 0:
                    gpu_power_per_frames[test] = defaultdict(list)
                gpu_power_per_frames[test][renderer].append(safe_cast_int(gpu_power_per_frame))

                if len(cpu_power_per_frames[test]) == 0:
                    cpu_power_per_frames[test] = defaultdict(list)
                cpu_power_per_frames[test][renderer].append(safe_cast_int(cpu_power_per_frame))

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
                elif args.power:
                    print(
                        '%-*s %-*s %-*s %-*s %-*i %-*i %-*i %-*i %-*i %-*i' %
                        (column_width['trace'], trace_name, column_width['wall_time'], wall_time,
                         column_width['gpu_time'], gpu_time, column_width['cpu_time'], cpu_time,
                         column_width['gpu_power'], gpu_power_per_frame, column_width['cpu_power'],
                         cpu_power_per_frame, column_width['gpu_mem_sustained'], gpu_mem_sustained,
                         column_width['gpu_mem_peak'], gpu_mem_peak,
                         column_width['proc_mem_median'], proc_mem_median,
                         column_width['proc_mem_peak'], proc_mem_peak))
                    output_writer.writerow([
                        mode + renderer + '_' + test, wall_time, gpu_time, cpu_time,
                        gpu_power_per_frame, cpu_power_per_frame, gpu_mem_sustained, gpu_mem_peak,
                        proc_mem_median, proc_mem_peak
                    ])
                else:
                    print('%-*s %-*s %-*s %-*s %-*i %-*i %-*i %-*i' %
                          (column_width['trace'], trace_name, column_width['wall_time'], wall_time,
                           column_width['gpu_time'], gpu_time, column_width['cpu_time'], cpu_time,
                           column_width['gpu_mem_sustained'], gpu_mem_sustained,
                           column_width['gpu_mem_peak'], gpu_mem_peak,
                           column_width['proc_mem_median'], proc_mem_median,
                           column_width['proc_mem_peak'], proc_mem_peak))


                # Early exit for testing
                #exit()

                # For unlocked clocks, try sleeping the same amount of time that the trace ran, to dissipate heat
                if args.sleep != 0:
                    time.sleep(args.sleep)

    # Generate the SUMMARY output

    summary_file = open("summary." + args.output_tag + ".csv", 'w', newline='')
    summary_writer = csv.writer(summary_file)

    android_result = run_adb_command('shell getprop ro.build.fingerprint')
    android_version = android_result.process.stdout.read().strip()

    angle_result = run_command('git rev-parse HEAD')
    angle_version = angle_result.process.stdout.read().strip()
    # test_time = run_command('date \"+%Y%m%d\"').stdout.read().strip()

    summary_writer.writerow([
        "\"Android: " + android_version + "\n" + "ANGLE: " + angle_version + "\n" +
        #  "Date: " + test_time + "\n" +
        "Source: " + raw_data_filename + "\n" + "\""
    ])
    summary_writer.writerow([
        "#", "\"Trace\"", "\"Native\nwall\ntime\nper\nframe\n(ms)\"",
        "\"Native\nwall\ntime\nvariance\"", "\"ANGLE\nwall\ntime\nper\nframe\n(ms)\"",
        "\"ANGLE\nwall\ntime\nvariance\"", "\"wall\ntime\ncompare\"",
        "\"Native\nGPU\ntime\nper\nframe\n(ms)\"", "\"Native\nGPU\ntime\nvariance\"",
        "\"ANGLE\nGPU\ntime\nper\nframe\n(ms)\"", "\"ANGLE\nGPU\ntime\nvariance\"",
        "\"GPU\ntime\ncompare\"", "\"Native\nCPU\ntime\nper\nframe\n(ms)\"",
        "\"Native\nCPU\ntime\nvariance\"", "\"ANGLE\nCPU\ntime\nper\nframe\n(ms)\"",
        "\"ANGLE\nCPU\ntime\nvariance\"", "\"CPU\ntime\ncompare\"",
        "\"Native\nGPU\npower\nper\nframe\n(uWs)\"", "\"Native\nGPU\npower\nvariance\"",
        "\"ANGLE\nGPU\npower\nper\nframe\n(uWs)\"", "\"ANGLE\nGPU\npower\nvariance\"",
        "\"GPU\npower\ncompare\"", "\"Native\nCPU\npower\nper\nframe\n(uWs)\"",
        "\"Native\nCPU\npower\nvariance\"", "\"ANGLE\nCPU\npower\nper\nframe\n(uWs)\"",
        "\"ANGLE\nCPU\npower\nvariance\"", "\"CPU\npower\ncompare\"", "\"Native\nGPU\nmem\n(B)\"",
        "\"Native\nGPU\nmem\nvariance\"", "\"ANGLE\nGPU\nmem\n(B)\"",
        "\"ANGLE\nGPU\nmem\nvariance\"", "\"GPU\nmem\ncompare\"",
        "\"Native\npeak\nGPU\nmem\n(B)\"", "\"Native\npeak\nGPU\nmem\nvariance\"",
        "\"ANGLE\npeak\nGPU\nmem\n(B)\"", "\"ANGLE\npeak\nGPU\nmem\nvariance\"",
        "\"GPU\npeak\nmem\ncompare\"", "\"Native\nprocess\nmem\n(B)\"",
        "\"Native\nprocess\nmem\nvariance\"", "\"ANGLE\nprocess\nmem\n(B)\"",
        "\"ANGLE\nprocess\nmem\nvariance\"", "\"process\nmem\ncompare\"",
        "\"Native\npeak\nprocess\nmem\n(B)\"", "\"Native\npeak\nprocess\nmem\nvariance\"",
        "\"ANGLE\npeak\nprocess\nmem\n(B)\"", "\"ANGLE\npeak\nprocess\nmem\nvariance\"",
        "\"process\npeak\nmem\ncompare\""
    ])

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

    for name, results in gpu_power_per_frames.items():
        populate_row(rows, name, results)

    for name, results in cpu_power_per_frames.items():
        populate_row(rows, name, results)

    for name, results in gpu_mem_sustaineds.items():
        populate_row(rows, name, results)

    for name, results in gpu_mem_peaks.items():
        populate_row(rows, name, results)

    for name, results in proc_mem_medians.items():
        populate_row(rows, name, results)

    for name, results in proc_mem_peaks.items():
        populate_row(rows, name, results)

    for name, data in rows.items():
        if "native" in data and "vulkan" in data:
            # The remaining code in this script expects both native and vulkan results
            break
        else:
            logging.info("Skipping summary file due to single renderer")
            exit()

    # Write the summary file
    trace_number = 0
    for name, data in rows.items():
        trace_number += 1
        summary_writer.writerow([
            trace_number, name,
            "%.3f" % data["native"][0],
            percent(data["native"][1]),
            "%.3f" % data["vulkan"][0],
            percent(data["vulkan"][1]),
            percent(safe_divide(data["native"][0], data["vulkan"][0])),
            "%.3f" % data["native"][2],
            percent(data["native"][3]),
            "%.3f" % data["vulkan"][2],
            percent(data["vulkan"][3]),
            percent(safe_divide(data["native"][2], data["vulkan"][2])),
            "%.3f" % data["native"][4],
            percent(data["native"][5]),
            "%.3f" % data["vulkan"][4],
            percent(data["vulkan"][5]),
            percent(safe_divide(data["native"][4], data["vulkan"][4])),
            int(data["native"][6]),
            percent(data["native"][7]),
            int(data["vulkan"][6]),
            percent(data["vulkan"][7]),
            percent(safe_divide(data["native"][6], data["vulkan"][6])),
            int(data["native"][8]),
            percent(data["native"][9]),
            int(data["vulkan"][8]),
            percent(data["vulkan"][9]),
            percent(safe_divide(data["native"][8], data["vulkan"][8])),
            int(data["native"][10]),
            percent(data["native"][11]),
            int(data["vulkan"][10]),
            percent(data["vulkan"][11]),
            percent(safe_divide(data["native"][10], data["vulkan"][10])),
            int(data["native"][12]),
            percent(data["native"][13]),
            int(data["vulkan"][12]),
            percent(data["vulkan"][13]),
            percent(safe_divide(data["native"][12], data["vulkan"][12])),
            int(data["native"][14]),
            percent(data["native"][15]),
            int(data["vulkan"][14]),
            percent(data["vulkan"][15]),
            percent(safe_divide(data["native"][14], data["vulkan"][14])),
            int(data["native"][16]),
            percent(data["native"][17]),
            int(data["vulkan"][16]),
            percent(data["vulkan"][17]),
            percent(safe_divide(data["native"][16], data["vulkan"][16]))
        ])

    # Clean up settings
    run_adb_command('shell settings delete global angle_debug_package')
    run_adb_command('shell settings delete global angle_gl_driver_selection_pkgs')
    run_adb_command('shell settings delete global angle_gl_driver_selection_values')

    return 0


if __name__ == '__main__':
    sys.exit(main())
