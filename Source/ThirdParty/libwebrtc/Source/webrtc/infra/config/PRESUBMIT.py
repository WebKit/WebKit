# Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.


def CheckChangeOnUpload(input_api, output_api):
  return input_api.RunTests(
      input_api.canned_checks.CheckLucicfgGenOutput(
          input_api, output_api,
          'config.star')) + input_api.canned_checks.CheckChangedLUCIConfigs(
              input_api, output_api)
  return res


def CheckChangeOnCommit(input_api, output_api):
  return CheckChangeOnUpload(input_api, output_api)
