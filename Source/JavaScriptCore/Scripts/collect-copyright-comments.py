#!/usr/bin/env python3

# Copyright (C) 2022 Apple Inc.  All rights reserved.
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
# 3.  Neither the name of Apple, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
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

import logging
import optparse
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), 'wkbuiltins'))

logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.ERROR)
log = logging.getLogger('global')

from lazywriter import LazyFileWriter

from wkbuiltins import *

from builtins_templates import BuiltinsGeneratorTemplates as Templates

multilineCommentRegExp = re.compile(r"\/\*.*?\*\/", re.MULTILINE | re.DOTALL)

def parse_copyright_lines(text):
    licenseBlock = multilineCommentRegExp.findall(text)[0]
    licenseBlock = licenseBlock[:licenseBlock.index("Redistribution")]

    copyrightLines = [Templates.DefaultCopyright]
    for line in licenseBlock.split("\n"):
        line = line.replace("/*", "")
        line = line.replace("*/", "")
        line = line.replace("*", "")
        line = line.replace("Copyright", "")
        line = line.replace("copyright", "")
        line = line.replace("(C)", "")
        line = line.replace("(c)", "")
        line = line.strip()

        if len(line) == 0:
            continue

        copyrightLines.append(line)

    return copyrightLines

def copyrights(copyright_lines):
    owner_to_years = dict()
    copyrightYearRegExp = re.compile(r"(\d{4})[, ]{0,2}")
    ownerStartRegExp = re.compile(r"[^\d, ]")

    # Returns deduplicated copyrights keyed on the owner.
    for line in copyright_lines:
        years = set(copyrightYearRegExp.findall(line))
        ownerIndex = ownerStartRegExp.search(line).start()
        owner = line[ownerIndex:]
        log.debug("Found years: %s and owner: %s" % (years, owner))
        if owner not in owner_to_years:
            owner_to_years[owner] = set()

        owner_to_years[owner] = owner_to_years[owner].union(years)

    result = []

    for owner, years in list(owner_to_years.items()):
        sorted_years = list(years)
        sorted_years.sort()
        result.append("%s %s" % (', '.join(sorted_years), owner))

    return result

def generate_license(copyrights):
    raw_license = Template(Templates.LicenseText).substitute(None)
    copyrights.sort()

    license_block = []
    license_block.append("/*")
    for copyright in copyrights:
        license_block.append(" * Copyright (c) %s" % copyright)
    if len(copyrights) > 0:
        license_block.append(" * ")

    for line in raw_license.split('\n'):
        license_block.append(" * " + line)

    license_block.append(" */")

    return '\n'.join(license_block)

cli_parser = optparse.OptionParser(usage="usage: %prog [options] Builtin1.js [, Builtin2.js, ...]")
cli_parser.add_option("-o", "--output", help="Output file")

options, filenames = cli_parser.parse_args()

copyright_lines = set()

for filename in filenames:
    with open(filename, 'r') as file:
        parsed_copyrights = set(parse_copyright_lines(file.read()))
        copyright_lines = copyright_lines.union(parsed_copyrights)

output = generate_license(copyrights(copyright_lines))
if options.output:
    output_file = LazyFileWriter(options.output, False)
    output_file.write(output)
    output_file.close()
else:
    print(output)
