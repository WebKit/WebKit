# Copyright 2025 The BoringSSL Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Presubmit checks for BoringSSL.

Run by the presubmit API in depot_tools, e.g. by running `git cl presubmit`.
"""

PRESUBMIT_VERSION = '2.0.0'
USE_PYTHON3 = True


def CheckPregeneratedFiles(input_api, output_api):
  """Checks that pregenerated files are properly updated."""
  # TODO(chlily): Make this compatible with the util/bot environment for CI/CQ.
  try:
    # Check that `go` is available on the $PATH.
    input_api.subprocess.check_call(['go', 'version'],
                                    stdout=input_api.subprocess.PIPE,
                                    stderr=input_api.subprocess.PIPE)
  except input_api.subprocess.CalledProcessError as e:
    return [
        output_api.PresubmitPromptOrNotify(f'Could not run `go`: {e}')
    ]

  pregenerate_script_path = input_api.os_path.join(
      input_api.change.RepositoryRoot(), 'util', 'pregenerate')
  try:
    out, retcode = input_api.subprocess.communicate(
        ['go', 'run', pregenerate_script_path, '-check'],
        stdout=input_api.subprocess.PIPE,
        stderr=input_api.subprocess.PIPE)
    if retcode:
      bad = out[1].decode("utf-8").splitlines()
      return [
          output_api.PresubmitError(
              ("Found out-of-date generated files. "
               "Run `go run ./util/pregenerate` to update them."), bad)
      ]
  except input_api.subprocess.CalledProcessError as e:
    return [output_api.PresubmitError(f'Could not run go script: {e}')]
  return []  # Check passed.
