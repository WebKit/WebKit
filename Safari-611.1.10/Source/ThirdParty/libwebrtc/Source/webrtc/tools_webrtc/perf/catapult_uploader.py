#!/usr/bin/env python
# Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.


import httplib2
import json
import subprocess
import zlib

from tracing.value import histogram
from tracing.value import histogram_set
from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import reserved_infos


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

  serialized = json.dumps(_ApplyHacks(histograms.AsDicts()), indent=4)

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


# TODO(https://crbug.com/1029452): HACKHACK
# Remove once we have doubles in the proto and handle -infinity correctly.
def _ApplyHacks(dicts):
  for d in dicts:
    if 'running' in d:
      def _NoInf(value):
        if value == float('inf'):
          return histogram.JS_MAX_VALUE
        if value == float('-inf'):
          return -histogram.JS_MAX_VALUE
        return value
      d['running'] = [_NoInf(value) for value in d['running']]

  return dicts


def _LoadHistogramSetFromProto(options):
  hs = histogram_set.HistogramSet()
  with options.input_results_file as f:
    hs.ImportProto(f.read())

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


def _DumpOutput(histograms, output_file):
  with output_file:
    json.dump(_ApplyHacks(histograms.AsDicts()), output_file, indent=4)


def UploadToDashboard(options):
  histograms = _LoadHistogramSetFromProto(options)
  _AddBuildInfo(histograms, options)

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
