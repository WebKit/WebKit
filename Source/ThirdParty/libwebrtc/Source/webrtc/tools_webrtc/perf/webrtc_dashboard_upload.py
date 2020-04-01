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
import httplib2
import json
import os
import sys
import subprocess
import zlib

# We just yank the python scripts we require into the PYTHONPATH. You could also
# imagine a solution where we use for instance protobuf:py_proto_runtime to copy
# catapult and protobuf code to out/, but this approach is allowed by
# convention. Fortunately neither catapult nor protobuf require any build rules
# to be executed. We can't do this for the histogram proto stub though because
# it's generated; see _LoadHistogramSetFromProto.
#
# It would be better if there was an equivalent to py_binary in GN, but there's
# not.
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
CHECKOUT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))
sys.path.insert(0, os.path.join(CHECKOUT_ROOT, 'third_party', 'catapult',
                                'tracing'))
sys.path.insert(0, os.path.join(CHECKOUT_ROOT, 'third_party', 'protobuf',
                                'python'))

from tracing.value import histogram_set
from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import reserved_infos

from google.protobuf import json_format


def _GenerateOauthToken():
  args = ['luci-auth', 'token']
  p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  if p.wait() == 0:
    output = p.stdout.read()
    return output.strip()
  else:
    raise RuntimeError(
        'Error generating authentication token.\nStdout: %s\nStderr:%s' %
        (p.stdout.read(), p.stderr.read()))


def _SendHistogramSet(url, histograms, oauth_token):
  """Make a HTTP POST with the given JSON to the Performance Dashboard.

  Args:
    url: URL of Performance Dashboard instance, e.g.
        "https://chromeperf.appspot.com".
    histograms: a histogram set object that contains the data to be sent.
    oauth_token: An oauth token to use for authorization.
  """
  headers = {'Authorization': 'Bearer %s' % oauth_token}
  serialized = json.dumps(histograms.AsDicts(), indent=4)

  if url.startswith('http://localhost'):
    # The catapult server turns off compression in developer mode.
    data = serialized
  else:
    data = zlib.compress(serialized)

  print 'Sending %d bytes to %s.' % (len(data), url + '/add_histograms')

  http = httplib2.Http()
  response, content = http.request(url + '/add_histograms', method='POST',
                                   body=data, headers=headers)
  return response, content


def _LoadHistogramSetFromProto(options):
  # The webrtc_dashboard_upload gn rule will build the protobuf stub for python,
  # so put it in the path for this script before we attempt to import it.
  histogram_proto_path = os.path.join(options.outdir, 'pyproto', 'tracing',
                                      'tracing', 'proto')
  sys.path.insert(0, histogram_proto_path)

  # TODO(https://crbug.com/1029452): Get rid of this import hack once we can
  # just hand the contents of input_results_file straight to the histogram set.
  try:
    import histogram_pb2
  except ImportError:
    raise ImportError('Could not find histogram_pb2. You need to build the '
                      'webrtc_dashboard_upload target before invoking this '
                      'script. Expected to find '
                      'histogram_pb2 in %s.' % histogram_proto_path)

  with options.input_results_file as f:
    histograms = histogram_pb2.HistogramSet()
    histograms.ParseFromString(f.read())

  # TODO(https://crbug.com/1029452): Don't convert to JSON as a middle step once
  # there is a proto de-serializer ready in catapult.
  json_data = json.loads(json_format.MessageToJson(histograms))
  hs = histogram_set.HistogramSet()
  hs.ImportDicts(json_data)
  return hs


def _AddBuildInfo(histograms, options):
  common_diagnostics = {
      reserved_infos.MASTERS: options.perf_dashboard_machine_group,
      reserved_infos.BOTS: options.bot,
      reserved_infos.POINT_ID: options.commit_position,
      reserved_infos.BENCHMARKS: options.test_suite,
      reserved_infos.WEBRTC_REVISIONS: str(options.webrtc_git_hash),
      reserved_infos.BUILD_URLS: options.build_page_url,
  }

  for k, v in common_diagnostics.items():
    histograms.AddSharedDiagnosticToAllHistograms(
        k.name, generic_set.GenericSet([v]))


# TODO(https://crbug.com/1029452): Remove this once
# https://chromium-review.googlesource.com/c/catapult/+/2094312 lands.
def _HackSummaryOptions(histograms):
  for histogram in histograms:
    histogram.CustomizeSummaryOptions({
      'avg': False,
      'std': False,
      'count': False,
      'sum': False,
      'min': False,
      'max': False,
      'nans': False,
    })


def _DumpOutput(histograms, output_file):
  with output_file:
    json.dump(histograms.AsDicts(), output_file, indent=4)


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


def main(args):
  parser = _CreateParser()
  options = parser.parse_args(args)

  histograms = _LoadHistogramSetFromProto(options)
  _AddBuildInfo(histograms, options)
  _HackSummaryOptions(histograms)

  if options.output_json_file:
    _DumpOutput(histograms, options.output_json_file)

  oauth_token = _GenerateOauthToken()
  response, content = _SendHistogramSet(
      options.dashboard_url, histograms, oauth_token)

  if response.status == 200:
    print 'Received 200 from dashboard.'
    return 0
  else:
    print('Upload failed with %d: %s\n\n%s' % (response.status, response.reason,
                                               content))
    return 1


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
