#!/usr/bin/python
#
# Copyright (C) 2009 Google Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# usage: rule_gperf.py INPUT_FILE OUTPUT_DIR
# INPUT_FILE is a path to DocTypeStrings.gperf, HTMLEntityNames.gperf, or
# ColorData.gperf.
# OUTPUT_DIR is where the gperf-generated .cpp file should be placed. Because
# some users want a .c file instead of a .cpp file, the .cpp file is copied
# to .c when done.

import posixpath
import shutil
import subprocess
import sys

assert len(sys.argv) == 3

inputFile = sys.argv[1]
outputDir = sys.argv[2]

gperfCommands = {
    'DocTypeStrings.gperf': [
        '-CEot', '-L', 'ANSI-C', '-k*', '-N', 'findDoctypeEntry',
        '-F', ',PubIDInfo::eAlmostStandards,PubIDInfo::eAlmostStandards'
    ],
    'HTMLEntityNames.gperf': [
        '-a', '-L', 'ANSI-C', '-C', '-G', '-c', '-o', '-t', '-k*',
        '-N', 'findEntity', '-D', '-s', '2'
    ],
    'ColorData.gperf': [
        '-CDEot', '-L', 'ANSI-C', '-k*', '-N', 'findColor', '-D', '-s', '2'
    ],
}

inputName = posixpath.basename(inputFile)
assert inputName in gperfCommands

(inputRoot, inputExt) = posixpath.splitext(inputName)
outputCpp = posixpath.join(outputDir, inputRoot + '.cpp')

#command = ['gperf', '--output-file', outputCpp]
command = ['gperf']
command.extend(gperfCommands[inputName])
command.append(inputFile)

ofile = open(outputCpp, 'w')

# Do it. check_call is new in 2.5, so simulate its behavior with call and
# assert.
returnCode = subprocess.call(command, stdout=ofile.fileno())
assert returnCode == 0

outputC = posixpath.join(outputDir, inputRoot + '.c')
shutil.copyfile(outputCpp, outputC)
