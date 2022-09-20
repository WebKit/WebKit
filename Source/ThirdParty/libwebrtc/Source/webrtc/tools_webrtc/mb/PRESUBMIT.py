#!/usr/bin/env vpython3

# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.


# Runs PRESUBMIT.py in py3 mode by git cl presubmit.
USE_PYTHON3 = True


def _CommonChecks(input_api, output_api):
  results = []

  # Run the MB unittests.
  results.extend(
      input_api.canned_checks.RunUnitTestsInDirectory(input_api,
                                                      output_api,
                                                      '.',
                                                      [r'^.+_unittest\.py$'],
                                                      skip_shebang_check=False,
                                                      run_on_python2=False))

  # Validate the format of the mb_config.pyl file.
  cmd = [input_api.python3_executable, 'mb.py', 'validate']
  kwargs = {'cwd': input_api.PresubmitLocalPath()}
  results.extend(input_api.RunTests([
      input_api.Command(name='mb_validate',
                        cmd=cmd, kwargs=kwargs,
                        message=output_api.PresubmitError)]))

  results.extend(
      input_api.canned_checks.CheckLongLines(
          input_api,
          output_api,
          maxlen=80,
          source_file_filter=lambda x: 'mb_config.pyl' in x.LocalPath()))

  return results


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
