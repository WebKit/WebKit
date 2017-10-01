#!/usr/bin/env python
#
# Copyright (c) 2017 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

import os.path
import sys
from optparse import OptionParser

from GenerateSettings import *


def main():
    parser = OptionParser(usage="usage: %prog --input file")
    parser.add_option('--input', dest='input', help='file to generate settings from', metavar='file')
    parser.add_option('--outputDir', dest='outputDir', help='directory to generate file in', metavar='dir')

    (options, arguments) = parser.parse_args()

    if not options.input:
        print "Error: must provide an input file"
        parser.print_usage()
        exit(-1)

    outputDirectory = options.outputDir if options.outputDir else os.getcwd()

    if not os.path.exists(outputDirectory):
        os.makedirs(outputDirectory)

    settings = parseInput(options.input)

    generateSettingsHeaderFile(outputDirectory, settings)
    generateSettingsImplementationFile(outputDirectory, settings)
    generateInternalSettingsIDLFile(outputDirectory, settings)
    generateInternalSettingsHeaderFile(outputDirectory, settings)
    generateInternalSettingsImplementationFile(outputDirectory, settings)


if __name__ == '__main__':
    main()
