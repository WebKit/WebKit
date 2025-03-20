#!/usr/bin/python3
# Copyright (C) 2013, 2015, 2016 Apple Inc. All rights reserved.
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

import os
import re
import subprocess
import sys
import xml.dom.minidom


def main(argv):
    tools_dir = os.path.dirname(__file__)
    public_v3_dir = os.path.abspath(os.path.join(tools_dir, '..', 'public', 'v3'))

    bundled_script = ''

    index_html = xml.dom.minidom.parse(os.path.join(public_v3_dir, 'index.html'))
    for template in index_html.getElementsByTagName('template'):
        if template.getAttribute('id') != 'unbundled-scripts':
            continue
        unbundled_scripts = template.getElementsByTagName('script')
        for script in unbundled_scripts:
            src = script.getAttribute('src')

            with open(os.path.join(public_v3_dir, src)) as script_file:
                script_content = script_file.read()
                script_content = re.sub(r'([\"\'])use strict\1;', '', script_content)
                script_content = re.sub(r'(?P<prefix>\n\s*static\s+(?P<type>\w+)Template\(\)\s*{[^`]*`)(?P<content>[^`]*)(?P<suffix>`;?\s*})',
                    compress_template, script_content, re.MULTILINE)

                bundled_script += script_content

    jsmin = subprocess.Popen(['python3', os.path.join(tools_dir, 'jsmin.py')], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    minified_script = jsmin.communicate(input=bundled_script.encode())[0]

    new_size = float(len(minified_script))
    old_size = float(len(bundled_script))
    print('%d -> %d (%.1f%%)' % (old_size, new_size, new_size / old_size * 100))

    with open(os.path.join(public_v3_dir, 'bundled-scripts.js'), 'w') as bundled_file:
        bundled_file.write(minified_script.decode())


def compress_template(match):
    prefix = match.group('prefix')
    content = match.group('content')
    suffix = match.group('suffix')

    if match.group('type') == 'css':
        content = cssminify(content)
    else:
        content = re.sub(r'\s+', ' ', content)

    return prefix + content + suffix


def cssminify(css):
    rules = (
        (r"\/\*.*?\*\/", ""),          # delete comments
        (r"\n", ""),                   # delete new lines
        (r"\s+", " "),                 # change multiple spaces to one space
        (r"\s?([;{},~>!])\s?", r"\1"), # delete space where it is not needed
        (r":\s", ":"),                 # delete spaces after colons, but not before. E.g. do not break selectors "a :focus", "b :matches(...)", "c :not(...)" where the leading space is significant
        (r"\s?([-+])(?:\s(?![0-9(])(?!var))", r"\1"), # delete whitespace around + and - when not followed by a number, paren, or var(). E.g. strip for selector "a + b" but not "calc(a + b)" which requires spaces.
        (r";}", "}")                   # change ';}' to '}' because the semicolon is not needed
    )

    css = css.replace("\r\n", "\n")
    for rule in rules:
        css = re.compile(rule[0], re.MULTILINE | re.UNICODE | re.DOTALL).sub(rule[1], css)
    return css


if __name__ == "__main__":
    main(sys.argv)
