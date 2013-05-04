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
import os
import re

from webkitpy.thirdparty.BeautifulSoup import BeautifulSoup as Parser


class TestParser(object):

    def __init__(self, options, filename):
        self.options = options
        self.filename = filename
        self.test_doc = self.load_file(filename)
        self.ref_doc = None

    def load_file(self, file_to_parse):
        """ Opens and read the |file_to_parse| """
        doc = None

        if os.path.exists(file_to_parse):
            f = open(file_to_parse)
            html = f.read()
            f.close()

            doc = Parser(html)

        return doc

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
        matches = self.get_reftests(True)
        if len(matches) != 0:

            if len(matches) > 1:
                print 'Warning: Webkit does not support multiple references. Importing the first ref defined in ' + os.path.basename(self.filename)

            ref_file = os.path.join(os.path.dirname(self.filename), matches[0]['href'])
            if self.ref_doc is None:
                self.ref_doc = self.load_file(ref_file)

            test_info = {'test': self.filename, 'reference': ref_file}

            # If the ref file path is relative, we need to check it for relative paths
            # because when it lands in Webkit, it will be moved down into the test dir
            # Note: The test files themselves are not checked for support files
            # that live outside their directories as it's the convention in the CSSWG
            # to put all support files in the same dir or subdir as the test. All non-test
            # files in the test's directory tree are automatically copied as part of the import as
            # they are assumed to be required support files There is exactly one case in the entire
            # css2.1 suite where at test depends on a file that lives in a different directory, which
            # depends on another file that lives outside of its directory. This code covers that case :)
            if matches[0]['href'].startswith('..'):
                supportFiles = self.get_support_files(self.ref_doc)
                test_info['refsupport'] = supportFiles

        # Next, if it's a JS test
        elif self.is_jstest():
            test_info = {'test': self.filename, 'jstest': True}

        # Or if we explicitly want everything
        elif self.options['all'] is True and \
             not('-ref' in self.filename) and \
             not('reference' in self.filename):
            test_info = {'test': self.filename}

        # Courtesy check and warning if there are any mismatches
        mismatches = self.get_reftests(False)
        if len(mismatches) != 0:
            print "Warning: Webkit does not support mismatch references. Ignoring mismatch defined in " + os.path.basename(self.filename)

        return test_info

    def get_reftests(self, find_match):
        """ Searches for all reference links in the file. If |find_match| is searches for matches, otherwise for mismatches """

        if find_match:
            return self.test_doc.findAll(rel='match')
        else:
            return self.test_doc.findAll(rel='mismatch')

    def is_jstest(self):
        """ Searches the file for usage of W3C-style testharness paths """

        if self.test_doc.find(src=re.compile('[\'\"/]?/resources/testharness')) is None:
            return False
        else:
            return True

    def get_support_files(self, doc):
        """ Searches the file for all paths specified in url()'s, href or src attributes """

        support_files = []

        if doc is None:
            return support_files

        # Look for tags with src or href attributes
        src_attrs = doc.findAll(src=re.compile('.*'))
        href_attrs = doc.findAll(href=re.compile('.*'))

        # Look for urls
        url_pattern = re.compile('url\(.*\)')
        urls = []
        for url in doc.findAll(text=url_pattern):
            url = re.search(url_pattern, url)
            url = re.sub('url\([\'\"]', '', url.group(0))
            url = re.sub('[\'\"]\)', '', url)
            urls.append(url)

        src_paths = [src_tag['src'] for src_tag in src_attrs]
        href_paths = [href_tag['href'] for href_tag in href_attrs]

        paths = src_paths + href_paths + urls
        for path in paths:
            # Ignore http and mailto links
            if not(path.startswith('http:')) and \
               not(path.startswith('mailto:')):
                support_files.append(path)

        return support_files
