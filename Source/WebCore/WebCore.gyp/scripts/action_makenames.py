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

# action_makenames.py is a harness script to connect actions sections of
# gyp-based builds to make_names.pl.
#
# usage: action_makenames.py OUTPUTS -- INPUTS [-- OPTIONS]
#
# Multiple OUTPUTS, INPUTS, and OPTIONS may be listed. The sections are
# separated by -- arguments.
#
# The directory name of the first output is chosen as the directory in which
# make_names will run. If the directory name for any subsequent output is
# different, those files will be moved to the desired directory.
#
# Multiple INPUTS may be listed. An input with a basename matching
# "make_names.pl" is taken as the path to that script. Inputs with names
# ending in TagNames.in or tags.in are taken as tag inputs. Inputs with names
# ending in AttributeNames.in or attrs.in are taken as attribute inputs. There
# may be at most one tag input and one attribute input. A make_names.pl input
# is required and at least one tag or attribute input must be present.
#
# OPTIONS is a list of additional options to pass to make_names.pl. This
# section need not be present.


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
    sections = SplitArgsIntoSections(args[1:])
    assert len(sections) == 2 or len(sections) == 3
    (outputs, inputs) = sections[:2]
    if len(sections) == 3:
        options = sections[2]
    else:
        options = []

    # Make all output pathnames absolute so that they can be accessed after
    # changing directory.
    for index in xrange(0, len(outputs)):
        outputs[index] = os.path.abspath(outputs[index])

    outputDir = os.path.dirname(outputs[0])

    # Look at the inputs and figure out which ones are make_names.pl, tags, and
    # attributes. There can be at most one of each, and those are the only
    # input types supported. make_names.pl is required and at least one of tags
    # and attributes is required.
    makeNamesInput = None
    tagInput = None
    attrInput = None
    eventsInput = None
    for input in inputs:
        # Make input pathnames absolute so they can be accessed after changing
        # directory. On Windows, convert \ to / for inputs to the perl script to
        # work around the intermix of activepython + cygwin perl.
        inputAbs = os.path.abspath(input)
        inputAbsPosix = inputAbs.replace(os.path.sep, posixpath.sep)
        inputBasename = os.path.basename(input)
        if inputBasename == 'make_names.pl' \
            or inputBasename == 'make_event_factory.pl' \
            or inputBasename == 'make_dom_exceptions.pl':
            assert makeNamesInput == None
            makeNamesInput = inputAbs
        elif inputBasename.endswith('TagNames.in') \
             or inputBasename.endswith('tags.in'):
            assert tagInput == None
            tagInput = inputAbsPosix
        elif inputBasename.endswith('AttributeNames.in') \
             or inputBasename.endswith('attrs.in'):
            assert attrInput == None
            attrInput = inputAbsPosix
        elif inputBasename.endswith('EventFactory.in') \
            or inputBasename.endswith('EventTargetFactory.in') \
            or inputBasename.endswith('DOMExceptions.in'):
            eventsInput = inputAbsPosix
        elif inputBasename.endswith('Names.in'):
            options.append(inputAbsPosix)
        else:
            assert False

    assert makeNamesInput != None
    assert tagInput != None or attrInput != None or eventsInput != None or ('--fonts' in options)

    # scriptsPath is a Perl include directory, located relative to
    # makeNamesInput.
    scriptsPath = os.path.normpath(
        os.path.join(os.path.dirname(makeNamesInput), os.pardir, 'bindings', 'scripts'))

    # Change to the output directory because make_names.pl puts output in its
    # working directory.
    os.chdir(outputDir)

    # Build up the command.
    command = ['perl', '-I', scriptsPath, makeNamesInput]
    if tagInput != None:
        command.extend(['--tags', tagInput])
    if attrInput != None:
        command.extend(['--attrs', attrInput])
    if eventsInput != None:
        command.extend(['--input', eventsInput])
    command.extend(options)

    # Do it. check_call is new in 2.5, so simulate its behavior with call and
    # assert.
    returnCode = subprocess.call(command)
    assert returnCode == 0

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
