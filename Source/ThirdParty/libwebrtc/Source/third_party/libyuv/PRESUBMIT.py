# Copyright 2017 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# Runs PRESUBMIT.py in py3 mode by git cl presubmit.
USE_PYTHON3 = True

def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(input_api.canned_checks.RunPylint(input_api, output_api,
      files_to_skip=(r'^base[\\\/].*\.py$',
                     r'^build[\\\/].*\.py$',
                     r'^buildtools[\\\/].*\.py$',
                     r'^ios[\\\/].*\.py$',
                     r'^out.*[\\\/].*\.py$',
                     r'^testing[\\\/].*\.py$',
                     r'^third_party[\\\/].*\.py$',
                     r'^tools[\\\/].*\.py$',
                     # TODO(kjellander): should arguably be checked.
                     r'^tools_libyuv[\\\/]valgrind[\\\/].*\.py$',
                     r'^xcodebuild.*[\\\/].*\.py$',),
      disabled_warnings=['F0401',  # Failed to import x
                         'E0611',  # No package y in x
                         'W0232',  # Class has no __init__ method
                        ],
      pylintrc='pylintrc',
      version='2.7'))
  return results


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  results.extend(
      input_api.canned_checks.CheckGNFormatted(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  results.extend(input_api.canned_checks.CheckOwners(input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeWasUploaded(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasDescription(
      input_api, output_api))
  return results
