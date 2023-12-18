# Copyright 2022 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import functools
import glob
import hashlib
import json
import logging
import os
import pathlib
import posixpath
import random
import re
import subprocess
import sys
import tarfile
import tempfile
import threading
import time

import angle_path_util

# Currently we only support a single test package name.
TEST_PACKAGE_NAME = 'com.android.angle.test'

ANGLE_TRACE_TEST_SUITE = 'angle_trace_tests'


class _Global(object):
    initialized = False
    is_android = False
    current_suite = None
    lib_extension = None
    traces_outside_of_apk = False
    temp_dir = None


def _ApkPath(suite_name):
    return os.path.join('%s_apk' % suite_name, '%s-debug.apk' % suite_name)


@functools.lru_cache()
def _FindAapt():
    build_tools = (
        pathlib.Path(angle_path_util.ANGLE_ROOT_DIR) / 'third_party' / 'android_sdk' / 'public' /
        'build-tools')
    latest_build_tools = sorted(build_tools.iterdir())[-1]
    aapt = str(latest_build_tools / 'aapt')
    aapt_info = subprocess.check_output([aapt, 'version']).decode()
    logging.info('aapt version: %s', aapt_info.strip())
    return aapt


def _RemovePrefix(str, prefix):
    assert str.startswith(prefix), 'Expected prefix %s, got: %s' % (prefix, str)
    return str[len(prefix):]


def _FindPackageName(apk_path):
    aapt = _FindAapt()
    badging = subprocess.check_output([aapt, 'dump', 'badging', apk_path]).decode()
    package_name = next(
        _RemovePrefix(item, 'name=').strip('\'')
        for item in badging.split()
        if item.startswith('name='))
    logging.debug('Package name: %s' % package_name)
    return package_name


def _InitializeAndroid(apk_path):
    if _GetAdbRoot():
        # /data/local/tmp/ is not writable by apps.. So use the app path
        _Global.temp_dir = '/data/data/' + TEST_PACKAGE_NAME + '/tmp/'
    else:
        # /sdcard/ is slow (see https://crrev.com/c/3615081 for details)
        # logging will be fully-buffered, can be truncated on crashes
        _Global.temp_dir = '/sdcard/Download/'

    assert _FindPackageName(apk_path) == TEST_PACKAGE_NAME

    apk_files = subprocess.check_output([_FindAapt(), 'list', apk_path]).decode().split()
    apk_so_libs = [posixpath.basename(f) for f in apk_files if f.endswith('.so')]
    if 'libangle_util.cr.so' in apk_so_libs:
        _Global.lib_extension = '.cr.so'
    else:
        assert 'libangle_util.so' in apk_so_libs
        _Global.lib_extension = '.so'
    # When traces are outside of the apk this lib is also outside
    interpreter_so_lib = 'libangle_trace_interpreter' + _Global.lib_extension
    _Global.traces_outside_of_apk = interpreter_so_lib not in apk_so_libs

    if logging.getLogger().isEnabledFor(logging.DEBUG):
        logging.debug(_AdbShell('dumpsys nfc | grep mScreenState || true').decode())
        logging.debug(_AdbShell('df -h').decode())


def Initialize(suite_name):
    if _Global.initialized:
        return

    apk_path = _ApkPath(suite_name)
    if os.path.exists(apk_path):
        _Global.is_android = True
        _InitializeAndroid(apk_path)

    _Global.initialized = True


def IsAndroid():
    assert _Global.initialized, 'Initialize not called'
    return _Global.is_android


def _EnsureTestSuite(suite_name):
    assert IsAndroid()

    if _Global.current_suite != suite_name:
        _PrepareTestSuite(suite_name)
        _Global.current_suite = suite_name


def _Run(cmd):
    logging.debug('Executing command: %s', cmd)
    startupinfo = None
    if hasattr(subprocess, 'STARTUPINFO'):
        # Prevent console window popping up on Windows
        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startupinfo.wShowWindow = subprocess.SW_HIDE
    output = subprocess.check_output(cmd, startupinfo=startupinfo)
    return output


@functools.lru_cache()
def _FindAdb():
    platform_tools = (
        pathlib.Path(angle_path_util.ANGLE_ROOT_DIR) / 'third_party' / 'android_sdk' / 'public' /
        'platform-tools')
    adb = str(platform_tools / 'adb') if platform_tools.exists() else 'adb'
    adb_info = ', '.join(subprocess.check_output([adb, '--version']).decode().strip().split('\n'))
    logging.info('adb --version: %s', adb_info)
    return adb


def _AdbRun(args):
    return _Run([_FindAdb()] + args)


def _AdbShell(cmd):
    return _Run([_FindAdb(), 'shell', cmd])


def _GetAdbRoot():
    shell_id, su_path = _AdbShell('id -u; which su || echo noroot').decode().strip().split('\n')
    if int(shell_id) == 0:
        logging.info('adb already got root')
        return True

    if su_path == 'noroot':
        logging.warning('adb root not available on this device')
        return False

    logging.info('Getting adb root (may take a few seconds)')
    _AdbRun(['root'])
    for _ in range(20):  # `adb root` restarts adbd which can take quite a few seconds
        time.sleep(0.5)
        id_out = _AdbShell('id -u').decode('ascii').strip()
        if id_out == '0':
            logging.info('adb root succeeded')
            return True

    # Device has "su" but we couldn't get adb root. Something is wrong.
    raise Exception('Failed to get adb root')


def _ReadDeviceFile(device_path):
    with _TempLocalFile() as tempfile_path:
        _AdbRun(['pull', device_path, tempfile_path])
        with open(tempfile_path, 'rb') as f:
            return f.read()


def _RemoveDeviceFile(device_path):
    _AdbShell('rm -f ' + device_path + ' || true')  # ignore errors


def _MakeTar(path, patterns):
    with _TempLocalFile() as tempfile_path:
        with tarfile.open(tempfile_path, 'w', format=tarfile.GNU_FORMAT) as tar:
            for p in patterns:
                for f in glob.glob(p, recursive=True):
                    tar.add(f, arcname=f.replace('../../', ''))
        _AdbRun(['push', tempfile_path, path])


def _AddRestrictedTracesJson():
    _MakeTar('/sdcard/chromium_tests_root/t.tar', [
        '../../src/tests/restricted_traces/*/*.json',
        '../../src/tests/restricted_traces/restricted_traces.json'
    ])
    _AdbShell('r=/sdcard/chromium_tests_root; tar -xf $r/t.tar -C $r/ && rm $r/t.tar')


def _AddDeqpFiles(suite_name):
    patterns = [
        '../../third_party/VK-GL-CTS/src/external/openglcts/data/mustpass/*/*/main/*.txt',
        '../../src/tests/deqp_support/*.txt'
    ]
    if '_gles2_' in suite_name:
        patterns.append('gen/vk_gl_cts_data/data/gles2/**')
    if '_gles3_' in suite_name:
        patterns.append('gen/vk_gl_cts_data/data/gles3/**')
        patterns.append('gen/vk_gl_cts_data/data/gl_cts/data/gles3/**')
    if '_gles31_' in suite_name:
        patterns.append('gen/vk_gl_cts_data/data/gles31/**')
        patterns.append('gen/vk_gl_cts_data/data/gl_cts/data/gles31/**')
    if '_gles32_' in suite_name:
        patterns.append('gen/vk_gl_cts_data/data/gl_cts/data/gles32/**')

    _MakeTar('/sdcard/chromium_tests_root/deqp.tar', patterns)
    _AdbShell('r=/sdcard/chromium_tests_root; tar -xf $r/deqp.tar -C $r/ && rm $r/deqp.tar')


def _GetDeviceApkPath():
    pm_path = _AdbShell('pm path %s || true' % TEST_PACKAGE_NAME).decode().strip()
    if not pm_path:
        logging.debug('No installed path found for %s' % TEST_PACKAGE_NAME)
        return None
    device_apk_path = _RemovePrefix(pm_path, 'package:')
    logging.debug('Device APK path is %s' % device_apk_path)
    return device_apk_path


def _LocalFileHash(local_path, gz_tail_size):
    h = hashlib.sha256()
    with open(local_path, 'rb') as f:
        if local_path.endswith('.gz'):
            # equivalent of tail -c {gz_tail_size}
            offset = os.path.getsize(local_path) - gz_tail_size
            if offset > 0:
                f.seek(offset)
        for data in iter(lambda: f.read(65536), b''):
            h.update(data)
    return h.hexdigest()


def _CompareHashes(local_path, device_path):
    # The last 8 bytes of gzip contain CRC-32 and the initial file size and the preceding
    # bytes should be affected by changes in the middle if we happen to run into a collision
    gz_tail_size = 4096

    if local_path.endswith('.gz'):
        cmd = 'test -f {path} && tail -c {gz_tail_size} {path} | sha256sum -b || true'.format(
            path=device_path, gz_tail_size=gz_tail_size)
    else:
        cmd = 'test -f {path} && sha256sum -b {path} || true'.format(path=device_path)

    if device_path.startswith('/data'):
        # Use run-as for files that reside on /data, which aren't accessible without root
        cmd = "run-as {TEST_PACKAGE_NAME} sh -c '{cmd}'".format(
            TEST_PACKAGE_NAME=TEST_PACKAGE_NAME, cmd=cmd)

    device_hash = _AdbShell(cmd).decode().strip()
    if not device_hash:
        logging.debug('_CompareHashes: File not found on device')
        return False  # file not on device

    return _LocalFileHash(local_path, gz_tail_size) == device_hash


def _CheckSameApkInstalled(apk_path):
    device_apk_path = _GetDeviceApkPath()

    try:
        if device_apk_path and _CompareHashes(apk_path, device_apk_path):
            return True
    except subprocess.CalledProcessError as e:
        # non-debuggable test apk installed on device breaks run-as
        logging.warning('_CompareHashes of apk failed: %s' % e)

    return False


def _PrepareTestSuite(suite_name):
    apk_path = _ApkPath(suite_name)

    if _CheckSameApkInstalled(apk_path):
        logging.info('Skipping APK install because host and device hashes match')
    else:
        logging.info('Installing apk path=%s size=%s' % (apk_path, os.path.getsize(apk_path)))
        _AdbRun(['install', '-r', '-d', apk_path])

    permissions = [
        'android.permission.CAMERA', 'android.permission.CHANGE_CONFIGURATION',
        'android.permission.READ_EXTERNAL_STORAGE', 'android.permission.RECORD_AUDIO',
        'android.permission.WRITE_EXTERNAL_STORAGE'
    ]
    _AdbShell('p=%s;'
              'for q in %s;do pm grant "$p" "$q";done;' %
              (TEST_PACKAGE_NAME, ' '.join(permissions)))

    _AdbShell('appops set %s MANAGE_EXTERNAL_STORAGE allow || true' % TEST_PACKAGE_NAME)

    _AdbShell('mkdir -p /sdcard/chromium_tests_root/')
    _AdbShell('mkdir -p %s' % _Global.temp_dir)

    if suite_name == ANGLE_TRACE_TEST_SUITE:
        _AddRestrictedTracesJson()

    if '_deqp_' in suite_name:
        _AddDeqpFiles(suite_name)

    if suite_name == 'angle_end2end_tests':
        _AdbRun([
            'push', '../../src/tests/angle_end2end_tests_expectations.txt',
            '/sdcard/chromium_tests_root/src/tests/angle_end2end_tests_expectations.txt'
        ])


def PrepareRestrictedTraces(traces):
    start = time.time()
    total_size = 0
    skipped = 0

    # In order to get files to the app's home directory and loadable as libraries, we must first
    # push them to tmp on the device.  We then use `run-as` which allows copying files from tmp.
    # Note that `mv` is not allowed with `run-as`.  This means there will briefly be two copies
    # of the trace on the device, so keep that in mind as space becomes a problem in the future.
    app_tmp_path = '/data/local/tmp/angle_traces/'

    def _HashesMatch(local_path, device_path):
        nonlocal total_size, skipped
        if _CompareHashes(local_path, device_path):
            skipped += 1
            return True
        else:
            total_size += os.path.getsize(local_path)
            return False

    def _Push(local_path, path_from_root):
        device_path = '/sdcard/chromium_tests_root/' + path_from_root
        if not _HashesMatch(local_path, device_path):
            _AdbRun(['push', local_path, device_path])

    def _PushLibToAppDir(lib_name):
        local_path = lib_name
        if not os.path.exists(local_path):
            print('Error: missing library: ' + local_path)
            print('Is angle_restricted_traces set in gn args?')  # b/294861737
            sys.exit(1)

        device_path = '/data/user/0/com.android.angle.test/angle_traces/' + lib_name
        if _HashesMatch(local_path, device_path):
            return

        tmp_path = posixpath.join(app_tmp_path, lib_name)
        logging.debug('_PushToAppDir: Pushing %s to %s' % (local_path, tmp_path))
        try:
            _AdbRun(['push', local_path, tmp_path])
            _AdbShell('run-as ' + TEST_PACKAGE_NAME + ' cp ' + tmp_path + ' ./angle_traces/')
            _AdbShell('rm ' + tmp_path)
        finally:
            _RemoveDeviceFile(tmp_path)

    # Create the directories we need
    _AdbShell('mkdir -p ' + app_tmp_path)
    _AdbShell('run-as ' + TEST_PACKAGE_NAME + ' mkdir -p angle_traces')

    # Set up each trace
    for idx, trace in enumerate(sorted(traces)):
        logging.info('Syncing %s trace (%d/%d)', trace, idx + 1, len(traces))

        path_from_root = 'src/tests/restricted_traces/' + trace + '/' + trace + '.angledata.gz'
        _Push('../../' + path_from_root, path_from_root)

        if _Global.traces_outside_of_apk:
            lib_name = 'libangle_restricted_traces_' + trace + _Global.lib_extension
            _PushLibToAppDir(lib_name)

        tracegz = 'gen/tracegz_' + trace + '.gz'
        _Push(tracegz, tracegz)

    # Push one additional file when running outside the APK
    if _Global.traces_outside_of_apk:
        _PushLibToAppDir('libangle_trace_interpreter' + _Global.lib_extension)

    logging.info('Synced files for %d traces (%.1fMB, %d files already ok) in %.1fs', len(traces),
                 total_size / 1e6, skipped,
                 time.time() - start)


def _RandomHex():
    return hex(random.randint(0, 2**64))[2:]


@contextlib.contextmanager
def _TempDeviceDir():
    path = posixpath.join(_Global.temp_dir, 'temp_dir-%s' % _RandomHex())
    _AdbShell('mkdir -p ' + path)
    try:
        yield path
    finally:
        _AdbShell('rm -rf ' + path)


@contextlib.contextmanager
def _TempDeviceFile():
    path = posixpath.join(_Global.temp_dir, 'temp_file-%s' % _RandomHex())
    try:
        yield path
    finally:
        _AdbShell('rm -f ' + path)


@contextlib.contextmanager
def _TempLocalFile():
    fd, path = tempfile.mkstemp()
    os.close(fd)
    try:
        yield path
    finally:
        os.remove(path)


def _SetCaptureProps(env, device_out_dir):
    capture_var_map = {  # src/libANGLE/capture/FrameCapture.cpp
        'ANGLE_CAPTURE_ENABLED': 'debug.angle.capture.enabled',
        'ANGLE_CAPTURE_FRAME_START': 'debug.angle.capture.frame_start',
        'ANGLE_CAPTURE_FRAME_END': 'debug.angle.capture.frame_end',
        'ANGLE_CAPTURE_TRIGGER': 'debug.angle.capture.trigger',
        'ANGLE_CAPTURE_LABEL': 'debug.angle.capture.label',
        'ANGLE_CAPTURE_COMPRESSION': 'debug.angle.capture.compression',
        'ANGLE_CAPTURE_VALIDATION': 'debug.angle.capture.validation',
        'ANGLE_CAPTURE_VALIDATION_EXPR': 'debug.angle.capture.validation_expr',
        'ANGLE_CAPTURE_SOURCE_EXT': 'debug.angle.capture.source_ext',
        'ANGLE_CAPTURE_SOURCE_SIZE': 'debug.angle.capture.source_size',
        'ANGLE_CAPTURE_FORCE_SHADOW': 'debug.angle.capture.force_shadow',
    }
    empty_value = '""'
    shell_cmds = [
        # out_dir is special because the corresponding env var is a host path not a device path
        'setprop debug.angle.capture.out_dir ' + (device_out_dir or empty_value),
    ] + [
        'setprop %s %s' % (v, env.get(k, empty_value)) for k, v in sorted(capture_var_map.items())
    ]

    _AdbShell('\n'.join(shell_cmds))


def _RunInstrumentation(flags):
    assert TEST_PACKAGE_NAME == 'com.android.angle.test'  # inlined below for readability

    with _TempDeviceFile() as temp_device_file:
        cmd = r'''
am instrument -w \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.StdoutFile {out} \
    -e org.chromium.native_test.NativeTest.CommandLineFlags "{flags}" \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.ShardNanoTimeout "1000000000000000000" \
    -e org.chromium.native_test.NativeTestInstrumentationTestRunner.NativeTestActivity \
    com.android.angle.test.AngleUnitTestActivity \
    com.android.angle.test/org.chromium.build.gtest_apk.NativeTestInstrumentationTestRunner
        '''.format(
            out=temp_device_file, flags=r' '.join(flags)).strip()

        capture_out_dir = os.environ.get('ANGLE_CAPTURE_OUT_DIR')
        if capture_out_dir:
            assert os.path.isdir(capture_out_dir)
            with _TempDeviceDir() as device_out_dir:
                _SetCaptureProps(os.environ, device_out_dir)
                try:
                    _AdbShell(cmd)
                finally:
                    _SetCaptureProps({}, None)  # reset
                _PullDir(device_out_dir, capture_out_dir)
        else:
            _AdbShell(cmd)
        return _ReadDeviceFile(temp_device_file)


def AngleSystemInfo(args):
    _EnsureTestSuite('angle_system_info_test')

    with _TempDeviceDir() as temp_dir:
        _RunInstrumentation(args + ['--render-test-output-dir=' + temp_dir])
        output_file = posixpath.join(temp_dir, 'angle_system_info.json')
        return json.loads(_ReadDeviceFile(output_file))


def GetBuildFingerprint():
    return _AdbShell('getprop ro.build.fingerprint').decode('ascii').strip()


def _PullDir(device_dir, local_dir):
    files = _AdbShell('ls -1 %s' % device_dir).decode('ascii').split('\n')
    for f in files:
        f = f.strip()
        if f:
            _AdbRun(['pull', posixpath.join(device_dir, f), posixpath.join(local_dir, f)])


def _RemoveFlag(args, f):
    matches = [a for a in args if a.startswith(f + '=')]
    assert len(matches) <= 1
    if matches:
        original_value = matches[0].split('=')[1]
        args.remove(matches[0])
    else:
        original_value = None

    return original_value


def RunTests(test_suite, args, stdoutfile=None, log_output=True):
    _EnsureTestSuite(test_suite)

    args = args[:]
    test_output_path = _RemoveFlag(args, '--isolated-script-test-output')
    perf_output_path = _RemoveFlag(args, '--isolated-script-test-perf-output')
    test_output_dir = _RemoveFlag(args, '--render-test-output-dir')

    result = 0
    output = b''
    output_json = {}
    try:
        with contextlib.ExitStack() as stack:
            device_test_output_path = stack.enter_context(_TempDeviceFile())
            args.append('--isolated-script-test-output=' + device_test_output_path)

            if perf_output_path:
                device_perf_path = stack.enter_context(_TempDeviceFile())
                args.append('--isolated-script-test-perf-output=%s' % device_perf_path)

            if test_output_dir:
                device_output_dir = stack.enter_context(_TempDeviceDir())
                args.append('--render-test-output-dir=' + device_output_dir)

            output = _RunInstrumentation(args)

            if '--list-tests' in args:
                # When listing tests, there may be no output file. We parse stdout anyways.
                test_output = '{"interrupted": false}'
            else:
                try:
                    test_output = _ReadDeviceFile(device_test_output_path)
                except subprocess.CalledProcessError:
                    logging.error('Unable to read test json output. Stdout:\n%s', output.decode())
                    result = 1
                    return result, output.decode(), None

            if test_output_path:
                with open(test_output_path, 'wb') as f:
                    f.write(test_output)

            output_json = json.loads(test_output)

            num_failures = output_json.get('num_failures_by_type', {}).get('FAIL', 0)
            interrupted = output_json.get('interrupted', True)  # Normally set to False
            if num_failures != 0 or interrupted or output_json.get('is_unexpected', False):
                logging.error('Tests failed: %s', test_output.decode())
                result = 1

            if test_output_dir:
                _PullDir(device_output_dir, test_output_dir)

            if perf_output_path:
                _AdbRun(['pull', device_perf_path, perf_output_path])

        if log_output:
            logging.info(output.decode())

        if stdoutfile:
            with open(stdoutfile, 'wb') as f:
                f.write(output)
    except Exception as e:
        logging.exception(e)
        result = 1

    return result, output.decode(), output_json


def GetTraceFromTestName(test_name):
    if test_name.startswith('TraceTest.'):
        return test_name[len('TraceTest.'):]
    return None
