#!/usr/bin/python

# Copyright (C) 2012 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms(with or without
# modification(are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice(this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice(this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES(INCLUDING(BUT NOT LIMITED TO(THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT(INDIRECT(INCIDENTAL(SPECIAL,
# EXEMPLARY(OR CONSEQUENTIAL DAMAGES (INCLUDING(BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE(DATA(OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY(WHETHER IN CONTRACT(STRICT LIABILITY(OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE(EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import sys
import getopt
import re

import opcode_parser
import opcode_definition_generator


def printUsage():
    print("Usage: %s -m [definition] [-i input] [-o output]" % (sys.argv[0]))


def main(argv):
    opts, args = getopt.getopt(argv, "ho:i:m:")
    if len(args) != 0:
        print(args)
        printUsage()
        sys.exit(1)
    infilename = None
    outfilename = None
    mode = None
    for opt, value in opts:
        if opt == "-o":
            if outfilename != None:
                print("Multiple output files specified")
                printUsage()
                sys.exit(1)
            outfilename = value
        if opt == "-i":
            if infilename != None:
                print("Multiple output files specified")
                printUsage()
                sys.exit(1)
            infilename = value
        if opt == "-m":
            if mode != None:
                print("Multiple output modes specified")
                printUsage()
                sys.exit(1)
            mode = value
        if opt == "-h":
            printUsage()
            sys.exit()
    if mode == None:
        print("No output mode specified")
        printUsage()
        sys.exit(1)

    infile = sys.stdin
    outfile = sys.stdout
    if infilename != None:
        infile = file(infilename, "r", True)
    if outfilename != None:
        outfile = file(outfilename, "w", True)
    input = [l for l in infile]
    infile.close()
    opcodes = opcode_parser.parseOpcodes(input)
    if mode == "definition":
        opcode_definition_generator.generate(outfile, opcodes)
    else:
        print("Invalid mode specified")
        printUsage()
        sys.exit(1)
    outfile.close()


if __name__ == "__main__":
    main(sys.argv[1:])
