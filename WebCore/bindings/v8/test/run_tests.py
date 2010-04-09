#!/usr/bin/python
#
# Copyright (C) 2010 Google Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# This script generates h and cpp file for TestObj.idl using the V8 code
# generator. Please execute the script whenever changes are made to
# CodeGeneratorV8.pm, and submit the changes in V8TestObj.h/cpp in the same
# patch. This makes it easier to track and review changes in generated code.
# To execute, invoke: 'python run_tests.py'

import os
import sys


def test(idlFilePath):
    cmd = ['perl', '-w',
         '-I../../scripts',
         '../../scripts/generate-bindings.pl',
         # idl include directories (path relative to generate-bindings.pl)
         '--include .',
         # place holder for defines (generate-bindings.pl requires it)
         '--defines xxx',
         '--generator V8',
         '--outputDir .',
         idlFilePath]
    os.system(' '.join(cmd))


def main(argv):
    scriptDir = os.path.dirname(__file__)
    os.chdir(scriptDir)
    test('TestObj.idl')


if __name__ == '__main__':
    sys.exit(main(sys.argv))
