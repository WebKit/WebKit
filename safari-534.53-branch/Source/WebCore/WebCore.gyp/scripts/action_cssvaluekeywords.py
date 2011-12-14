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

# action_cssvaluekeywords.py is a harness script to connect actions sections of
# gyp-based builds to makevalues.pl.
#
# usage: action_cssvaluekeywords.py OUTPUTS -- INPUTS
#
# Exactly two outputs must be specified: a path to each of CSSValueKeywords.c
# and CSSValueKeywords.h.
#
# Multiple inputs may be specified. One input must have a basename of
# makevalues.pl; this is taken as the path to makevalues.pl. All other inputs
# are paths to .in files that are used as input to makevalues.pl; at least
# one, CSSValueKeywords.in, is required.


import os
import posixpath
import shutil
import subprocess
import sys


def SplitArgsIntoSections(args):
    sections = []
    while len(args) > 0:
        if not '--' in args:
            # If there is no '--' left, everything remaining is an entire section.
            dashes = len(args)
        else:
            dashes = args.index('--')

        sections.append(args[:dashes])

        # Next time through the loop, look at everything after this '--'.
        if dashes + 1 == len(args):
            # If the '--' is at the end of the list, we won't come back through the
            # loop again. Add an empty section now corresponding to the nothingness
            # following the final '--'.
            args = []
            sections.append(args)
        else:
            args = args[dashes + 1:]

    return sections


def main(args):
    (outputs, inputs) = SplitArgsIntoSections(args[1:])

    # Make all output pathnames absolute so that they can be accessed after
    # changing directory.
    for index in xrange(0, len(outputs)):
        outputs[index] = os.path.abspath(outputs[index])

    outputDir = os.path.dirname(outputs[0])

    # Look at the inputs and figure out which one is makevalues.pl and which are
    # inputs to that script.
    makevaluesInput = None
    inFiles = []
    for input in inputs:
        # Make input pathnames absolute so they can be accessed after changing
        # directory. On Windows, convert \ to / for inputs to the perl script to
        # work around the intermix of activepython + cygwin perl.
        inputAbs = os.path.abspath(input)
        inputAbsPosix = inputAbs.replace(os.path.sep, posixpath.sep)
        inputBasename = os.path.basename(input)
        if inputBasename == 'makevalues.pl':
            assert makevaluesInput == None
            makevaluesInput = inputAbs
        elif inputBasename.endswith('.in'):
            inFiles.append(inputAbsPosix)
        else:
            assert False

    assert makevaluesInput != None
    assert len(inFiles) >= 1

    # Change to the output directory because makevalues.pl puts output in its
    # working directory.
    os.chdir(outputDir)

    # Merge all inFiles into a single file whose name will be the same as the
    # first listed inFile, but in the output directory.
    mergedPath = os.path.basename(inFiles[0])
    merged = open(mergedPath, 'wb')    # 'wb' to get \n only on windows

    # Make sure there aren't any duplicate lines in the in files. Lowercase
    # everything because CSS values are case-insensitive.
    lineDict = {}
    for inFilePath in inFiles:
        inFile = open(inFilePath)
        for line in inFile:
            line = line.rstrip()
            if line.startswith('#'):
                line = ''
            if line == '':
                continue
            line = line.lower()
            if line in lineDict:
                raise KeyError, 'Duplicate value %s' % line
            lineDict[line] = True
            print >>merged, line
        inFile.close()

    merged.close()

    # Build up the command.
    command = ['perl', makevaluesInput]

    # Do it. checkCall is new in 2.5, so simulate its behavior with call and
    # assert.
    returnCode = subprocess.call(command)
    assert returnCode == 0

    # Don't leave behind the merged file or the .gperf file created by
    # makevalues.
    (root, ext) = os.path.splitext(mergedPath)
    gperfPath = root + '.gperf'
    os.unlink(gperfPath)
    os.unlink(mergedPath)

    # Go through the outputs. Any output that belongs in a different directory
    # is moved. Do a copy and delete instead of rename for maximum portability.
    # Note that all paths used in this section are still absolute.
    for output in outputs:
        thisOutputDir = os.path.dirname(output)
        if thisOutputDir != outputDir:
            outputBasename = os.path.basename(output)
            src = os.path.join(outputDir, outputBasename)
            dst = os.path.join(thisOutputDir, outputBasename)
            shutil.copyfile(src, dst)
            os.unlink(src)

    return returnCode


if __name__ == '__main__':
    sys.exit(main(sys.argv))
