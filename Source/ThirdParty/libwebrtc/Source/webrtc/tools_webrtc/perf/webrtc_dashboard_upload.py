#!/usr/bin/env python
# Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
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
import os
import sys


def _CreateParser():
  parser = argparse.ArgumentParser()
  parser.add_argument('--perf-dashboard-machine-group', required=True,
                      help='The "master" the bots are grouped under. This '
                           'string is the group in the the perf dashboard path '
                            'group/bot/perf_id/metric/subtest.')
  parser.add_argument('--bot', required=True,
                      help='The bot running the test (e.g. '
                           'webrtc-win-large-tests).')
  parser.add_argument('--test-suite', required=True,
                      help='The key for the test in the dashboard (i.e. what '
                      'you select in the top-level test suite selector in the '
                      'dashboard')
  parser.add_argument('--webrtc-git-hash', required=True,
                      help='webrtc.googlesource.com commit hash.')
  parser.add_argument('--commit-position', type=int, required=True,
                      help='Commit pos corresponding to the git hash.')
  parser.add_argument('--build-page-url', required=True,
                      help='URL to the build page for this build.')
  parser.add_argument('--dashboard-url', required=True,
                      help='Which dashboard to use.')
  parser.add_argument('--input-results-file', type=argparse.FileType(),
                      required=True,
                      help='A JSON file with output from WebRTC tests.')
  parser.add_argument('--output-json-file', type=argparse.FileType('w'),
                      help='Where to write the output (for debugging).')
  parser.add_argument('--outdir', required=True,
                      help='Path to the local out/ dir (usually out/Default)')
  return parser


def _ConfigurePythonPath(options):
  # We just yank the python scripts we require into the PYTHONPATH. You could
  # also imagine a solution where we use for instance protobuf:py_proto_runtime
  # to copy catapult and protobuf code to out/. This is the convention in
  # Chromium and WebRTC python scripts. We do need to build histogram_pb2
  # however, so that's why we add out/ to sys.path below.
  #
  # It would be better if there was an equivalent to py_binary in GN, but
  # there's not.
  script_dir = os.path.dirname(os.path.realpath(__file__))
  checkout_root = os.path.abspath(
      os.path.join(script_dir, os.pardir, os.pardir))

  sys.path.insert(0, os.path.join(checkout_root, 'third_party', 'catapult',
                                  'tracing'))
  sys.path.insert(0, os.path.join(checkout_root, 'third_party', 'protobuf',
                                  'python'))

  # The webrtc_dashboard_upload gn rule will build the protobuf stub for python,
  # so put it in the path for this script before we attempt to import it.
  histogram_proto_path = os.path.join(
      options.outdir, 'pyproto', 'tracing', 'tracing', 'proto')
  sys.path.insert(0, histogram_proto_path)

  # Fail early in case the proto hasn't been built.
  from tracing.proto import histogram_proto
  if not histogram_proto.HAS_PROTO:
    raise ImportError('Could not find histogram_pb2. You need to build the '
                      'webrtc_dashboard_upload target before invoking this '
                      'script. Expected to find '
                      'histogram_pb2.py in %s.' % histogram_proto_path)


def main(args):
  parser = _CreateParser()
  options = parser.parse_args(args)

  _ConfigurePythonPath(options)

  import catapult_uploader

  return catapult_uploader.UploadToDashboard(options)

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
