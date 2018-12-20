# Copyright 2018 The LibYuv Project Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS. All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.


def CheckChangeOnUpload(input_api, output_api):
  return input_api.canned_checks.CheckChangedLUCIConfigs(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return input_api.canned_checks.CheckChangedLUCIConfigs(input_api, output_api)
