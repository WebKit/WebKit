#!/usr/bin/python
# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Tests for mb.py."""

import ast
import json
import StringIO
import os
import re
import sys
import tempfile
import unittest

import mb


class FakeMBW(mb.MetaBuildWrapper):
  def __init__(self, win32=False):
    super(FakeMBW, self).__init__()

    # Override vars for test portability.
    if win32:
      self.src_dir = 'c:\\fake_src'
      self.default_config = 'c:\\fake_src\\tools_webrtc\\mb\\mb_config.pyl'
      self.default_isolate_map = ('c:\\fake_src\\testing\\buildbot\\'
                                  'gn_isolate_map.pyl')
      self.platform = 'win32'
      self.executable = 'c:\\python\\python.exe'
      self.sep = '\\'
      self.cwd = 'c:\\fake_src\\out\\Default'
    else:
      self.src_dir = '/fake_src'
      self.default_config = '/fake_src/tools_webrtc/mb/mb_config.pyl'
      self.default_isolate_map = '/fake_src/testing/buildbot/gn_isolate_map.pyl'
      self.executable = '/usr/bin/python'
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
    return '$HOME/%s' % path

  def Exists(self, path):
    abs_path = self._AbsPath(path)
    return (self.files.get(abs_path) is not None or abs_path in self.dirs)

  def MaybeMakeDirectory(self, path):
    abpath = self._AbsPath(path)
    self.dirs.add(abpath)

  def PathJoin(self, *comps):
    return self.sep.join(comps)

  def ReadFile(self, path):
    return self.files[self._AbsPath(path)]

  def WriteFile(self, path, contents, force_verbose=False):
    if self.args.dryrun or self.args.verbose or force_verbose:
      self.Print('\nWriting """\\\n%s""" to %s.\n' % (contents, path))
    abpath = self._AbsPath(path)
    self.files[abpath] = contents

  def Call(self, cmd, env=None, buffer_output=True):
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
    return FakeFile(self.files)

  def RemoveFile(self, path):
    abpath = self._AbsPath(path)
    self.files[abpath] = None

  def RemoveDirectory(self, path):
    abpath = self._AbsPath(path)
    self.rmdirs.append(abpath)
    files_to_delete = [f for f in self.files if f.startswith(abpath)]
    for f in files_to_delete:
      self.files[f] = None

  def _AbsPath(self, path):
    if not ((self.platform == 'win32' and path.startswith('c:')) or
            (self.platform != 'win32' and path.startswith('/'))):
      path = self.PathJoin(self.cwd, path)
    if self.sep == '\\':
      return re.sub(r'\\+', r'\\', path)
    else:
      return re.sub('/+', '/', path)


class FakeFile(object):
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
      'fake_args_bot': '//build/args/bots/fake_group/fake_args_bot.gn',
      'fake_multi_phase': { 'phase_1': 'phase_1', 'phase_2': 'phase_2'},
      'fake_android_bot': 'android_bot',
    },
  },
  'configs': {
    'rel_bot': ['rel', 'goma', 'fake_feature1'],
    'debug_goma': ['debug', 'goma'],
    'phase_1': ['phase_1'],
    'phase_2': ['phase_2'],
    'android_bot': ['android'],
  },
  'mixins': {
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
      'gn_args': 'is_debug=false',
    },
    'debug': {
      'gn_args': 'is_debug=true',
    },
    'android': {
      'gn_args': 'target_os="android"',
    }
  },
}
"""


class UnitTest(unittest.TestCase):
  def fake_mbw(self, files=None, win32=False):
    mbw = FakeMBW(win32=win32)
    mbw.files.setdefault(mbw.default_config, TEST_CONFIG)
    mbw.files.setdefault(
      mbw.ToAbsPath('//testing/buildbot/gn_isolate_map.pyl'),
      '''{
        "foo_unittests": {
          "label": "//foo:foo_unittests",
          "type": "console_test_launcher",
          "args": [],
        },
      }''')
    mbw.files.setdefault(
        mbw.ToAbsPath('//build/args/bots/fake_group/fake_args_bot.gn'),
        'is_debug = false\n')
    if files:
      for path, contents in files.items():
        mbw.files[path] = contents
    return mbw

  def check(self, args, mbw=None, files=None, out=None, err=None, ret=None,
            env=None):
    if not mbw:
      mbw = self.fake_mbw(files)

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

  def test_analyze(self):
    files = {'/tmp/in.json': '''{\
               "files": ["foo/foo_unittest.cc"],
               "test_targets": ["foo_unittests"],
               "additional_compile_targets": ["all"]
             }''',
             '/tmp/out.json.gn': '''{\
               "status": "Found dependency",
               "compile_targets": ["//foo:foo_unittests"],
               "test_targets": ["//foo:foo_unittests"]
             }'''}

    mbw = self.fake_mbw(files)
    mbw.Call = lambda cmd, env=None, buffer_output=True: (0, '', '')

    self.check(['analyze', '-c', 'debug_goma', '//out/Default',
                '/tmp/in.json', '/tmp/out.json'], mbw=mbw, ret=0)
    out = json.loads(mbw.files['/tmp/out.json'])
    self.assertEqual(out, {
      'status': 'Found dependency',
      'compile_targets': ['foo:foo_unittests'],
      'test_targets': ['foo_unittests']
    })

  def test_gen(self):
    mbw = self.fake_mbw()
    self.check(['gen', '-c', 'debug_goma', '//out/Default', '-g', '/goma'],
               mbw=mbw, ret=0)
    self.assertMultiLineEqual(mbw.files['/fake_src/out/Default/args.gn'],
                              ('goma_dir = "/goma"\n'
                               'is_debug = true\n'
                               'use_goma = true\n'))

    # Make sure we log both what is written to args.gn and the command line.
    self.assertIn('Writing """', mbw.out)
    self.assertIn('/fake_src/buildtools/linux64/gn gen //out/Default --check',
                  mbw.out)

    mbw = self.fake_mbw(win32=True)
    self.check(['gen', '-c', 'debug_goma', '-g', 'c:\\goma', '//out/Debug'],
               mbw=mbw, ret=0)
    self.assertMultiLineEqual(mbw.files['c:\\fake_src\\out\\Debug\\args.gn'],
                              ('goma_dir = "c:\\\\goma"\n'
                               'is_debug = true\n'
                               'use_goma = true\n'))
    self.assertIn('c:\\fake_src\\buildtools\\win\\gn.exe gen //out/Debug '
                  '--check\n', mbw.out)

    mbw = self.fake_mbw()
    self.check(['gen', '-m', 'fake_group', '-b', 'fake_args_bot',
                '//out/Debug'],
               mbw=mbw, ret=0)
    self.assertEqual(
        mbw.files['/fake_src/out/Debug/args.gn'],
        'import("//build/args/bots/fake_group/fake_args_bot.gn")\n\n')


  def test_gen_fails(self):
    mbw = self.fake_mbw()
    mbw.Call = lambda cmd, env=None, buffer_output=True: (1, '', '')
    self.check(['gen', '-c', 'debug_goma', '//out/Default'], mbw=mbw, ret=1)

  def test_gen_swarming(self):
    files = {
      '/tmp/swarming_targets': 'base_unittests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    mbw = self.fake_mbw(files)
    self.check(['gen',
                '-c', 'debug_goma',
                '--swarming-targets-file', '/tmp/swarming_targets',
                '//out/Default'], mbw=mbw, ret=0)
    self.assertIn('/fake_src/out/Default/base_unittests.isolate',
                  mbw.files)
    self.assertIn('/fake_src/out/Default/base_unittests.isolated.gen.json',
                  mbw.files)

  def test_gen_swarming_android(self):
    test_files = {
      '/tmp/swarming_targets': 'base_unittests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'additional_compile_target',"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    mbw = self.check(['gen', '-c', 'android_bot', '//out/Default',
                      '--swarming-targets-file', '/tmp/swarming_targets',
                      '--isolate-map-file',
                      '/fake_src/testing/buildbot/gn_isolate_map.pyl'],
                     files=test_files, ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/base_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, ['../../.vpython', '../../testing/test_env.py',
                             'base_unittests'])
    self.assertEqual(command, [
        '../../build/android/test_wrapper/logdog_wrapper.py',
        '--target', 'base_unittests',
        '--logdog-bin-cmd', '../../bin/logdog_butler',
        '--logcat-output-file', '${ISOLATED_OUTDIR}/logcats',
        '--store-tombstones',
    ])

  def test_gen_swarming_android_junit_test(self):
    test_files = {
      '/tmp/swarming_targets': 'base_unittests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'junit_test',"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    mbw = self.check(['gen', '-c', 'android_bot', '//out/Default',
                      '--swarming-targets-file', '/tmp/swarming_targets',
                      '--isolate-map-file',
                      '/fake_src/testing/buildbot/gn_isolate_map.pyl'],
                     files=test_files, ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/base_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, ['../../.vpython', '../../testing/test_env.py',
                             'base_unittests'])
    self.assertEqual(command, [
        '../../build/android/test_wrapper/logdog_wrapper.py',
        '--target', 'base_unittests',
        '--logdog-bin-cmd', '../../bin/logdog_butler',
        '--logcat-output-file', '${ISOLATED_OUTDIR}/logcats',
        '--store-tombstones',
    ])

  def test_gen_timeout(self):
    test_files = {
      '/tmp/swarming_targets': 'base_unittests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'non_parallel_console_test_launcher',"
          "  'timeout': 500,"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    mbw = self.check(['gen', '-c', 'debug_goma', '//out/Default',
                      '--swarming-targets-file', '/tmp/swarming_targets',
                      '--isolate-map-file',
                      '/fake_src/testing/buildbot/gn_isolate_map.pyl'],
                     files=test_files, ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/base_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython',
        '../../testing/test_env.py',
        '../../third_party/gtest-parallel/gtest-parallel',
        '../../third_party/gtest-parallel/gtest_parallel.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        'base_unittests',
    ])
    self.assertEqual(command, [
        '../../testing/test_env.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        '--output_dir=${ISOLATED_OUTDIR}/test_logs',
        '--gtest_color=no',
        '--timeout=500',
        '--workers=1',
        '--retry_failed=3',
        './base_unittests',
        '--asan=0',
        '--lsan=0',
        '--msan=0',
        '--tsan=0',
    ])

  def test_gen_script(self):
    test_files = {
      '/tmp/swarming_targets': 'base_unittests_script\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests_script': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'script',"
          "  'script': '//base/base_unittests_script.py',"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
          "base_unittests_script.py\n"
      ),
    }
    mbw = self.check(['gen', '-c', 'debug_goma', '//out/Default',
                      '--swarming-targets-file', '/tmp/swarming_targets',
                      '--isolate-map-file',
                      '/fake_src/testing/buildbot/gn_isolate_map.pyl'],
                     files=test_files, ret=0)

    isolate_file = (
        mbw.files['/fake_src/out/Default/base_unittests_script.isolate'])
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython', '../../testing/test_env.py',
        'base_unittests', 'base_unittests_script.py',
    ])
    self.assertEqual(command, [
        '../../base/base_unittests_script.py',
    ])

  def test_gen_raw(self):
    test_files = {
      '/tmp/swarming_targets': 'base_unittests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'raw',"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    mbw = self.check(['gen', '-c', 'debug_goma', '//out/Default',
                      '--swarming-targets-file', '/tmp/swarming_targets',
                      '--isolate-map-file',
                      '/fake_src/testing/buildbot/gn_isolate_map.pyl'],
                     files=test_files, ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/base_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython',
        '../../testing/test_env.py',
        '../../tools_webrtc/flags_compatibility.py',
        'base_unittests',
    ])
    self.assertEqual(command, [
        '../../tools_webrtc/flags_compatibility.py',
        '../../testing/test_env.py',
        './base_unittests',
        '--asan=0',
        '--lsan=0',
        '--msan=0',
        '--tsan=0',
    ])

  def test_gen_non_parallel_console_test_launcher(self):
    test_files = {
      '/tmp/swarming_targets': 'base_unittests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'non_parallel_console_test_launcher',"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    mbw = self.check(['gen', '-c', 'debug_goma', '//out/Default',
                      '--swarming-targets-file', '/tmp/swarming_targets',
                      '--isolate-map-file',
                      '/fake_src/testing/buildbot/gn_isolate_map.pyl'],
                     files=test_files, ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/base_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython',
        '../../testing/test_env.py',
        '../../third_party/gtest-parallel/gtest-parallel',
        '../../third_party/gtest-parallel/gtest_parallel.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        'base_unittests',
    ])
    self.assertEqual(command, [
        '../../testing/test_env.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        '--output_dir=${ISOLATED_OUTDIR}/test_logs',
        '--gtest_color=no',
        '--timeout=900',
        '--workers=1',
        '--retry_failed=3',
        './base_unittests',
        '--asan=0',
        '--lsan=0',
        '--msan=0',
        '--tsan=0',
    ])

  def test_isolate_windowed_test_launcher_linux(self):
    test_files = {
      '/tmp/swarming_targets': 'base_unittests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'windowed_test_launcher',"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
          "some_resource_file\n"
      ),
    }
    mbw = self.check(['gen', '-c', 'debug_goma', '//out/Default',
                      '--swarming-targets-file', '/tmp/swarming_targets',
                      '--isolate-map-file',
                      '/fake_src/testing/buildbot/gn_isolate_map.pyl'],
                     files=test_files, ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/base_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython',
        '../../testing/test_env.py',
        '../../testing/xvfb.py',
        '../../third_party/gtest-parallel/gtest-parallel',
        '../../third_party/gtest-parallel/gtest_parallel.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        'base_unittests',
        'some_resource_file',
    ])
    self.assertEqual(command, [
        '../../testing/xvfb.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        '--output_dir=${ISOLATED_OUTDIR}/test_logs',
        '--gtest_color=no',
        '--timeout=900',
        '--retry_failed=3',
        './base_unittests',
        '--asan=0',
        '--lsan=0',
        '--msan=0',
        '--tsan=0',
    ])

  def test_gen_windowed_test_launcher_win(self):
    files = {
      'c:\\fake_src\\out\\Default\\tmp\\swarming_targets': 'unittests\n',
      'c:\\fake_src\\testing\\buildbot\\gn_isolate_map.pyl': (
          "{'unittests': {"
          "  'label': '//somewhere:unittests',"
          "  'type': 'windowed_test_launcher',"
          "}}\n"
      ),
      r'c:\fake_src\out\Default\unittests.exe.runtime_deps': (
          "unittests.exe\n"
          "some_dependency\n"
      ),
    }
    mbw = self.fake_mbw(files=files, win32=True)
    self.check(['gen',
                '-c', 'debug_goma',
                '--swarming-targets-file',
                'c:\\fake_src\\out\\Default\\tmp\\swarming_targets',
                '--isolate-map-file',
                'c:\\fake_src\\testing\\buildbot\\gn_isolate_map.pyl',
                '//out/Default'], mbw=mbw, ret=0)

    isolate_file = mbw.files['c:\\fake_src\\out\\Default\\unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython',
        '../../testing/test_env.py',
        '../../third_party/gtest-parallel/gtest-parallel',
        '../../third_party/gtest-parallel/gtest_parallel.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        'some_dependency',
        'unittests.exe',
    ])
    self.assertEqual(command, [
        '../../testing/test_env.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        '--output_dir=${ISOLATED_OUTDIR}\\test_logs',
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
      '/tmp/swarming_targets': 'base_unittests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'console_test_launcher',"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    mbw = self.check(['gen', '-c', 'debug_goma', '//out/Default',
                      '--swarming-targets-file', '/tmp/swarming_targets',
                      '--isolate-map-file',
                      '/fake_src/testing/buildbot/gn_isolate_map.pyl'],
                     files=test_files, ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/base_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython',
        '../../testing/test_env.py',
        '../../third_party/gtest-parallel/gtest-parallel',
        '../../third_party/gtest-parallel/gtest_parallel.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        'base_unittests',
    ])
    self.assertEqual(command, [
        '../../testing/test_env.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        '--output_dir=${ISOLATED_OUTDIR}/test_logs',
        '--gtest_color=no',
        '--timeout=900',
        '--retry_failed=3',
        './base_unittests',
        '--asan=0',
        '--lsan=0',
        '--msan=0',
        '--tsan=0',
    ])

  def test_isolate_test_launcher_with_webcam(self):
    test_files = {
      '/tmp/swarming_targets': 'base_unittests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'console_test_launcher',"
          "  'use_webcam': True,"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
          "some_resource_file\n"
      ),
    }
    mbw = self.check(['gen', '-c', 'debug_goma', '//out/Default',
                      '--swarming-targets-file', '/tmp/swarming_targets',
                      '--isolate-map-file',
                      '/fake_src/testing/buildbot/gn_isolate_map.pyl'],
                     files=test_files, ret=0)

    isolate_file = mbw.files['/fake_src/out/Default/base_unittests.isolate']
    isolate_file_contents = ast.literal_eval(isolate_file)
    files = isolate_file_contents['variables']['files']
    command = isolate_file_contents['variables']['command']

    self.assertEqual(files, [
        '../../.vpython',
        '../../testing/test_env.py',
        '../../third_party/gtest-parallel/gtest-parallel',
        '../../third_party/gtest-parallel/gtest_parallel.py',
        '../../tools_webrtc/ensure_webcam_is_running.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        'base_unittests',
        'some_resource_file',
    ])
    self.assertEqual(command, [
        '../../tools_webrtc/ensure_webcam_is_running.py',
        '../../testing/test_env.py',
        '../../tools_webrtc/gtest-parallel-wrapper.py',
        '--output_dir=${ISOLATED_OUTDIR}/test_logs',
        '--gtest_color=no',
        '--timeout=900',
        '--retry_failed=3',
        './base_unittests',
        '--asan=0',
        '--lsan=0',
        '--msan=0',
        '--tsan=0',
    ])

  def test_isolate(self):
    files = {
      '/fake_src/out/Default/toolchain.ninja': "",
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'non_parallel_console_test_launcher',"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    self.check(['isolate', '-c', 'debug_goma', '//out/Default',
                'base_unittests'], files=files, ret=0)

    # test running isolate on an existing build_dir
    files['/fake_src/out/Default/args.gn'] = 'is_debug = True\n'
    self.check(['isolate', '//out/Default', 'base_unittests'],
               files=files, ret=0)
    files['/fake_src/out/Default/mb_type'] = 'gn\n'
    self.check(['isolate', '//out/Default', 'base_unittests'],
               files=files, ret=0)

  def test_run(self):
    files = {
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'windowed_test_launcher',"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    self.check(['run', '-c', 'debug_goma', '//out/Default',
                'base_unittests'], files=files, ret=0)

  def test_run_swarmed(self):
    files = {
        '/fake_src/testing/buildbot/gn_isolate_map.pyl':
        ("{'base_unittests': {"
         "  'label': '//base:base_unittests',"
         "  'type': 'console_test_launcher',"
         "}}\n"),
        '/fake_src/out/Default/base_unittests.runtime_deps':
        ("base_unittests\n"),
        '/fake_src/out/Default/base_unittests.archive.json':
        ("{\"base_unittests\":\"fake_hash\"}"),
        '/fake_src/third_party/depot_tools/cipd_manifest.txt':
        ("# vpython\n"
         "/some/vpython/pkg  git_revision:deadbeef\n"),
    }
    task_json = json.dumps({'tasks': [{'task_id': '00000'}]})
    collect_json = json.dumps({'00000': {'results': {}}})

    mbw = self.fake_mbw(files=files)
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'task.json')] = task_json
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'collect_output.json')] = collect_json
    original_impl = mbw.ToSrcRelPath

    def to_src_rel_path_stub(path):
      if path.endswith('base_unittests.archive.json'):
        return 'base_unittests.archive.json'
      return original_impl(path)

    mbw.ToSrcRelPath = to_src_rel_path_stub

    self.check(['run', '-s', '-c', 'debug_goma', '//out/Default',
                'base_unittests'], mbw=mbw, ret=0)
    mbw = self.fake_mbw(files=files)
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'task.json')] = task_json
    mbw.files[mbw.PathJoin(mbw.TempDir(), 'collect_output.json')] = collect_json
    mbw.ToSrcRelPath = to_src_rel_path_stub
    self.check(['run', '-s', '-c', 'debug_goma', '-d', 'os', 'Win7',
                '//out/Default', 'base_unittests'], mbw=mbw, ret=0)

  def test_lookup(self):
    self.check(['lookup', '-c', 'debug_goma'], ret=0)

  def test_quiet_lookup(self):
    self.check(['lookup', '-c', 'debug_goma', '--quiet'], ret=0,
               out=('is_debug = true\n'
                    'use_goma = true\n'))

  def test_lookup_goma_dir_expansion(self):
    self.check(['lookup', '-c', 'rel_bot', '-g', '/foo'], ret=0,
               out=('\n'
                    'Writing """\\\n'
                    'enable_doom_melon = true\n'
                    'goma_dir = "/foo"\n'
                    'is_debug = false\n'
                    'use_goma = true\n'
                    '""" to _path_/args.gn.\n\n'
                    '/fake_src/buildtools/linux64/gn gen _path_\n'))

  def test_help(self):
    orig_stdout = sys.stdout
    try:
      sys.stdout = StringIO.StringIO()
      self.assertRaises(SystemExit, self.check, ['-h'])
      self.assertRaises(SystemExit, self.check, ['help'])
      self.assertRaises(SystemExit, self.check, ['help', 'gen'])
    finally:
      sys.stdout = orig_stdout

  def test_multiple_phases(self):
    # Check that not passing a --phase to a multi-phase builder fails.
    mbw = self.check(['lookup', '-m', 'fake_group', '-b', 'fake_multi_phase'],
                     ret=1)
    self.assertIn('Must specify a build --phase', mbw.out)

    # Check that passing a --phase to a single-phase builder fails.
    mbw = self.check(['lookup', '-m', 'fake_group', '-b', 'fake_builder',
                      '--phase', 'phase_1'], ret=1)
    self.assertIn('Must not specify a build --phase', mbw.out)

    # Check that passing a wrong phase key to a multi-phase builder fails.
    mbw = self.check(['lookup', '-m', 'fake_group', '-b', 'fake_multi_phase',
                      '--phase', 'wrong_phase'], ret=1)
    self.assertIn('Phase wrong_phase doesn\'t exist', mbw.out)

    # Check that passing a correct phase key to a multi-phase builder passes.
    mbw = self.check(['lookup', '-m', 'fake_group', '-b', 'fake_multi_phase',
                      '--phase', 'phase_1'], ret=0)
    self.assertIn('phase = 1', mbw.out)

    mbw = self.check(['lookup', '-m', 'fake_group', '-b', 'fake_multi_phase',
                      '--phase', 'phase_2'], ret=0)
    self.assertIn('phase = 2', mbw.out)

  def test_validate(self):
    mbw = self.fake_mbw()
    self.check(['validate'], mbw=mbw, ret=0)


if __name__ == '__main__':
  unittest.main()
