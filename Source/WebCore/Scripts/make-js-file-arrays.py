#!/usr/bin/env python
# Copyright (C) 2014 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import io
import os
from optparse import OptionParser
from StringIO import StringIO
from jsmin import JavascriptMinify


def stringifyCodepoint(code):
    if code < 128:
        return '{0:d}'.format(code)
    else:
        return "'\\x{0:02x}'".format(code)


def chunk(list, chunkSize):
    for i in xrange(0, len(list), chunkSize):
        yield list[i:i + chunkSize]


def main():
    parser = OptionParser(usage="usage: %prog [--no-minify] header source [input [input...]]")
    parser.add_option('--no-minify', action='store_true', help='Do not run the input files through jsmin')
    (options, arguments) = parser.parse_args()
    if len(arguments) < 3:
        print 'Error: must provide at least 3 arguments'
        parser.print_usage()
        exit(-1)

    headerPath = arguments[0]
    sourcePath = arguments[1]
    inputPaths = arguments[2:]

    headerFile = open(headerPath, 'w')
    print >> headerFile, 'namespace WebCore {'

    sourceFile = open(sourcePath, 'w')
    print >> sourceFile, '#include "{0:s}"'.format(os.path.basename(headerPath))
    print >> sourceFile, 'namespace WebCore {'

    jsm = JavascriptMinify()

    for inputFileName in inputPaths:
        inputStream = io.FileIO(inputFileName)
        outputStream = StringIO()

        if not options.no_minify:
            jsm.minify(inputStream, outputStream)
            characters = outputStream.getvalue()
        else:
            characters = inputStream.read()

        size = len(characters)
        variableName = os.path.splitext(os.path.basename(inputFileName))[0]

        print >> headerFile, 'extern const char {0:s}JavaScript[{1:d}];'.format(variableName, size)
        print >> sourceFile, 'const char {0:s}JavaScript[{1:d}] = {{'.format(variableName, size)

        codepoints = map(ord, characters)
        for codepointChunk in chunk(codepoints, 16):
            print >> sourceFile, '    {0:s},'.format(','.join(map(stringifyCodepoint, codepointChunk)))

        print >> sourceFile, '};'

    print >> headerFile, '}'
    print >> sourceFile, '}'

if __name__ == '__main__':
    main()
