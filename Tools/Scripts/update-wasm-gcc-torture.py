#! /usr/bin/env python
#
# Copyright (C) 2017 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import glob
import json
import os.path
import shutil
import subprocess
import sys
import tarfile
import urllib2

LKGR_URL = 'https://storage.googleapis.com/wasm-llvm/builds/%s/lkgr.json'
TORTURE_FILE = 'wasm-torture-emwasm-%s.tbz2'
TORTURE_URL = 'https://storage.googleapis.com/wasm-llvm/builds/%s/%s/%s'
TORTURE_DIR = 'emwasm-torture-out'
WATERFALL_DIR = 'waterfall'
WATERFALL_GIT = 'https://github.com/WebAssembly/waterfall.git'
WATERFALL_KNOWN_FAILURES = 'src/test/run_known_gcc_test_failures.txt'

DESCRIPTION = """GCC torture test packager for jsc testing

Obtain the GCC torture tests as compiled for WebAssembly by the waterfall, using
Emscripten with its embedded libc. Create a yaml file which can be executed with
run-jsc-stress-tests."""


def parse_args():
    parser = argparse.ArgumentParser(
        description=DESCRIPTION,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='See http://wasm-stat.us for current waterfall status')
    parser.add_argument('--lkgr', default=None, help='LKGR value to use')
    parser.add_argument('--platform', default='mac', help='Waterfall platform to sync to')
    parser.add_argument('--nountar', default=False, action='store_true', help='If set, torture tests are assumed to already be in their destination folder')
    parser.add_argument('--waterfall_hash', default=None, help='git hash to use for the waterfall repo')
    parser.add_argument('--yaml', default='gcc-torture.yaml', help='yaml test file to generate')
    return parser.parse_args()


def update_lkgr(args):
    lkgr_url = LKGR_URL % args.platform
    if not args.lkgr:
        print('Downloading: %s' % lkgr_url)
        args.lkgr = json.loads(urllib2.urlopen(lkgr_url).read())['build']
    print('lkgr: %s' % args.lkgr)


def untar_torture(args):
    torture_file = TORTURE_FILE % args.lkgr
    torture_url = TORTURE_URL % (args.platform, args.lkgr, torture_file)
    if not os.path.exists(torture_file):
        print('Downloading: %s' % torture_url)
        torture_download = urllib2.urlopen(torture_url)
        with open(torture_file, 'wb') as f:
            f.write(torture_download.read())
    if not args.nountar:
        if os.path.isdir(TORTURE_DIR):
            shutil.rmtree(TORTURE_DIR)
        with tarfile.open(torture_file, 'r') as torture_tar:
            print('Extracting: %s -> %s' % (torture_file, TORTURE_DIR))
            torture_tar.extractall()
    assert os.path.isdir(TORTURE_DIR)


def list_js_files(args):
    js_files = sorted([os.path.basename(f) for f in glob.glob(os.path.join(TORTURE_DIR, '*.js'))])
    print('Found %s JavaScript tests' % len(js_files))
    assert len(js_files) > 1200
    return js_files


def waterfall_known_failures(args):
    if not os.path.isdir(WATERFALL_DIR):
        subprocess.check_call(['git', 'clone', WATERFALL_GIT, WATERFALL_DIR])
    else:
        subprocess.check_call(['git', 'pull'], cwd=WATERFALL_DIR)
    if args.waterfall_hash:
        subprocess.check_call(['git', 'checkout', args.waterfall_hash], cwd=WATERFALL_DIR)
    else:
        args.waterfall_hash = subprocess.check_output(['git', 'log', '-n1', '--pretty=format:%H'], cwd=WATERFALL_DIR).strip()
    print('Waterfall at: %s' % args.waterfall_hash)
    known_failures = []
    with open(os.path.join(WATERFALL_DIR, WATERFALL_KNOWN_FAILURES)) as failures_file:
        for line in failures_file:
            line = line.strip()
            if '#' in line:
                line = line[:line.index('#')].strip()
            tokens = line.split()
            if not tokens:
                continue
            test_file = tokens[0]
            if len(tokens) > 1:
                attributes = set(tokens[1].split(','))
                if 'emwasm' not in attributes:
                    continue
            if os.path.splitext(test_file)[-1] != '.js':
                continue
            known_failures.append(test_file)
    assert known_failures
    return set(known_failures)


def create_yaml(args, js_files, known_failures):
    with open(args.yaml, 'w+') as yaml:
        yaml.write('# GCC torture tests\n')
        yaml.write('# Auto-generated by: %s\n' % sys.argv[0])
        yaml.write('# lkgr: %s\n' % args.lkgr)
        yaml.write('# Waterfall hash: %s\n' % args.waterfall_hash)
        for js in js_files:
            yaml.write('\n- path: %s/%s\n' % (TORTURE_DIR, js))
            yaml.write('  cmd: runWebAssemblyEmscripten :%s\n' % ('normal' if js not in known_failures else 'skip'))


def main():
    args = parse_args()
    update_lkgr(args)
    untar_torture(args)
    js_files = list_js_files(args)
    known_failures = waterfall_known_failures(args)
    create_yaml(args, js_files, known_failures)


if __name__ == '__main__':
    sys.exit(main())
