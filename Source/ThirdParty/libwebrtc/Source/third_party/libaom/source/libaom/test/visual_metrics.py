#!/usr/bin/python
#
# Copyright (c) 2016, Alliance for Open Media. All rights reserved.
#
# This source code is subject to the terms of the BSD 2 Clause License and
# the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
# was not distributed with this source code in the LICENSE file, you can
# obtain it at www.aomedia.org/license/software. If the Alliance for Open
# Media Patent License 1.0 was not distributed with this source code in the
# PATENTS file, you can obtain it at www.aomedia.org/license/patent.
#

"""Converts video encoding result data from text files to visualization
data source."""

__author__ = "jzern@google.com (James Zern),"
__author__ += "jimbankoski@google.com (Jim Bankoski)"

import fnmatch
import numpy as np
import scipy as sp
import scipy.interpolate
import os
import re
import string
import sys
import math
import warnings

import gviz_api

from os.path import basename
from os.path import splitext

warnings.simplefilter('ignore', np.RankWarning)
warnings.simplefilter('ignore', RuntimeWarning)

def bdsnr2(metric_set1, metric_set2):
  """
  BJONTEGAARD    Bjontegaard metric calculation adapted
  Bjontegaard's snr metric allows to compute the average % saving in decibels
  between two rate-distortion curves [1].  This is an adaptation of that
  method that fixes inconsistencies when the curve fit operation goes awry
  by replacing the curve fit function with a Piecewise Cubic Hermite
  Interpolating Polynomial and then integrating that by evaluating that
  function at small intervals using the trapezoid method to calculate
  the integral.

  metric_set1 - list of tuples ( bitrate,  metric ) for first graph
  metric_set2 - list of tuples ( bitrate,  metric ) for second graph
  """

  if not metric_set1 or not metric_set2:
    return 0.0

  try:

    # pchip_interlopate requires keys sorted by x axis. x-axis will
    # be our metric not the bitrate so sort by metric.
    metric_set1.sort()
    metric_set2.sort()

    # Pull the log of the rate and clamped psnr from metric_sets.
    log_rate1 = [math.log(x[0]) for x in metric_set1]
    metric1 = [100.0 if x[1] == float('inf') else x[1] for x in metric_set1]
    log_rate2 = [math.log(x[0]) for x in metric_set2]
    metric2 = [100.0 if x[1] == float('inf') else x[1] for x in metric_set2]

    # Integration interval.  This metric only works on the area that's
    # overlapping.   Extrapolation of these things is sketchy so we avoid.
    min_int = max([min(log_rate1), min(log_rate2)])
    max_int = min([max(log_rate1), max(log_rate2)])

    # No overlap means no sensible metric possible.
    if max_int <= min_int:
      return 0.0

    # Use Piecewise Cubic Hermite Interpolating Polynomial interpolation to
    # create 100 new samples points separated by interval.
    lin = np.linspace(min_int, max_int, num=100, retstep=True)
    interval = lin[1]
    samples = lin[0]
    v1 = scipy.interpolate.pchip_interpolate(log_rate1, metric1, samples)
    v2 = scipy.interpolate.pchip_interpolate(log_rate2, metric2, samples)

    # Calculate the integral using the trapezoid method on the samples.
    int_v1 = np.trapz(v1, dx=interval)
    int_v2 = np.trapz(v2, dx=interval)

    # Calculate the average improvement.
    avg_exp_diff = (int_v2 - int_v1) / (max_int - min_int)

  except (TypeError, ZeroDivisionError, ValueError, np.RankWarning) as e:
    return 0

  return avg_exp_diff

def bdrate2(metric_set1, metric_set2):
  """
  BJONTEGAARD    Bjontegaard metric calculation adapted
  Bjontegaard's metric allows to compute the average % saving in bitrate
  between two rate-distortion curves [1].  This is an adaptation of that
  method that fixes inconsistencies when the curve fit operation goes awry
  by replacing the curve fit function with a Piecewise Cubic Hermite
  Interpolating Polynomial and then integrating that by evaluating that
  function at small intervals using the trapezoid method to calculate
  the integral.

  metric_set1 - list of tuples ( bitrate,  metric ) for first graph
  metric_set2 - list of tuples ( bitrate,  metric ) for second graph
  """

  if not metric_set1 or not metric_set2:
    return 0.0

  try:

    # pchip_interlopate requires keys sorted by x axis. x-axis will
    # be our metric not the bitrate so sort by metric.
    metric_set1.sort(key=lambda tup: tup[1])
    metric_set2.sort(key=lambda tup: tup[1])

    # Pull the log of the rate and clamped psnr from metric_sets.
    log_rate1 = [math.log(x[0]) for x in metric_set1]
    metric1 = [100.0 if x[1] == float('inf') else x[1] for x in metric_set1]
    log_rate2 = [math.log(x[0]) for x in metric_set2]
    metric2 = [100.0 if x[1] == float('inf') else x[1] for x in metric_set2]

    # Integration interval.  This metric only works on the area that's
    # overlapping.   Extrapolation of these things is sketchy so we avoid.
    min_int = max([min(metric1), min(metric2)])
    max_int = min([max(metric1), max(metric2)])

    # No overlap means no sensible metric possible.
    if max_int <= min_int:
      return 0.0

    # Use Piecewise Cubic Hermite Interpolating Polynomial interpolation to
    # create 100 new samples points separated by interval.
    lin = np.linspace(min_int, max_int, num=100, retstep=True)
    interval = lin[1]
    samples = lin[0]
    v1 = scipy.interpolate.pchip_interpolate(metric1, log_rate1, samples)
    v2 = scipy.interpolate.pchip_interpolate(metric2, log_rate2, samples)

    # Calculate the integral using the trapezoid method on the samples.
    int_v1 = np.trapz(v1, dx=interval)
    int_v2 = np.trapz(v2, dx=interval)

    # Calculate the average improvement.
    avg_exp_diff = (int_v2 - int_v1) / (max_int - min_int)

  except (TypeError, ZeroDivisionError, ValueError, np.RankWarning) as e:
    return 0

  # Convert to a percentage.
  avg_diff = (math.exp(avg_exp_diff) - 1) * 100

  return avg_diff



def FillForm(string_for_substitution, dictionary_of_vars):
  """
  This function substitutes all matches of the command string //%% ... %%//
  with the variable represented by ...  .
  """
  return_string = string_for_substitution
  for i in re.findall("//%%(.*)%%//", string_for_substitution):
    return_string = re.sub("//%%" + i + "%%//", dictionary_of_vars[i],
                           return_string)
  return return_string


def HasMetrics(line):
  """
  The metrics files produced by aomenc are started with a B for headers.
  """
  # If the first char of the first word on the line is a digit
  if len(line) == 0:
    return False
  if len(line.split()) == 0:
    return False
  if line.split()[0][0:1].isdigit():
    return True
  return False

def GetMetrics(file_name):
  metric_file = open(file_name, "r")
  return metric_file.readline().split();

def ParseMetricFile(file_name, metric_column):
  metric_set1 = set([])
  metric_file = open(file_name, "r")
  for line in metric_file:
    metrics = string.split(line)
    if HasMetrics(line):
      if metric_column < len(metrics):
        try:
          tuple = float(metrics[0]), float(metrics[metric_column])
        except:
          tuple = float(metrics[0]), 0
      else:
        tuple = float(metrics[0]), 0
      metric_set1.add(tuple)
  metric_set1_sorted = sorted(metric_set1)
  return metric_set1_sorted


def FileBetter(file_name_1, file_name_2, metric_column, method):
  """
  Compares two data files and determines which is better and by how
  much. Also produces a histogram of how much better, by PSNR.
  metric_column is the metric.
  """
  # Store and parse our two files into lists of unique tuples.

  # Read the two files, parsing out lines starting with bitrate.
  metric_set1_sorted = ParseMetricFile(file_name_1, metric_column)
  metric_set2_sorted = ParseMetricFile(file_name_2, metric_column)


  def GraphBetter(metric_set1_sorted, metric_set2_sorted, base_is_set_2):
    """
    Search through the sorted metric file for metrics on either side of
    the metric from file 1.  Since both lists are sorted we really
    should not have to search through the entire range, but these
    are small files."""
    total_bitrate_difference_ratio = 0.0
    count = 0
    for bitrate, metric in metric_set1_sorted:
      if bitrate == 0:
        continue
      for i in range(len(metric_set2_sorted) - 1):
        s2_bitrate_0, s2_metric_0 = metric_set2_sorted[i]
        s2_bitrate_1, s2_metric_1 = metric_set2_sorted[i + 1]
        # We have a point on either side of our metric range.
        if metric > s2_metric_0 and metric <= s2_metric_1:

          # Calculate a slope.
          if s2_metric_1 - s2_metric_0 != 0:
            metric_slope = ((s2_bitrate_1 - s2_bitrate_0) /
                            (s2_metric_1 - s2_metric_0))
          else:
            metric_slope = 0

          estimated_s2_bitrate = (s2_bitrate_0 + (metric - s2_metric_0) *
                                  metric_slope)

          if estimated_s2_bitrate == 0:
            continue
          # Calculate percentage difference as given by base.
          if base_is_set_2 == 0:
            bitrate_difference_ratio = ((bitrate - estimated_s2_bitrate) /
                                        bitrate)
          else:
            bitrate_difference_ratio = ((bitrate - estimated_s2_bitrate) /
                                        estimated_s2_bitrate)

          total_bitrate_difference_ratio += bitrate_difference_ratio
          count += 1
          break

    # Calculate the average improvement between graphs.
    if count != 0:
      avg = total_bitrate_difference_ratio / count

    else:
      avg = 0.0

    return avg

  # Be fair to both graphs by testing all the points in each.
  if method == 'avg':
    avg_improvement = 50 * (
                       GraphBetter(metric_set1_sorted, metric_set2_sorted, 1) -
                       GraphBetter(metric_set2_sorted, metric_set1_sorted, 0))
  elif method == 'dsnr':
      avg_improvement = bdsnr2(metric_set1_sorted, metric_set2_sorted)
  else:
      avg_improvement = bdrate2(metric_set2_sorted, metric_set1_sorted)

  return avg_improvement


def HandleFiles(variables):
  """
  This script creates html for displaying metric data produced from data
  in a video stats file,  as created by the AOM project when enable_psnr
  is turned on:

  Usage: visual_metrics.py template.html pattern base_dir sub_dir [ sub_dir2 ..]

  The script parses each metrics file [see below] that matches the
  statfile_pattern  in the baseline directory and looks for the file that
  matches that same file in each of the sub_dirs, and compares the resultant
  metrics bitrate, avg psnr, glb psnr, and ssim. "

  It provides a table in which each row is a file in the line directory,
  and a column for each subdir, with the cells representing how that clip
  compares to baseline for that subdir.   A graph is given for each which
  compares filesize to that metric.  If you click on a point in the graph it
  zooms in on that point.

  a SAMPLE metrics file:

  Bitrate  AVGPsnr  GLBPsnr  AVPsnrP  GLPsnrP  VPXSSIM    Time(us)
   25.911   38.242   38.104   38.258   38.121   75.790    14103
  Bitrate  AVGPsnr  GLBPsnr  AVPsnrP  GLPsnrP  VPXSSIM    Time(us)
   49.982   41.264   41.129   41.255   41.122   83.993    19817
  Bitrate  AVGPsnr  GLBPsnr  AVPsnrP  GLPsnrP  VPXSSIM    Time(us)
   74.967   42.911   42.767   42.899   42.756   87.928    17332
  Bitrate  AVGPsnr  GLBPsnr  AVPsnrP  GLPsnrP  VPXSSIM    Time(us)
  100.012   43.983   43.838   43.881   43.738   89.695    25389
  Bitrate  AVGPsnr  GLBPsnr  AVPsnrP  GLPsnrP  VPXSSIM    Time(us)
  149.980   45.338   45.203   45.184   45.043   91.591    25438
  Bitrate  AVGPsnr  GLBPsnr  AVPsnrP  GLPsnrP  VPXSSIM    Time(us)
  199.852   46.225   46.123   46.113   45.999   92.679    28302
  Bitrate  AVGPsnr  GLBPsnr  AVPsnrP  GLPsnrP  VPXSSIM    Time(us)
  249.922   46.864   46.773   46.777   46.673   93.334    27244
  Bitrate  AVGPsnr  GLBPsnr  AVPsnrP  GLPsnrP  VPXSSIM    Time(us)
  299.998   47.366   47.281   47.317   47.220   93.844    27137
  Bitrate  AVGPsnr  GLBPsnr  AVPsnrP  GLPsnrP  VPXSSIM    Time(us)
  349.769   47.746   47.677   47.722   47.648   94.178    32226
  Bitrate  AVGPsnr  GLBPsnr  AVPsnrP  GLPsnrP  VPXSSIM    Time(us)
  399.773   48.032   47.971   48.013   47.946   94.362    36203

  sample use:
  visual_metrics.py template.html "*stt" aom aom_b aom_c > metrics.html
  """

  # The template file is the html file into which we will write the
  # data from the stats file, formatted correctly for the gviz_api.
  template_file = open(variables[1], "r")
  page_template = template_file.read()
  template_file.close()

  # This is the path match pattern for finding stats files amongst
  # all the other files it could be.  eg: *.stt
  file_pattern = variables[2]

  # This is the directory with files that we will use to do the comparison
  # against.
  baseline_dir = variables[3]
  snrs = ''
  filestable = {}

  filestable['dsnr'] = ''
  filestable['drate'] = ''
  filestable['avg'] = ''

  # Dirs is directories after the baseline to compare to the base.
  dirs = variables[4:len(variables)]

  # Find the metric files in the baseline directory.
  dir_list = sorted(fnmatch.filter(os.listdir(baseline_dir), file_pattern))

  metrics = GetMetrics(baseline_dir + "/" + dir_list[0])

  metrics_js = 'metrics = ["' + '", "'.join(metrics) + '"];'

  for column in range(1, len(metrics)):

    for metric in ['avg','dsnr','drate']:
      description = {"file": ("string", "File")}

      # Go through each directory and add a column header to our description.
      countoverall = {}
      sumoverall = {}

      for directory in dirs:
        description[directory] = ("number", directory)
        countoverall[directory] = 0
        sumoverall[directory] = 0

      # Data holds the data for the visualization, name given comes from
      # gviz_api sample code.
      data = []
      for filename in dir_list:
        row = {'file': splitext(basename(filename))[0] }
        baseline_file_name = baseline_dir + "/" + filename

        # Read the metric file from each of the directories in our list.
        for directory in dirs:
          metric_file_name = directory + "/" + filename

          # If there is a metric file in the current directory, open it
          # and calculate its overall difference between it and the baseline
          # directory's metric file.
          if os.path.isfile(metric_file_name):
            overall = FileBetter(baseline_file_name, metric_file_name,
                                 column, metric)
            row[directory] = overall

            sumoverall[directory] += overall
            countoverall[directory] += 1

        data.append(row)

      # Add the overall numbers.
      row = {"file": "OVERALL" }
      for directory in dirs:
        row[directory] = sumoverall[directory] / countoverall[directory]
      data.append(row)

      # write the tables out
      data_table = gviz_api.DataTable(description)
      data_table.LoadData(data)

      filestable[metric] = ( filestable[metric] + "filestable_" + metric +
                             "[" + str(column) + "]=" +
                             data_table.ToJSon(columns_order=["file"]+dirs) + "\n" )

    filestable_avg = filestable['avg']
    filestable_dpsnr = filestable['dsnr']
    filestable_drate = filestable['drate']

    # Now we collect all the data for all the graphs.  First the column
    # headers which will be Datarate and then each directory.
    columns = ("datarate",baseline_dir)
    description = {"datarate":("number", "Datarate")}
    for directory in dirs:
      description[directory] = ("number", directory)

    description[baseline_dir] = ("number", baseline_dir)

    snrs = snrs + "snrs[" + str(column) + "] = ["

    # Now collect the data for the graphs, file by file.
    for filename in dir_list:

      data = []

      # Collect the file in each directory and store all of its metrics
      # in the associated gviz metrics table.
      all_dirs = dirs + [baseline_dir]
      for directory in all_dirs:

        metric_file_name = directory + "/" + filename
        if not os.path.isfile(metric_file_name):
          continue

        # Read and parse the metrics file storing it to the data we'll
        # use for the gviz_api.Datatable.
        metrics = ParseMetricFile(metric_file_name, column)
        for bitrate, metric in metrics:
          data.append({"datarate": bitrate, directory: metric})

      data_table = gviz_api.DataTable(description)
      data_table.LoadData(data)
      snrs = snrs + "'" + data_table.ToJSon(
         columns_order=tuple(["datarate",baseline_dir]+dirs)) + "',"

    snrs = snrs + "]\n"

    formatters = ""
    for i in range(len(dirs)):
      formatters = "%s   formatter.format(better, %d);" % (formatters, i+1)

  print FillForm(page_template, vars())
  return

if len(sys.argv) < 3:
  print HandleFiles.__doc__
else:
  HandleFiles(sys.argv)
