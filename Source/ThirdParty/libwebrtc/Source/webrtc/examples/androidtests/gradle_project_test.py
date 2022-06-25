#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""
This scripts tests creating an Android Studio project using the
generate_gradle.py script and making a debug build using it.

It expect to be given the webrtc output build directory as the first argument
all other arguments are optional.
"""

import argparse
import logging
import os
import shutil
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))
GENERATE_GRADLE_SCRIPT = os.path.join(
    SRC_DIR, 'build/android/gradle/generate_gradle.py')
GRADLEW_BIN = os.path.join(SCRIPT_DIR, 'third_party/gradle/gradlew')


def _RunCommand(argv, cwd=SRC_DIR, **kwargs):
    logging.info('Running %r', argv)
    subprocess.check_call(argv, cwd=cwd, **kwargs)


def _ParseArgs():
    parser = argparse.ArgumentParser(
        description='Test generating Android gradle project.')
    parser.add_argument('build_dir_android',
                        help='The path to the build directory for Android.')
    parser.add_argument('--project_dir',
                        help='A temporary directory to put the output.')

    args = parser.parse_args()
    return args


def main():
    logging.basicConfig(level=logging.INFO)
    args = _ParseArgs()

    project_dir = args.project_dir
    if not project_dir:
        project_dir = tempfile.mkdtemp()

    output_dir = os.path.abspath(args.build_dir_android)
    project_dir = os.path.abspath(project_dir)

    try:
        env = os.environ.copy()
        env['PATH'] = os.pathsep.join([
            os.path.join(SRC_DIR, 'third_party', 'depot_tools'),
            env.get('PATH', '')
        ])
        _RunCommand([
            GENERATE_GRADLE_SCRIPT, '--output-directory', output_dir,
            '--target', '//examples:AppRTCMobile', '--project-dir',
            project_dir, '--use-gradle-process-resources', '--split-projects'
        ],
                    env=env)
        _RunCommand([GRADLEW_BIN, 'assembleDebug'], project_dir)
    finally:
        # Do not delete temporary directory if user specified it manually.
        if not args.project_dir:
            shutil.rmtree(project_dir, True)


if __name__ == '__main__':
    sys.exit(main())
