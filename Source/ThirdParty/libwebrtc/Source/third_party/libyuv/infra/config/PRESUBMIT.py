# Copyright 2018 The PDFium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True


def CheckChangeOnUpload(input_api, output_api):
  return input_api.canned_checks.CheckChangedLUCIConfigs(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return input_api.canned_checks.CheckChangedLUCIConfigs(input_api, output_api)
