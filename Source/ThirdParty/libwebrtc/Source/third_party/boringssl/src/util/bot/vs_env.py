# Copyright 2015 The BoringSSL Authors
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

import json
import os
import os.path
import subprocess
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))
sdk_root = os.path.join(script_dir, 'windows_sdk')

def SetEnvironmentForCPU(cpu):
  """Sets the environment to build with the selected toolchain for |cpu|."""
  assert cpu in ('x86', 'x64', 'arm', 'arm64')
  sdk_dir = os.path.join(sdk_root, 'Windows Kits', '10')
  os.environ['WINDOWSSDKDIR'] = sdk_dir
  # Include the VS runtime in the PATH in case it's not machine-installed.
  runtime_dirs = \
      [os.path.join(sdk_root, d) for d in ['sys64', 'sys32', 'sysarm64']]
  os.environ['PATH'] = \
      os.pathsep.join(runtime_dirs) + os.pathsep + os.environ['PATH']

  # Set up the architecture-specific environment from the SetEnv files. See
  # _LoadToolchainEnv() from setup_toolchain.py in Chromium.
  with open(os.path.join(sdk_dir, 'bin', 'SetEnv.%s.json' % cpu)) as f:
    env = json.load(f)['env']
  if env['VSINSTALLDIR'] == [["..", "..\\"]]:
    # Old-style paths were relative to the win_sdk\bin directory.
    json_relative_dir = os.path.join(sdk_dir, 'bin')
  else:
    # New-style paths are relative to the toolchain directory.
    json_relative_dir = sdk_root
  for k in env:
    entries = [os.path.join(*([json_relative_dir] + e)) for e in env[k]]
    # clang-cl wants INCLUDE to be ;-separated even on non-Windows,
    # lld-link wants LIB to be ;-separated even on non-Windows.  Path gets :.
    sep = os.pathsep if k == 'PATH' else ';'
    env[k] = sep.join(entries)
  # PATH is a bit of a special case, it's in addition to the current PATH.
  env['PATH'] = env['PATH'] + os.pathsep + os.environ['PATH']

  for k, v in env.items():
    os.environ[k] = v

if len(sys.argv) < 2:
  print("Usage: vs_env.py TARGET_ARCH CMD...", file=sys.stderr)
  sys.exit(1)

target_arch = sys.argv[1]
cmd = sys.argv[2:]

SetEnvironmentForCPU(target_arch)
sys.exit(subprocess.call(cmd))
