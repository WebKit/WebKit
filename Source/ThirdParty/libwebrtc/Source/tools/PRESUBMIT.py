# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

def _LicenseHeader(input_api):
  """Returns the license header regexp."""
  # Accept any year number from 2003 to the current year
  current_year = int(input_api.time.strftime('%Y'))
  allowed_years = (str(s) for s in reversed(xrange(2003, current_year + 1)))
  years_re = '(' + '|'.join(allowed_years) + ')'
  license_header = (
      r'.*? Copyright( \(c\))? %(year)s The WebRTC [Pp]roject [Aa]uthors\. '
        r'All [Rr]ights [Rr]eserved\.\n'
      r'.*?\n'
      r'.*? Use of this source code is governed by a BSD-style license\n'
      r'.*? that can be found in the LICENSE file in the root of the source\n'
      r'.*? tree\. An additional intellectual property rights grant can be '
        r'found\n'
      r'.*? in the file PATENTS\.  All contributing project authors may\n'
      r'.*? be found in the AUTHORS file in the root of the source tree\.\n'
  ) % {
      'year': years_re,
  }
  return license_header

def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(input_api.canned_checks.CheckLicense(
      input_api, output_api, _LicenseHeader(input_api)))
  return results

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results

def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results
