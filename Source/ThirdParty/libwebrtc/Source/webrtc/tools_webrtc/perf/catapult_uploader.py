#!/usr/bin/env vpython3

# Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import datetime
import json
import subprocess
import time
import zlib

from typing import Optional
import dataclasses
import httplib2

from tracing.value import histogram
from tracing.value import histogram_set
from tracing.value.diagnostics import generic_set
from tracing.value.diagnostics import reserved_infos


@dataclasses.dataclass
class UploaderOptions():
  """Required information to upload perf metrics.

    Attributes:
      perf_dashboard_machine_group: The "master" the bots are grouped under.
        This string is the group in the the perf dashboard path
        group/bot/perf_id/metric/subtest.
      bot: The bot running the test (e.g. webrtc-win-large-tests).
      test_suite: The key for the test in the dashboard (i.e. what you select
        in the top-level test suite selector in the dashboard
      webrtc_git_hash: webrtc.googlesource.com commit hash.
      commit_position: Commit pos corresponding to the git hash.
      build_page_url: URL to the build page for this build.
      dashboard_url: Which dashboard to use.
      input_results_file: A HistogramSet proto file coming from WebRTC tests.
      output_json_file: Where to write the output (for debugging).
      wait_timeout_sec: Maximum amount of time in seconds that the script will
        wait for the confirmation.
      wait_polling_period_sec: Status will be requested from the Dashboard
        every wait_polling_period_sec seconds.
    """
  perf_dashboard_machine_group: str
  bot: str
  test_suite: str
  webrtc_git_hash: str
  commit_position: int
  build_page_url: str
  dashboard_url: str
  input_results_file: str
  output_json_file: Optional[str] = None
  wait_timeout_sec: datetime.timedelta = datetime.timedelta(seconds=1200)
  wait_polling_period_sec: datetime.timedelta = datetime.timedelta(seconds=120)


def _GenerateOauthToken():
  args = ['luci-auth', 'token']
  p = subprocess.Popen(args,
                       universal_newlines=True,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
  if p.wait() == 0:
    output = p.stdout.read()
    return output.strip()
  raise RuntimeError(
      'Error generating authentication token.\nStdout: %s\nStderr:%s' %
      (p.stdout.read(), p.stderr.read()))


def _CreateHeaders(oauth_token):
  return {'Authorization': 'Bearer %s' % oauth_token}


def _SendHistogramSet(url, histograms):
  """Make a HTTP POST with the given JSON to the Performance Dashboard.

    Args:
      url: URL of Performance Dashboard instance, e.g.
          "https://chromeperf.appspot.com".
      histograms: a histogram set object that contains the data to be sent.
    """
  headers = _CreateHeaders(_GenerateOauthToken())

  serialized = json.dumps(_ApplyHacks(histograms.AsDicts()), indent=4)

  if url.startswith('http://localhost'):
    # The catapult server turns off compression in developer mode.
    data = serialized
  else:
    data = zlib.compress(serialized.encode('utf-8'))

  print('Sending %d bytes to %s.' % (len(data), url + '/add_histograms'))

  http = httplib2.Http()
  response, content = http.request(url + '/add_histograms',
                                   method='POST',
                                   body=data,
                                   headers=headers)
  return response, content


def _WaitForUploadConfirmation(url, upload_token, wait_timeout,
                               wait_polling_period):
  """Make a HTTP GET requests to the Performance Dashboard untill upload
    status is known or the time is out.

    Args:
      url: URL of Performance Dashboard instance, e.g.
          "https://chromeperf.appspot.com".
      upload_token: String that identifies Performance Dashboard and can be used
        for the status check.
      wait_timeout: (datetime.timedelta) Maximum time to wait for the
        confirmation.
      wait_polling_period: (datetime.timedelta) Performance Dashboard will be
        polled every wait_polling_period amount of time.
    """
  assert wait_polling_period <= wait_timeout

  headers = _CreateHeaders(_GenerateOauthToken())
  http = httplib2.Http()

  oauth_refreshed = False
  response = None
  resp_json = None
  current_time = datetime.datetime.now()
  end_time = current_time + wait_timeout
  next_poll_time = current_time + wait_polling_period
  while datetime.datetime.now() < end_time:
    current_time = datetime.datetime.now()
    if next_poll_time > current_time:
      time.sleep((next_poll_time - current_time).total_seconds())
    next_poll_time = datetime.datetime.now() + wait_polling_period

    response, content = http.request(url + '/uploads/' + upload_token,
                                     method='GET',
                                     headers=headers)

    print('Upload state polled. Response: %r.' % content)

    if not oauth_refreshed and response.status == 403:
      print('Oauth token refreshed. Continue polling.')
      headers = _CreateHeaders(_GenerateOauthToken())
      oauth_refreshed = True
      continue

    if response.status != 200:
      break

    resp_json = json.loads(content)
    if resp_json['state'] == 'COMPLETED' or resp_json['state'] == 'FAILED':
      break

  return response, resp_json


# Because of an issues on the Dashboard side few measurements over a large set
# can fail to upload. That would lead to the whole upload to be marked as
# failed. Check it, so it doesn't increase flakiness of our tests.
# TODO(crbug.com/1145904): Remove check after fixed.
def _CheckFullUploadInfo(url, upload_token,
                         min_measurements_amount=50,
                         max_failed_measurements_percent=0.03):
  """Make a HTTP GET requests to the Performance Dashboard to get full info
    about upload (including measurements). Checks if upload is correct despite
    not having status "COMPLETED".

    Args:
      url: URL of Performance Dashboard instance, e.g.
          "https://chromeperf.appspot.com".
      upload_token: String that identifies Performance Dashboard and can be used
        for the status check.
      min_measurements_amount: minimal amount of measurements that the upload
        should have to start tolerating failures in particular measurements.
      max_failed_measurements_percent: maximal percent of failured measurements
        to tolerate.
    """
  headers = _CreateHeaders(_GenerateOauthToken())
  http = httplib2.Http()

  response, content = http.request(url + '/uploads/' + upload_token +
                                   '?additional_info=measurements',
                                   method='GET',
                                   headers=headers)

  if response.status != 200:
    print('Failed to reach the dashboard to get full upload info.')
    return False

  resp_json = json.loads(content)
  print('Full upload info: %s.' % json.dumps(resp_json, indent=4))

  if 'measurements' in resp_json:
    measurements_cnt = len(resp_json['measurements'])
    not_completed_state_cnt = len(
        [m for m in resp_json['measurements'] if m['state'] != 'COMPLETED'])

    if (measurements_cnt >= min_measurements_amount
        and (not_completed_state_cnt /
             (measurements_cnt * 1.0) <= max_failed_measurements_percent)):
      print(('Not all measurements were confirmed to upload. '
             'Measurements count: %d, failed to upload or timed out: %d' %
             (measurements_cnt, not_completed_state_cnt)))
      return True

  return False


# TODO(https://crbug.com/1029452): HACKHACK
# Remove once we have doubles in the proto and handle -infinity correctly.
def _ApplyHacks(dicts):
  def _NoInf(value):
    if value == float('inf'):
      return histogram.JS_MAX_VALUE
    if value == float('-inf'):
      return -histogram.JS_MAX_VALUE
    return value

  for d in dicts:
    if 'running' in d:
      d['running'] = [_NoInf(value) for value in d['running']]
    if 'sampleValues' in d:
      d['sampleValues'] = [_NoInf(value) for value in d['sampleValues']]

  return dicts


def _LoadHistogramSetFromProto(options):
  hs = histogram_set.HistogramSet()
  with open(options.input_results_file, 'rb') as f:
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

  for k, v in list(common_diagnostics.items()):
    histograms.AddSharedDiagnosticToAllHistograms(k.name,
                                                  generic_set.GenericSet([v]))


def _DumpOutput(histograms, output_file):
  with open(output_file, 'w') as f:
    json.dump(_ApplyHacks(histograms.AsDicts()), f, indent=4)


def UploadToDashboardImpl(options):
  histograms = _LoadHistogramSetFromProto(options)
  _AddBuildInfo(histograms, options)

  if options.output_json_file:
    _DumpOutput(histograms, options.output_json_file)

  response, content = _SendHistogramSet(options.dashboard_url, histograms)

  if response.status != 200:
    print(('Upload failed with %d: %s\n\n%s' %
           (response.status, response.reason, content)))
    return 1

  upload_token = json.loads(content).get('token')
  if not upload_token:
    print(('Received 200 from dashboard. ',
           'Not waiting for the upload status confirmation.'))
    return 0

  response, resp_json = _WaitForUploadConfirmation(
      options.dashboard_url, upload_token, options.wait_timeout_sec,
      options.wait_polling_period_sec)

  if ((resp_json and resp_json['state'] == 'COMPLETED')
      or _CheckFullUploadInfo(options.dashboard_url, upload_token)):
    print('Upload completed.')
    return 0

  if response.status != 200:
    print(('Upload status poll failed with %d: %s' %
           (response.status, response.reason)))
    return 1

  if resp_json['state'] == 'FAILED':
    print('Upload failed.')
    return 1

  print(('Upload wasn\'t completed in a given time: %s seconds.' %
         options.wait_timeout_sec))
  return 1


def UploadToDashboard(options):
  try:
    exit_code = UploadToDashboardImpl(options)
  except RuntimeError as e:
    print(e)
    return 1
  return exit_code
