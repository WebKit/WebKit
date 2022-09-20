#!/usr/bin/env vpython3

# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Tests for mb.py."""

import ast
import os
import re
import sys
import tempfile
import unittest

_SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
_SRC_DIR = os.path.dirname(os.path.dirname(_SCRIPT_DIR))
sys.path.insert(0, _SRC_DIR)

from tools_webrtc.mb import mb


class FakeMBW(mb.WebRTCMetaBuildWrapper):
  def __init__(self, win32=False):
    super().__init__()

    # Override vars for test portability.
    if win32:
      self.chromium_src_dir = 'c:\\fake_src'
      self.default_config = 'c:\\fake_src\\tools_webrtc\\mb\\mb_config.pyl'
      self.default_isolate_map = ('c:\\fake_src\\testing\\buildbot\\'
                                  'gn_isolate_map.pyl')
      self.platform = 'win32'
      self.executable = 'c:\\python\\vpython3.exe'
      self.sep = '\\'
      self.cwd = 'c:\\fake_src\\out\\Default'
    else:
      self.chromium_src_dir = '/fake_src'
      self.default_config = '/fake_src/tools_webrtc/mb/mb_config.pyl'
      self.default_isolate_map = '/fake_src/testing/buildbot/gn_isolate_map.pyl'
      self.executable = '/usr/bin/vpython3'
      self.platform = 'linux2'
      self.sep = '/'
      self.cwd = '/fake_src/out/Default'

    self.files = {}
    self.dirs = set()
    self.calls = []
    self.cmds = []
    self.cross_compile = None
    self.out = ''
    self.err = ''
    self.rmdirs = []

  def ExpandUser(self, path):
    # pylint: disable=no-self-use
    return '$HOME/%s' % path

  def Exists(self, path):
    abs_path = self._AbsPath(path)
    return self.files.get(abs_path) is not None or abs_path in self.dirs

  def ListDir(self, path):
    dir_contents = []
    for f in list(self.files.keys()) + list(self.dirs):
      head, _ = os.path.split(f)
      if head == path:
        dir_contents.append(f)
    return dir_contents

  def MaybeMakeDirectory(self, path):
    abpath = self._AbsPath(path)
    self.dirs.add(abpath)

  def PathJoin(self, *comps):
    return self.sep.join(comps)

  def ReadFile(self, path):
    try:
      return self.files[self._AbsPath(path)]
    except KeyError as e:
      raise IOError('%s not found' % path) from e

  def WriteFile(self, path, contents, force_verbose=False):
    if self.args.dryrun or self.args.verbose or force_verbose:
      self.Print('\nWriting """\\\n%s""" to %s.\n' % (contents, path))
    abpath = self._AbsPath(path)
    self.files[abpath] = contents

  def Call(self, cmd, env=None, capture_output=True, stdin=None):
    del env
    del capture_output
    del stdin
    self.calls.append(cmd)
    if self.cmds:
      return self.cmds.pop(0)
    return 0, '', ''

  def Print(self, *args, **kwargs):
    sep = kwargs.get('sep', ' ')
    end = kwargs.get('end', '\n')
    f = kwargs.get('file', sys.stdout)
    if f == sys.stderr:
      self.err += sep.join(args) + end
    else:
      self.out += sep.join(args) + end

  def TempDir(self):
    tmp_dir = os.path.join(tempfile.gettempdir(), 'mb_test')
    self.dirs.add(tmp_dir)
    return tmp_dir

  def TempFile(self, mode='w'):
    del mode
    return FakeFile(self.files)

  def RemoveFile(self, path):
    abpath = self._AbsPath(path)
    self.files[abpath] = None

  def RemoveDirectory(self, abs_path):
    # Normalize the passed-in path to handle different working directories
    # used during unit testing.
    abs_path = self._AbsPath(abs_path)
    self.rmdirs.append(abs_path)
    files_to_delete = [f for f in self.files if f.startswith(abs_path)]
    for f in files_to_delete:
      self.files[f] = None

  def _AbsPath(self, path):
    if not ((self.platform == 'win32' and path.startswith('c:')) or
            (self.platform != 'win32' and path.startswith('/'))):
      path = self.PathJoin(self.cwd, path)
    if self.sep == '\\':
      return re.sub(r'\\+', r'\\', path)
    return re.sub('/+', '/', path)


class FakeFile:
  # pylint: disable=invalid-name
  def __init__(self, files):
    self.name = '/tmp/file'
    self.buf = ''
    self.files = files

  def write(self, contents):
    self.buf += contents

  def close(self):
    self.files[self.name] = self.buf


TEST_CONFIG = """\
{
  'builder_groups': {
    'chromium': {},
    'fake_group': {
      'fake_builder': 'rel_bot',
      'fake_debug_builder': 'debug_goma',
      'fake_args_bot': 'fake_args_bot',
      'fake_multi_phase': { 'phase_1': 'phase_1', 'phase_2': 'phase_2'},
      'fake_android_bot': 'android_bot',
      'fake_args_file': 'args_file_goma',
      'fake_ios_error': 'ios_error',
    },
  },
  'configs': {
    'args_file_goma': ['fake_args_bot', 'goma'],
    'fake_args_bot': ['fake_args_bot'],
    'rel_bot': ['rel', 'goma', 'fake_feature1'],
    'debug_goma': ['debug', 'goma'],
    'phase_1': ['rel', 'phase_1'],
    'phase_2': ['rel', 'phase_2'],
    'android_bot': ['android'],
    'ios_error': ['error'],
  },
  'mixins': {
    'error': {
      'gn_args': 'error',
    },
    'fake_args_bot': {
      'args_file': '//build/args/bots/fake_group/fake_args_bot.gn',
    },
    'fake_feature1': {
      'gn_args': 'enable_doom_melon=true',
    },
    'goma': {
      'gn_args': 'use_goma=true',
    },
    'phase_1': {
      'gn_args': 'phase=1',
    },
    'phase_2': {
      'gn_args': 'phase=2',
    },
    'rel': {
      'gn_args': 'is_debug=false dcheck_always_on=false',
    },
    'debug': {
      'gn_args': 'is_debug=true',
    },
    'android': {
      'gn_args': 'target_os="android" dcheck_always_on=false',
    }
  },
}
"""


def CreateFakeMBW(files=None, win32=False):
  mbw = FakeMBW(win32=win32)
  mbw.files.setdefault(mbw.default_config, TEST_CONFIG)
  mbw.files.setdefault(
      mbw.ToAbsPath('//testing/buildbot/gn_isolate_map.pyl'), '''{
      "foo_unittests": {
        "label": "//foo:foo_unittests",
        "type": "console_test_launcher",
        "args": [],
      },
    }''')
  mbw.files.setdefault(
      mbw.ToAbsPath('//build/args/bots/fake_group/fake_args_bot.gn'),
      'is_debug = false\ndcheck_always_on=false\n')
  mbw.files.setdefault(mbw.ToAbsPath('//tools/mb/rts_banned_suites.json'), '{}')
  if files:
    for path, contents in list(files.items()):
      mbw.files[path] = contents
      if path.endswith('.runtime_deps'):

        def FakeCall(cmd, env=None, capture_output=True, stdin=None):
          # pylint: disable=cell-var-from-loop
          del cmd
          del env
          del capture_output
          del stdin
          mbw.files[path] = contents
          return 0, '', ''

        # pylint: disable=invalid-name
        mbw.Call = FakeCall
  return mbw


class UnitTest(unittest.TestCase):
  # pylint: disable=invalid-name
  def check(self,
            args,
            mbw=None,
            files=None,
            out=None,
            err=None,
            ret=None,
            env=None):
    if not mbw:
      mbw = CreateFakeMBW(files)

    try:
      prev_env = os.environ.copy()
      os.environ = env if env else prev_env
      actual_ret = mbw.Main(args)
    finally:
      os.environ = prev_env
    self.assertEqual(
        actual_ret, ret,
        "ret: %s, out: %s, err: %s" % (actual_ret, mbw.out, mbw.err))
    if out is not None:
      self.assertEqual(mbw.out, out)
    if err is not None:
      self.assertEqual(mbw.err, err)
    return mbw

  def test_gen_swarming(self):
    files = {
        '/tmp/swarming_targets':
        'foo_unittests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'foo_unittests': {"
         "  'label': '//foo:foo_unittests',"
         "  'type': 'raw',"
         "  'args': [],"
         "}}\n"),
        '/fake_src/out/Default/foo_unittests.runtime_deps': ("foo_unittests\n"),
    }
    mbw = CreateFakeMBW(files)
    self.check([
        'gen', '-c', 'debug_goma', '--swarming-targets-file',
        '/tmp/swarming_targets', '//out/Default'
    ],
               mbw=mbw,
               ret=0)
    self.assertIn('/fake_src/out/Default/foo_unittests.isolate', mbw.files)
    self.assertIn('/fake_src/out/Default/foo_unittests.isolated.gen.json',
                  mbw.files)

  def test_gen_swarming_android(self):
    test_files = {
        '/tmp/swarming_targets':
        'foo_unittests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'foo_unittests': {"
         "  'label': '//foo:foo_unittests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
        '/fake_src/out/Default/foo_unittests.runtime_deps': ("foo_unittests\n"),
    }
    mbw = self.check([
        'gen', '-c', 'android_bot', '//out/Default', '--swarming-targets-file',
        '/tmp/swarming_targets', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl'
    ],
                     files=test_files,
                     ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/foo_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(
        files,
        ['../../.vpython3', '../../testing/test_env.py', 'foo_unittests'])
    self.assertEqual(command, [
        'vpython3',
        '../../build/android/test_wrapper/logdog_wrapper.py',
        '--target',
        'foo_unittests',
        '--logdog-bin-cmd',
        '../../bin/logdog_butler',
        '--logcat-output-file',
        '${ISOLATED_OUTDIR}/logcats',
        '--store-tombstones',
    ])

  def test_gen_swarming_android_junit_test(self):
    test_files = {
        '/tmp/swarming_targets':
        'foo_unittests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'foo_unittests': {"
         "  'label': '//foo:foo_unittests',"
         "  'type': 'junit_test',"
         "}}\n"),
        '/fake_src/out/Default/foo_unittests.runtime_deps': ("foo_unittests\n"),
    }
    mbw = self.check([
        'gen', '-c', 'android_bot', '//out/Default', '--swarming-targets-file',
        '/tmp/swarming_targets', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl'
    ],
                     files=test_files,
                     ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/foo_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(
        files,
        ['../../.vpython3', '../../testing/test_env.py', 'foo_unittests'])
    self.assertEqual(command, [
        'vpython3',
        '../../build/android/test_wrapper/logdog_wrapper.py',
        '--target',
        'foo_unittests',
        '--logdog-bin-cmd',
        '../../bin/logdog_butler',
        '--logcat-output-file',
        '${ISOLATED_OUTDIR}/logcats',
        '--store-tombstones',
    ])

  def test_gen_timeout(self):
    test_files = {
        '/tmp/swarming_targets':
        'foo_unittests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'foo_unittests': {"
         "  'label': '//foo:foo_unittests',"
         "  'type': 'non_parallel_console_test_launcher',"
         "  'timeout': 500,"
         "}}\n"),
        '/fake_src/out/Default/foo_unittests.runtime_deps': ("foo_unittests\n"),
    }
    mbw = self.check([
        'gen', '-c', 'debug_goma', '//out/Default', '--swarming-targets-file',
        '/tmp/swarming_targets', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl'
    ],
                     files=test_files,
                     ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/foo_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython3',
        '../../testing/test_env.py',
        '../../third_party/gtest-parallel/gtest-parallel',
        '../../third_party/gtest-parallel/gtest_parallel.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        'foo_unittests',
    ])
    self.assertEqual(command, [
        'vpython3',
        '../../testing/test_env.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        '--output_dir=${ISOLATED_OUTDIR}/test_logs',
        '--gtest_color=no',
        '--timeout=500',
        '--workers=1',
        '--retry_failed=3',
        './foo_unittests',
        '--asan=0',
        '--lsan=0',
        '--msan=0',
        '--tsan=0',
    ])

  def test_gen_script(self):
    test_files = {
        '/tmp/swarming_targets':
        'foo_unittests_script\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'foo_unittests_script': {"
         "  'label': '//foo:foo_unittests',"
         "  'type': 'script',"
         "  'script': '//foo/foo_unittests_script.py',"
         "}}\n"),
        '/fake_src/out/Default/foo_unittests_script.runtime_deps':
        ("foo_unittests\n"
         "foo_unittests_script.py\n"),
    }
    mbw = self.check([
        'gen', '-c', 'debug_goma', '//out/Default', '--swarming-targets-file',
        '/tmp/swarming_targets', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl'
    ],
                     files=test_files,
                     ret=0)

    isolate_file = (
        mbw.files['/fake_src/out/Default/foo_unittests_script.isolate'])
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython3',
        '../../testing/test_env.py',
        'foo_unittests',
        'foo_unittests_script.py',
    ])
    self.assertEqual(command, [
        'vpython3',
        '../../foo/foo_unittests_script.py',
    ])

  def test_gen_raw(self):
    test_files = {
        '/tmp/swarming_targets':
        'foo_unittests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'foo_unittests': {"
         "  'label': '//foo:foo_unittests',"
         "  'type': 'raw',"
         "}}\n"),
        '/fake_src/out/Default/foo_unittests.runtime_deps': ("foo_unittests\n"),
    }
    mbw = self.check([
        'gen', '-c', 'debug_goma', '//out/Default', '--swarming-targets-file',
        '/tmp/swarming_targets', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl'
    ],
                     files=test_files,
                     ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/foo_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython3',
        '../../testing/test_env.py',
        '../../tools_webrtc/flags_compatibility.py',
        'foo_unittests',
    ])
    self.assertEqual(command, [
        'vpython3',
        '../../tools_webrtc/flags_compatibility.py',
        './foo_unittests',
    ])

  def test_gen_non_parallel_console_test_launcher(self):
    test_files = {
        '/tmp/swarming_targets':
        'foo_unittests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'foo_unittests': {"
         "  'label': '//foo:foo_unittests',"
         "  'type': 'non_parallel_console_test_launcher',"
         "}}\n"),
        '/fake_src/out/Default/foo_unittests.runtime_deps': ("foo_unittests\n"),
    }
    mbw = self.check([
        'gen', '-c', 'debug_goma', '//out/Default', '--swarming-targets-file',
        '/tmp/swarming_targets', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl'
    ],
                     files=test_files,
                     ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/foo_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython3',
        '../../testing/test_env.py',
        '../../third_party/gtest-parallel/gtest-parallel',
        '../../third_party/gtest-parallel/gtest_parallel.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        'foo_unittests',
    ])
    self.assertEqual(command, [
        'vpython3',
        '../../testing/test_env.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        '--output_dir=${ISOLATED_OUTDIR}/test_logs',
        '--gtest_color=no',
        '--timeout=900',
        '--workers=1',
        '--retry_failed=3',
        './foo_unittests',
        '--asan=0',
        '--lsan=0',
        '--msan=0',
        '--tsan=0',
    ])

  def test_isolate_windowed_test_launcher_linux(self):
    test_files = {
        '/tmp/swarming_targets':
        'foo_unittests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'foo_unittests': {"
         "  'label': '//foo:foo_unittests',"
         "  'type': 'windowed_test_launcher',"
         "}}\n"),
        '/fake_src/out/Default/foo_unittests.runtime_deps':
        ("foo_unittests\n"
         "some_resource_file\n"),
    }
    mbw = self.check([
        'gen', '-c', 'debug_goma', '//out/Default', '--swarming-targets-file',
        '/tmp/swarming_targets', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl'
    ],
                     files=test_files,
                     ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/foo_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython3',
        '../../testing/test_env.py',
        '../../testing/xvfb.py',
        '../../third_party/gtest-parallel/gtest-parallel',
        '../../third_party/gtest-parallel/gtest_parallel.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        'foo_unittests',
        'some_resource_file',
    ])
    self.assertEqual(command, [
        'vpython3',
        '../../testing/xvfb.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        '--output_dir=${ISOLATED_OUTDIR}/test_logs',
        '--gtest_color=no',
        '--timeout=900',
        '--retry_failed=3',
        './foo_unittests',
        '--asan=0',
        '--lsan=0',
        '--msan=0',
        '--tsan=0',
    ])

  def test_gen_windowed_test_launcher_win(self):
    files = {
        'c:\\fake_src\\out\\Default\\tmp\\swarming_targets':
        'unittests\n',
        'c:\\fake_src\\testing\\buildbot\\gn_isolate_map.pyl':
        ("{'unittests': {"
         "  'label': '//somewhere:unittests',"
         "  'type': 'windowed_test_launcher',"
         "}}\n"),
        r'c:\fake_src\out\Default\unittests.exe.runtime_deps':
        ("unittests.exe\n"
         "some_dependency\n"),
    }
    mbw = CreateFakeMBW(files=files, win32=True)
    self.check([
        'gen', '-c', 'debug_goma', '--swarming-targets-file',
        'c:\\fake_src\\out\\Default\\tmp\\swarming_targets',
        '--isolate-map-file',
        'c:\\fake_src\\testing\\buildbot\\gn_isolate_map.pyl', '//out/Default'
    ],
               mbw=mbw,
               ret=0)

    isolate_file = mbw.files['c:\\fake_src\\out\\Default\\unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython3',
        '../../testing/test_env.py',
        '../../third_party/gtest-parallel/gtest-parallel',
        '../../third_party/gtest-parallel/gtest_parallel.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        'some_dependency',
        'unittests.exe',
    ])
    self.assertEqual(command, [
        'vpython3',
        '../../testing/test_env.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        '--output_dir=${ISOLATED_OUTDIR}/test_logs',
        '--gtest_color=no',
        '--timeout=900',
        '--retry_failed=3',
        r'.\unittests.exe',
        '--asan=0',
        '--lsan=0',
        '--msan=0',
        '--tsan=0',
    ])

  def test_gen_console_test_launcher(self):
    test_files = {
        '/tmp/swarming_targets':
        'foo_unittests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'foo_unittests': {"
         "  'label': '//foo:foo_unittests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
        '/fake_src/out/Default/foo_unittests.runtime_deps': ("foo_unittests\n"),
    }
    mbw = self.check([
        'gen', '-c', 'debug_goma', '//out/Default', '--swarming-targets-file',
        '/tmp/swarming_targets', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl'
    ],
                     files=test_files,
                     ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/foo_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython3',
        '../../testing/test_env.py',
        '../../third_party/gtest-parallel/gtest-parallel',
        '../../third_party/gtest-parallel/gtest_parallel.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        'foo_unittests',
    ])
    self.assertEqual(command, [
        'vpython3',
        '../../testing/test_env.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        '--output_dir=${ISOLATED_OUTDIR}/test_logs',
        '--gtest_color=no',
        '--timeout=900',
        '--retry_failed=3',
        './foo_unittests',
        '--asan=0',
        '--lsan=0',
        '--msan=0',
        '--tsan=0',
    ])

  def test_isolate_test_launcher_with_webcam(self):
    test_files = {
        '/tmp/swarming_targets':
        'foo_unittests\n',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'foo_unittests': {"
         "  'label': '//foo:foo_unittests',"
         "  'type': 'console_test_launcher',"
         "  'use_webcam': True,"
         "}}\n"),
        '/fake_src/out/Default/foo_unittests.runtime_deps':
        ("foo_unittests\n"
         "some_resource_file\n"),
    }
    mbw = self.check([
        'gen', '-c', 'debug_goma', '//out/Default', '--swarming-targets-file',
        '/tmp/swarming_targets', '--isolate-map-file',
        '/fake_src/testing/buildbot/gn_isolate_map.pyl'
    ],
                     files=test_files,
                     ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/foo_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython3',
        '../../testing/test_env.py',
        '../../third_party/gtest-parallel/gtest-parallel',
        '../../third_party/gtest-parallel/gtest_parallel.py',
        '../../tools_webrtc/ensure_webcam_is_running.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        'foo_unittests',
        'some_resource_file',
    ])
    self.assertEqual(command, [
        'vpython3',
        '../../tools_webrtc/ensure_webcam_is_running.py',
        'vpython3',
        '../../testing/test_env.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        '--output_dir=${ISOLATED_OUTDIR}/test_logs',
        '--gtest_color=no',
        '--timeout=900',
        '--retry_failed=3',
        './foo_unittests',
        '--asan=0',
        '--lsan=0',
        '--msan=0',
        '--tsan=0',
    ])

  def test_isolate(self):
    files = {
        '/fake_src/out/Default/toolchain.ninja':
        "",
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'foo_unittests': {"
         "  'label': '//foo:foo_unittests',"
         "  'type': 'non_parallel_console_test_launcher',"
         "}}\n"),
        '/fake_src/out/Default/foo_unittests.runtime_deps': ("foo_unittests\n"),
    }
    self.check(
        ['isolate', '-c', 'debug_goma', '//out/Default', 'foo_unittests'],
        files=files,
        ret=0)

    # test running isolate on an existing build_dir
    files['/fake_src/out/Default/args.gn'] = 'is_debug = true\n'
    self.check(['isolate', '//out/Default', 'foo_unittests'],
               files=files,
               ret=0)
    files['/fake_src/out/Default/mb_type'] = 'gn\n'
    self.check(['isolate', '//out/Default', 'foo_unittests'],
               files=files,
               ret=0)

if __name__ == '__main__':
  unittest.main()
