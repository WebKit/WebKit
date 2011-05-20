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

# usage:
# action_useragentstylesheets.py OUTPUTS INPUTS -- MAINSCRIPT MODULES -- OPTIONS
#
# OUTPUTS must contain two items, in order: a path to UserAgentStyleSheets.h
# and a path to UserAgentStyleSheetsData.cpp.
# INPUTS contains one or more CSS files.
#
# MAINSCRIPT is the path to make-css-file-arrays.pl. MODULES may contain
# multiple paths to additional perl modules.
#
# OPTIONS are passed as-is to MAINSCRIPT as additional arguments.


import os
import shlex
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
    sections = SplitArgsIntoSections(args[1:])
    assert len(sections) == 3
    (outputsInputs, scripts, options) = sections

    assert len(outputsInputs) >= 3
    outputH = outputsInputs[0]
    outputCpp = outputsInputs[1]
    styleSheets = outputsInputs[2:]

    assert len(scripts) >= 1
    makeCssFileArrays = scripts[0]
    perlModules = scripts[1:]

    includeDirs = []
    for perlModule in perlModules:
        includeDir = os.path.dirname(perlModule)
        if not includeDir in includeDirs:
            includeDirs.append(includeDir)

    # The defines come in as one flat string. Split it up into distinct arguments.
    if '--defines' in options:
        definesIndex = options.index('--defines')
        if definesIndex + 1 < len(options):
            splitOptions = shlex.split(options[definesIndex + 1])
            if splitOptions:
                options[definesIndex + 1] = ' '.join(splitOptions)

    # Build up the command.
    command = ['perl']
    for includeDir in includeDirs:
        command.extend(['-I', includeDir])
    command.append(makeCssFileArrays)
    command.extend(options)
    command.extend([outputH, outputCpp])
    command.extend(styleSheets)

    # Do it. check_call is new in 2.5, so simulate its behavior with call and
    # assert.
    returnCode = subprocess.call(command)
    assert returnCode == 0

    return returnCode


if __name__ == '__main__':
    sys.exit(main(sys.argv))
