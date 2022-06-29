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
import tarfile
import tempfile
import threading
import time

import angle_path_util


def _ApkPath(suite_name):
    return os.path.join('%s_apk' % suite_name, '%s-debug.apk' % suite_name)


def ApkFileExists(suite_name):
    return os.path.exists(_ApkPath(suite_name))


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

    if platform_tools.exists():
        adb = str(platform_tools / 'adb')
    else:
        adb = 'adb'

    adb_info = subprocess.check_output([adb, '--version']).decode()
    logging.info('adb --version: %s', adb_info)

    return adb


def _AdbRun(args):
    return _Run([_FindAdb()] + args)


def _AdbShell(cmd):
    return _Run([_FindAdb(), 'shell', cmd])


def _GetAdbRoot():
    _AdbRun(['root'])

    for _ in range(20):
        time.sleep(0.5)
        try:
            id_out = _AdbShell('id').decode('ascii')
            if 'uid=0(root)' in id_out:
                return
        except Exception:
            continue
    raise Exception("adb root failed")


def _ReadDeviceFile(device_path):
    with _TempLocalFile() as tempfile_path:
        _AdbRun(['pull', device_path, tempfile_path])
        with open(tempfile_path, 'rb') as f:
            return f.read()


def _RemoveDeviceFile(device_path):
    _AdbShell('rm -f ' + device_path + ' || true')  # ignore errors


def _AddRestrictedTracesJson():
    def add(tar, fn):
        assert (fn.startswith('../../'))
        tar.add(fn, arcname=fn.replace('../../', ''))

    with _TempLocalFile() as tempfile_path:
        with tarfile.open(tempfile_path, 'w', format=tarfile.GNU_FORMAT) as tar:
            for f in glob.glob('../../src/tests/restricted_traces/*/*.json', recursive=True):
                add(tar, f)
            add(tar, '../../src/tests/restricted_traces/restricted_traces.json')
        _AdbRun(['push', tempfile_path, '/sdcard/chromium_tests_root/t.tar'])

    _AdbShell('r=/sdcard/chromium_tests_root; tar -xf $r/t.tar -C $r/ && rm $r/t.tar')


def PrepareTestSuite(suite_name):
    _GetAdbRoot()

    apk_path = _ApkPath(suite_name)
    logging.info('Installing apk path=%s size=%s' % (apk_path, os.path.getsize(apk_path)))

    _AdbRun(['install', '-r', '-d', apk_path])

    permissions = [
        'android.permission.CAMERA', 'android.permission.CHANGE_CONFIGURATION',
        'android.permission.READ_EXTERNAL_STORAGE', 'android.permission.RECORD_AUDIO',
        'android.permission.WRITE_EXTERNAL_STORAGE'
    ]
    _AdbShell('p=com.android.angle.test;'
              'for q in %s;do pm grant "$p" "$q";done;' % ' '.join(permissions))

    _AdbShell('appops set com.android.angle.test MANAGE_EXTERNAL_STORAGE allow || true')

    _AdbShell('mkdir -p /sdcard/chromium_tests_root/')

    if suite_name == 'angle_perftests':
        _AddRestrictedTracesJson()

    if suite_name == 'angle_end2end_tests':
        _AdbRun([
            'push', '../../src/tests/angle_end2end_tests_expectations.txt',
            '/sdcard/chromium_tests_root/src/tests/angle_end2end_tests_expectations.txt'
        ])


def _CompareHashes(local_path, device_path):
    device_hash = _AdbShell('sha256sum -b ' + device_path +
                            ' 2> /dev/null || true').decode().strip()
    if not device_hash:
        return False  # file not on device

    h = hashlib.sha256()
    with open(local_path, 'rb') as f:
        for data in iter(lambda: f.read(65536), b''):
            h.update(data)
    return h.hexdigest() == device_hash


def PrepareRestrictedTraces(traces, check_hash=False):
    start = time.time()
    total_size = 0
    skipped = 0
    for trace in traces:
        path_from_root = 'src/tests/restricted_traces/' + trace + '/' + trace + '.angledata.gz'
        local_path = '../../' + path_from_root
        device_path = '/sdcard/chromium_tests_root/' + path_from_root
        if check_hash and _CompareHashes(local_path, device_path):
            skipped += 1
        else:
            total_size += os.path.getsize(local_path)
            _AdbRun(['push', local_path, device_path])

    logging.info('Synced %d trace files (%.1fMB, %d files already ok) in %.1fs', len(traces),
                 total_size / 1e6, skipped,
                 time.time() - start)


def _RandomHex():
    return hex(random.randint(0, 2**64))[2:]


@contextlib.contextmanager
def _TempDeviceDir():
    path = '/sdcard/Download/temp_dir-%s' % _RandomHex()
    _AdbShell('mkdir -p ' + path)
    try:
        yield path
    finally:
        _AdbShell('rm -rf ' + path)


@contextlib.contextmanager
def _TempDeviceFile():
    path = '/sdcard/Download/temp_file-%s' % _RandomHex()
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


def _RunInstrumentation(flags):
    with _TempDeviceFile() as temp_device_file:
        cmd = ' '.join([
            'p=com.android.angle.test;',
            'ntr=org.chromium.native_test.NativeTestInstrumentationTestRunner;',
            'am instrument -w',
            '-e $ntr.NativeTestActivity "$p".AngleUnitTestActivity',
            '-e $ntr.ShardNanoTimeout 2400000000000',
            '-e org.chromium.native_test.NativeTest.CommandLineFlags "%s"' % ' '.join(flags),
            '-e $ntr.StdoutFile ' + temp_device_file,
            '"$p"/org.chromium.build.gtest_apk.NativeTestInstrumentationTestRunner',
        ])

        _AdbShell(cmd)
        return _ReadDeviceFile(temp_device_file)


def _DumpDebugInfo(since_time):
    logcat_output = _AdbRun(['logcat', '-t', since_time]).decode()
    logging.info('logcat:\n%s', logcat_output)

    pid_lines = [
        ln for ln in logcat_output.split('\n')
        if 'org.chromium.native_test.NativeTest.StdoutFile' in ln
    ]
    if pid_lines:
        debuggerd_output = _AdbShell('debuggerd %s' % pid_lines[-1].split(' ')[2]).decode()
        logging.warning('debuggerd output:\n%s', debuggerd_output)


def _RunInstrumentationWithTimeout(flags, timeout):
    initial_time = _AdbShell('date +"%F %T.%3N"').decode().strip()

    results = []

    def run():
        results.append(_RunInstrumentation(flags))

    t = threading.Thread(target=run)
    t.daemon = True
    t.start()
    t.join(timeout=timeout)

    if t.is_alive():  # join timed out
        logging.warning('Timed out, dumping debug info')
        _DumpDebugInfo(since_time=initial_time)
        raise TimeoutError('Test run did not finish in %s seconds' % timeout)

    return results[0]


def AngleSystemInfo(args):
    PrepareTestSuite('angle_system_info_test')

    with _TempDeviceDir() as temp_dir:
        _RunInstrumentation(args + ['--render-test-output-dir=' + temp_dir])
        output_file = posixpath.join(temp_dir, 'angle_system_info.json')
        return json.loads(_ReadDeviceFile(output_file))


def ListTests():
    out_lines = _RunInstrumentation(["--list-tests"]).decode('ascii').split('\n')

    start = out_lines.index('Tests list:')
    end = out_lines.index('End tests list.')
    return out_lines[start + 1:end]


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


def RunSmokeTest():
    test_name = 'TracePerfTest.Run/vulkan_words_with_friends_2'
    run_instrumentation_timeout = 60

    logging.info('Running smoke test (%s)', test_name)

    PrepareRestrictedTraces([GetTraceFromTestName(test_name)])

    with _TempDeviceFile() as device_test_output_path:
        flags = [
            '--gtest_filter=' + test_name, '--no-warmup', '--steps-per-trial', '1', '--trials',
            '1', '--isolated-script-test-output=' + device_test_output_path
        ]
        try:
            _RunInstrumentationWithTimeout(flags, run_instrumentation_timeout)
        except TimeoutError:
            raise Exception('Smoke test did not finish in %s seconds' %
                            run_instrumentation_timeout)

        test_output = _ReadDeviceFile(device_test_output_path)

    output_json = json.loads(test_output)
    if output_json['tests'][test_name]['actual'] != 'PASS':
        raise Exception('Smoke test (%s) failed' % test_name)

    logging.info('Smoke test passed')


def RunTests(args, stdoutfile=None, output_dir=None, log_output=True):
    args = args[:]
    test_output_path = _RemoveFlag(args, '--isolated-script-test-output')
    perf_output_path = _RemoveFlag(args, '--isolated-script-test-perf-output')
    test_output_dir = _RemoveFlag(args, '--render-test-output-dir')

    result = 0
    output = b''
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

            output = _RunInstrumentationWithTimeout(args, timeout=10 * 60)

            test_output = _ReadDeviceFile(device_test_output_path)
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

    return result, output


def GetTraceFromTestName(test_name):
    m = re.search(r'TracePerfTest.Run/(native|vulkan)_(.*)', test_name)
    if m:
        return m.group(2)

    if test_name.startswith('TracePerfTest.Run/'):
        raise Exception('Unexpected test: %s' % test_name)

    return None
