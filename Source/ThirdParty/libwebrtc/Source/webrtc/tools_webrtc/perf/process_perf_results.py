#!/usr/bin/env vpython3

# Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Adds build info to perf results and uploads them.

The tests don't know which bot executed the tests or at what revision, so we
need to take their output and enrich it with this information. We load the proto
from the tests, add the build information as shared diagnostics and then
upload it to the dashboard.

This script can't be in recipes, because we can't access the catapult APIs from
there. It needs to be here source-side.
"""

import argparse
import json
import os
import sys

from pathlib import Path

# Even if protobuf is not used directly, this allows transitive imports
# of the protobuf library to use the vpython wheel specified in the root
# level .vpython (see bugs.webrtc.org/12211 for context).
import google.protobuf  # pylint: disable=unused-import


def _ConfigurePythonPath(outdir):
  # We just yank the python scripts we require into the PYTHONPATH. You could
  # also imagine a solution where we use for instance
  # protobuf:py_proto_runtime to copy catapult and protobuf code to out/.
  # This is the convention in Chromium and WebRTC python scripts. We do need
  # to build histogram_pb2 however, so that's why we add out/ to sys.path
  # below.
  #
  # It would be better if there was an equivalent to py_binary in GN, but
  # there's not.
  script_dir = os.path.dirname(os.path.realpath(__file__))
  checkout_root = os.path.abspath(os.path.join(script_dir, os.pardir,
                                               os.pardir))

  sys.path.insert(
      0, os.path.join(checkout_root, 'third_party', 'catapult', 'tracing'))
  sys.path.insert(
      0, os.path.join(checkout_root, 'third_party', 'protobuf', 'python'))

  # The webrtc_dashboard_upload gn rule will build the protobuf stub for
  # python, so put it in the path for this script before we attempt to import
  # it.
  histogram_proto_path = os.path.join(outdir, 'pyproto', 'tracing', 'tracing',
                                      'proto')
  sys.path.insert(0, histogram_proto_path)

  # Fail early in case the proto hasn't been built.
  from tracing.proto import histogram_proto
  if not histogram_proto.HAS_PROTO:
    print('Could not find histogram_pb2. You need to build the '
          'webrtc_dashboard_upload target before invoking this '
          'script. Expected to find '
          'histogram_pb2.py in %s.' % histogram_proto_path)
    return 1
  return 0


def _UploadToDasboard(args):
  build_properties = json.loads(args.build_properties)
  exit_code = _ConfigurePythonPath(build_properties['outdir'])
  if exit_code != 0:
    return exit_code

  import catapult_uploader

  perftest_outputs = [
      f.absolute() for f in Path(args.task_output_dir).rglob('perftest-output*')
      if f.is_file()
  ]
  for perftest_output in perftest_outputs:
    uploader_options = catapult_uploader.UploaderOptions(
        perf_dashboard_machine_group=(
            build_properties['perf_dashboard_machine_group']),
        bot=build_properties['bot'],
        webrtc_git_hash=build_properties['webrtc_git_hash'],
        commit_position=build_properties['commit_position'],
        build_page_url=build_properties['build_page_url'],
        dashboard_url=build_properties['dashboard_url'],
        test_suite=args.test_suite,
        input_results_file=perftest_output,
    )
    exit_code = catapult_uploader.UploadToDashboard(uploader_options)
    if exit_code != 0:
      return exit_code
  return 0


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--build-properties', help=argparse.SUPPRESS)
  parser.add_argument('--summary-json', help=argparse.SUPPRESS)
  parser.add_argument('--task-output-dir', help=argparse.SUPPRESS)
  parser.add_argument('--test-suite', help=argparse.SUPPRESS)
  parser.add_argument('-o', '--output-json', help=argparse.SUPPRESS)
  parser.add_argument('json_files', nargs='*', help=argparse.SUPPRESS)
  args = parser.parse_args()

  exit_code = _UploadToDasboard(args)
  if exit_code != 0:
    with open(args.output_json, 'w') as f:
      json.dump({
          "global_tags": ["UNRELIABLE_RESULTS"],
          "missing_shards": [0]
      }, f)
    return exit_code
  return 0


if __name__ == '__main__':
  sys.exit(main())
