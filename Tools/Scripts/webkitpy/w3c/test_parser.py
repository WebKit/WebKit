# Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

import logging
import re

try:
    from pathlib import Path
except ImportError:
    from pathlib2 import Path

from webkitpy.common.host import Host
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup as Parser
from webkitpy.thirdparty.wpt.manifest.sourcefile import SourceFile

_log = logging.getLogger(__name__)


class TestParser(object):

    def __init__(self, options, filename, host=None, source_root_directory=None):
        self.options = options
        self.filename = filename
        self.host = Host() if host is None else host
        self.filesystem = self.host.filesystem
        self.source_root_directory = source_root_directory

    def analyze_test(self, test_contents=None, ref_contents=None):
        """ Analyzes a file to determine if it's a test, what type of test, and what reference or support files it requires. Returns all of the test info """

        test_info = None

        if test_contents is None and isinstance(self.host.filesystem, MockFileSystem):
            test_contents = self.filesystem.read_binary_file(self.filename)

        if self.source_root_directory is None:
            source_root_directory = Path(self.filename).anchor
        else:
            source_root_directory = self.source_root_directory

        wpt_sourcefile = SourceFile(
            source_root_directory,
            self.filesystem.relpath(self.filename, source_root_directory),
            "/",
            contents=test_contents,
        )

        test_type, test_items = wpt_sourcefile.manifest_items()

        if test_type == "manual":
            test_info = {'test': self.filename, 'manualtest': True}

        elif test_type == "reftest":
            assert len(test_items) == 1
            test_item = test_items[0]

            matches = [ref for ref in test_item.references if ref[1] == "=="]
            mismatches = [ref for ref in test_item.references if ref[1] == "!="]
            if len(matches) > 1 or len(mismatches) > 1:
                # FIXME: Is this actually true? We should fix this.
                _log.warning('Multiple references of the same type are not supported. Importing the first ref defined in %s',
                             self.filesystem.basename(self.filename))

            test_info = {'test': self.filename}
            refs_to_import = matches[0:1] + mismatches[0:1]

            for (href_match_file, reference_type) in refs_to_import:
                if href_match_file.startswith('/'):
                    ref_file = self.filesystem.join(source_root_directory, href_match_file.lstrip('/'))
                else:
                    ref_file = self.filesystem.join(self.filesystem.dirname(self.filename), href_match_file)

                assert reference_type in ("==", "!=")
                reference_type = "match" if reference_type == "==" else "mismatch"

                if ref_contents is not None:
                    ref_doc = Parser(ref_contents)
                elif self.filesystem.exists(ref_file):
                    ref_doc = Parser(self.filesystem.read_binary_file(ref_file))
                else:
                    ref_doc = None

                test_info[reference_type + '_reference'] = ref_file
                test_info[reference_type + '_reference_support_info'] = {}

                # If the ref file does not live in the same directory as the test file, check it for support files
                if self.filesystem.dirname(ref_file) != self.filesystem.dirname(self.filename):
                    reference_support_files = self.support_files(ref_doc)
                    if len(reference_support_files) > 0:
                        reference_relpath = self.filesystem.relpath(self.filesystem.dirname(self.filename), self.filesystem.dirname(ref_file)) + self.filesystem.sep
                        test_info[reference_type + '_reference_support_info'] = {'reference_relpath': reference_relpath, 'files': reference_support_files}

        elif test_type == "crashtest":
            test_info = {'test': self.filename, 'crashtest': True}
        elif test_type == "testharness":
            test_info = {'test': self.filename, 'jstest': True}
        elif self.options['all'] is True:
            test_info = {'test': self.filename}

        if test_info and wpt_sourcefile.timeout == "long":
            test_info['slow'] = True

        return test_info

    def support_files(self, doc):
        """ Searches the file for all paths specified in url()'s, href or src attributes."""
        support_files = []

        if doc is None:
            return support_files

        elements_with_src_attributes = doc.findAll(src=re.compile('.*'))
        elements_with_href_attributes = doc.findAll(href=re.compile('.*'))

        url_pattern = re.compile(r'url\(.*\)')
        urls = []
        for url in doc.findAll(text=url_pattern):
            for url in re.findall(url_pattern, url):
                url = re.sub('url\\([\'\"]?', '', url)
                url = re.sub('[\'\"]?\\)', '', url)
                urls.append(url)

        src_paths = [src_tag['src'] for src_tag in elements_with_src_attributes]
        href_paths = [href_tag['href'] for href_tag in elements_with_href_attributes]

        paths = src_paths + href_paths + urls
        for path in paths:
            uri_scheme_pattern = re.compile(r"[A-Za-z][A-Za-z+.-]*:")
            if not uri_scheme_pattern.match(path):
                support_files.append(path)

        return support_files
