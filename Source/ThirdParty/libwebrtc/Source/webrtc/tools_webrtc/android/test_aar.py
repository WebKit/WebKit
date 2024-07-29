#!/usr/bin/env vpython3

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Script for building and testing WebRTC AAR."""

import argparse
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.realpath(sys.argv[0]))
CHECKOUT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))

sys.path.append(os.path.join(CHECKOUT_ROOT, 'tools_webrtc'))
from android.build_aar import BuildAar

ARCHS = ['armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64']
ARTIFACT_ID = 'google-webrtc'
COMMIT_POSITION_REGEX = r'^Cr-Commit-Position: refs/heads/master@{#(\d+)}$'
GRADLEW_BIN = os.path.join(CHECKOUT_ROOT,
                           'examples/androidtests/third_party/gradle/gradlew')
ADB_BIN = os.path.join(CHECKOUT_ROOT,
                       'third_party/android_sdk/public/platform-tools/adb')
AAR_PROJECT_DIR = os.path.join(CHECKOUT_ROOT, 'examples/aarproject')


def _parse_args():
    parser = argparse.ArgumentParser(description='Releases WebRTC on Bintray.')
    parser.add_argument('--use-remoteexec',
                        action='store_true',
                        default=False,
                        help='Use RBE.')
    parser.add_argument('--skip-tests',
                        action='store_true',
                        default=False,
                        help='Skips running the tests.')
    parser.add_argument(
        '--build-dir',
        default=None,
        help='Temporary directory to store the build files. If not specified, '
        'a new directory will be created.')
    parser.add_argument('--verbose',
                        action='store_true',
                        default=False,
                        help='Debug logging.')
    return parser.parse_args()


def _get_commit_hash():
    commit_hash = subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'],
        cwd=CHECKOUT_ROOT).decode('UTF-8').strip()
    return commit_hash


def _get_commit_pos():
    commit_message = subprocess.check_output(
        ['git', 'rev-list', '--format=%B', '--max-count=1', 'HEAD'],
        cwd=CHECKOUT_ROOT).decode('UTF-8')
    commit_pos_match = re.search(COMMIT_POSITION_REGEX, commit_message,
                                 re.MULTILINE)
    if not commit_pos_match:
        raise Exception('Commit position not found in the commit message: %s' %
                        commit_message)
    return commit_pos_match.group(1)


def _test_aar(build_dir):
    """Runs AppRTCMobile tests using AAR. Returns true if the tests pass."""
    logging.info('Testing library.')

    # Uninstall any existing version of AppRTCMobile.
    logging.info('Uninstalling previous AppRTCMobile versions. It is okay for '
                 'these commands to fail if AppRTCMobile is not installed.')
    subprocess.call([ADB_BIN, 'uninstall', 'org.appspot.apprtc'])
    subprocess.call([ADB_BIN, 'uninstall', 'org.appspot.apprtc.test'])

    # Run tests.
    try:
        # First clean the project.
        subprocess.check_call([GRADLEW_BIN, 'clean'], cwd=AAR_PROJECT_DIR)
        # Then run the tests.
        subprocess.check_call([
            GRADLEW_BIN, 'connectedDebugAndroidTest',
            '-PaarDir=' + os.path.abspath(build_dir)
        ],
                              cwd=AAR_PROJECT_DIR)
    except subprocess.CalledProcessError:
        logging.exception('Test failure.')
        return False  # Clean or tests failed

    return True  # Tests pass


def build_and_test_aar(use_remoteexec, skip_tests, build_dir):
    version = '1.0.' + _get_commit_pos()
    commit = _get_commit_hash()
    logging.info('Building and Testing AAR version %s with hash %s', version,
                 commit)

    # If build directory is not specified, create a temporary directory.
    use_tmp_dir = not build_dir
    if use_tmp_dir:
        build_dir = tempfile.mkdtemp()

    try:
        base_name = ARTIFACT_ID + '-' + version
        aar_file = os.path.join(build_dir, base_name + '.aar')

        logging.info('Building at %s', build_dir)
        BuildAar(ARCHS,
                 aar_file,
                 use_remoteexec=use_remoteexec,
                 ext_build_dir=os.path.join(build_dir, 'aar-build'))

        tests_pass = skip_tests or _test_aar(build_dir)
        if not tests_pass:
            raise Exception('Test failure.')

        logging.info('Test success.')

    finally:
        if use_tmp_dir:
            shutil.rmtree(build_dir, True)


def main():
    args = _parse_args()
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)
    build_and_test_aar(args.use_remoteexec, args.skip_tests, args.build_dir)


if __name__ == '__main__':
    sys.exit(main())
