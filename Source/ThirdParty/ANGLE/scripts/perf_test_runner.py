#!/usr/bin/python2
#
# Copyright 2015 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# perf_test_runner.py:
#   Helper script for running and analyzing perftest results. Runs the
#   tests in an infinite batch, printing out the mean and coefficient of
#   variation of the population continuously.
#

import glob
import subprocess
import sys
import os
import re

base_path = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

# Look for a [Rr]elease build.
perftests_paths = glob.glob('out/*elease*')
metric = 'wall_time'
max_experiments = 10

binary_name = 'angle_perftests'
if sys.platform == 'win32':
    binary_name += '.exe'

scores = []


# Danke to http://stackoverflow.com/a/27758326
def mean(data):
    """Return the sample arithmetic mean of data."""
    n = len(data)
    if n < 1:
        raise ValueError('mean requires at least one data point')
    return float(sum(data)) / float(n)  # in Python 2 use sum(data)/float(n)


def sum_of_square_deviations(data, c):
    """Return sum of square deviations of sequence data."""
    ss = sum((float(x) - c)**2 for x in data)
    return ss


def coefficient_of_variation(data):
    """Calculates the population coefficient of variation."""
    n = len(data)
    if n < 2:
        raise ValueError('variance requires at least two data points')
    c = mean(data)
    ss = sum_of_square_deviations(data, c)
    pvar = ss / n  # the population variance
    stddev = (pvar**0.5)  # population standard deviation
    return stddev / c


def truncated_list(data, n):
    """Compute a truncated list, n is truncation size"""
    if len(data) < n * 2:
        raise ValueError('list not large enough to truncate')
    return sorted(data)[n:-n]


def truncated_mean(data, n):
    """Compute a truncated mean, n is truncation size"""
    return mean(truncated_list(data, n))


def truncated_cov(data, n):
    """Compute a truncated coefficient of variation, n is truncation size"""
    return coefficient_of_variation(truncated_list(data, n))


# Find most recent binary
newest_binary = None
newest_mtime = None

for path in perftests_paths:
    binary_path = os.path.join(base_path, path, binary_name)
    if os.path.exists(binary_path):
        binary_mtime = os.path.getmtime(binary_path)
        if (newest_binary is None) or (binary_mtime > newest_mtime):
            newest_binary = binary_path
            newest_mtime = binary_mtime

perftests_path = newest_binary

if perftests_path == None or not os.path.exists(perftests_path):
    print('Cannot find Release %s!' % binary_name)
    sys.exit(1)

if sys.platform == 'win32':
    test_name = 'DrawCallPerfBenchmark.Run/d3d11_null'
else:
    test_name = 'DrawCallPerfBenchmark.Run/gl'

if len(sys.argv) >= 2:
    test_name = sys.argv[1]

print('Using test executable: ' + perftests_path)
print('Test name: ' + test_name)


def get_results(metric, extra_args=[]):
    process = subprocess.Popen(
        [perftests_path, '--gtest_filter=' + test_name] + extra_args,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
    output, err = process.communicate()

    m = re.search(r'Running (\d+) tests', output)
    if m and int(m.group(1)) > 1:
        print("Found more than one test result in output:")
        print(output)
        sys.exit(3)

    # Results are reported in the format:
    # name_backend.metric: story= value units.
    pattern = r'\.' + metric + r':.*= ([0-9.]+)'
    m = re.findall(pattern, output)
    if not m:
        print("Did not find the metric '%s' in the test output:" % metric)
        print(output)
        sys.exit(1)

    return [float(value) for value in m]


# Calibrate the number of steps
steps = get_results("steps", ["--calibration"])[0]
print("running with %d steps." % steps)

# Loop 'max_experiments' times, running the tests.
for experiment in range(max_experiments):
    experiment_scores = get_results(metric, ["--steps", str(steps)])

    for score in experiment_scores:
        sys.stdout.write("%s: %.2f" % (metric, score))
        scores.append(score)

        if (len(scores) > 1):
            sys.stdout.write(", mean: %.2f" % mean(scores))
            sys.stdout.write(", variation: %.2f%%" % (coefficient_of_variation(scores) * 100.0))

        if (len(scores) > 7):
            truncation_n = len(scores) >> 3
            sys.stdout.write(", truncated mean: %.2f" % truncated_mean(scores, truncation_n))
            sys.stdout.write(", variation: %.2f%%" % (truncated_cov(scores, truncation_n) * 100.0))

        print("")
