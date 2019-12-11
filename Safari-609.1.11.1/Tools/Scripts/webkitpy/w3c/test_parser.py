#!/usr/bin/env python

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

from webkitpy.common.host import Host
from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup as Parser


_log = logging.getLogger(__name__)


class TestParser(object):

    def __init__(self, options, filename, host=Host(), source_root_directory=None):
        self.options = options
        self.filename = filename
        self.host = host
        self.filesystem = self.host.filesystem
        self.source_root_directory = source_root_directory

        self.test_doc = None
        self.ref_doc = None
        self.load_file(filename)

    def load_file(self, filename, is_ref=False):
        if self.filesystem.isfile(filename):
            try:
                doc = Parser(self.filesystem.read_binary_file(filename))
            except:
                # FIXME: Figure out what to do if we can't parse the file.
                _log.error("Failed to parse %s", filename)
                doc = None
        else:
            if self.filesystem.isdir(filename):
                # FIXME: Figure out what is triggering this and what to do about it.
                _log.error("Trying to load %s, which is a directory", filename)
            doc = None

        if is_ref:
            self.ref_doc = doc
        else:
            self.test_doc = doc

    def analyze_test(self, test_contents=None, ref_contents=None):
        """ Analyzes a file to determine if it's a test, what type of test, and what reference or support files it requires. Returns all of the test info """

        test_info = None

        if test_contents is None and self.test_doc is None:
            return test_info
        if test_contents is not None:
            self.test_doc = Parser(test_contents)
        if ref_contents is not None:
            self.ref_doc = Parser(ref_contents)

        # First check if it's a reftest
        matches = self.reference_links_of_type('match') + self.reference_links_of_type('mismatch')
        if matches:
            if len(matches) > 1:
                # FIXME: Is this actually true? We should fix this.
                _log.warning('Multiple references are not supported. Importing the first ref defined in %s',
                             self.filesystem.basename(self.filename))

            try:
                href_match_file = matches[0]['href'].strip()
                if href_match_file.startswith('/'):
                    ref_file = self.filesystem.join(self.source_root_directory, href_match_file.lstrip('/'))
                else:
                    ref_file = self.filesystem.join(self.filesystem.dirname(self.filename), href_match_file)
            except KeyError as e:
                # FIXME: Figure out what to do w/ invalid test files.
                _log.error('%s has a reference link but is missing the "href"', self.filesystem)
                return None

            if (ref_file == self.filename):
                return {'referencefile': self.filename}

            if self.ref_doc is None:
                self.load_file(ref_file, True)

            test_info = {'test': self.filename, 'reference': ref_file}

            # If the ref file does not live in the same directory as the test file, check it for support files
            test_info['reference_support_info'] = {}
            if self.filesystem.dirname(ref_file) != self.filesystem.dirname(self.filename):
                reference_support_files = self.support_files(self.ref_doc)
                if len(reference_support_files) > 0:
                    reference_relpath = self.filesystem.relpath(self.filesystem.dirname(self.filename), self.filesystem.dirname(ref_file)) + self.filesystem.sep
                    test_info['reference_support_info'] = {'reference_relpath': reference_relpath, 'files': reference_support_files}

        # not all reference tests have a <link rel='match'> element in WPT repo
        elif self.is_wpt_reftest():
            test_info = {'test': self.filename, 'reference': self.potential_ref_filename()}
            test_info['reference_support_info'] = {}
        # we check for wpt manual test before checking for jstest, as some WPT manual tests can be classified as CSS JS tests
        elif self.is_wpt_manualtest():
            test_info = {'test': self.filename, 'manualtest': True}
        elif self.is_jstest():
            test_info = {'test': self.filename, 'jstest': True}
        elif '-ref' in self.filename or 'reference' in self.filename:
            test_info = {'referencefile': self.filename}
        elif self.options['all'] is True:
            test_info = {'test': self.filename}

        if test_info and self.is_slow_test():
            test_info['slow'] = True

        return test_info

    def reference_links_of_type(self, reftest_type):
        return self.test_doc.findAll(rel=reftest_type)

    def is_jstest(self):
        """Returns whether the file appears to be a jstest, by searching for usage of W3C-style testharness paths."""
        return bool(self.test_doc.find(src=re.compile('[\'\"/]?/resources/testharness')))

    def is_wpt_manualtest(self):
        """Returns whether the test is a manual test according WPT rules."""
        # General rule for manual test i.e. file ends with -manual.htm path
        # See https://web-platform-tests.org/writing-tests/manual.html#requirements-for-a-manual-test
        if self.filename.endswith('-manual.htm') or self.filename.endswith('-manual.html'):
            return True

        # Rule specific to CSS WG manual tests i.e. rely on <meta name="flags">
        # See https://web-platform-tests.org/writing-tests/css-metadata.html#requirement-flags
        # For further details and discussions, see the following links:
        # https://github.com/web-platform-tests/wpt/issues/5381
        # https://github.com/web-platform-tests/wpt/issues/5293
        for match in self.test_doc.findAll(name='meta', attrs={'name': 'flags', 'content': True}):
            css_flags = set(match['content'].split())
            if bool(css_flags & {"animated", "font", "history", "interact", "paged", "speech", "userstyle"}):
                return True

        return False

    def is_slow_test(self):
        return any([match.name == 'meta' and match['name'] == 'timeout' for match in self.test_doc.findAll(content='long')])

    def potential_ref_filename(self):
        parts = self.filesystem.splitext(self.filename)
        return parts[0] + '-ref' + parts[1]

    def is_wpt_reftest(self):
        """Returns whether the test is a ref test according WPT rules (i.e. file has a -ref.html counterpart)."""
        parts = self.filesystem.splitext(self.filename)
        return  self.filesystem.isfile(self.potential_ref_filename())

    def support_files(self, doc):
        """ Searches the file for all paths specified in url()'s, href or src attributes."""
        support_files = []

        if doc is None:
            return support_files

        elements_with_src_attributes = doc.findAll(src=re.compile('.*'))
        elements_with_href_attributes = doc.findAll(href=re.compile('.*'))

        url_pattern = re.compile('url\(.*\)')
        urls = []
        for url in doc.findAll(text=url_pattern):
            url = re.search(url_pattern, url)
            url = re.sub('url\([\'\"]?', '', url.group(0))
            url = re.sub('[\'\"]?\)', '', url)
            urls.append(url)

        src_paths = [src_tag['src'] for src_tag in elements_with_src_attributes]
        href_paths = [href_tag['href'] for href_tag in elements_with_href_attributes]

        paths = src_paths + href_paths + urls
        for path in paths:
            uri_scheme_pattern = re.compile(r"[A-Za-z][A-Za-z+.-]*:")
            if not uri_scheme_pattern.match(path):
                support_files.append(path)

        return support_files
