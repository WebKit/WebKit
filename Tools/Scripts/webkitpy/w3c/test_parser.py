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

from collections import deque

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

        matches = self.reference_links_of_type('match')
        mismatches = self.reference_links_of_type('mismatch')

        # Manual tests may also have properties that make them look like non-manual reference or JS
        # tests, so exclude them first.
        if self.is_wpt_manualtest() and not self.is_reference_filename():
            test_info = {'test': self.filename, 'manualtest': True}

        elif matches or mismatches:
            if len(matches) > 1 or len(mismatches) > 1:
                # FIXME: Is this actually true? We should fix this.
                _log.warning('Multiple references of the same type are not supported. Importing the first ref defined in %s',
                             self.filesystem.basename(self.filename))

            refs_to_import = []
            if matches:
                refs_to_import.append(matches[0])
            if mismatches:
                refs_to_import.append(mismatches[0])

            test_info = {'test': self.filename}

            for ref_to_import in refs_to_import:
                try:
                    href_match_file = ref_to_import['href'].strip()
                    if href_match_file.startswith('/'):
                        ref_file = self.filesystem.join(self.source_root_directory, href_match_file.lstrip('/'))
                    else:
                        ref_file = self.filesystem.join(self.filesystem.dirname(self.filename), href_match_file)

                    reference_type = ref_to_import['rel'][0] if isinstance(ref_to_import['rel'], list) else ref_to_import['rel']
                except KeyError as e:
                    # FIXME: Figure out what to do w/ invalid test files.
                    _log.error('%s has a reference link but is missing the "href"', self.filesystem)
                    return None

                if (ref_file == self.filename):
                    return {'referencefile': self.filename}

                if self.ref_doc is None:
                    self.load_file(ref_file, True)

                test_info[reference_type + '_reference'] = ref_file
                test_info[reference_type + '_reference_support_info'] = {}

                # If the ref file does not live in the same directory as the test file, check it for support files
                if self.filesystem.dirname(ref_file) != self.filesystem.dirname(self.filename):
                    reference_support_files = self.support_files(self.ref_doc)
                    if len(reference_support_files) > 0:
                        reference_relpath = self.filesystem.relpath(self.filesystem.dirname(self.filename), self.filesystem.dirname(ref_file)) + self.filesystem.sep
                        test_info[reference_type + '_reference_support_info'] = {'reference_relpath': reference_relpath, 'files': reference_support_files}

        elif self.is_wpt_crashtest():
            test_info = {'test': self.filename, 'crashtest': True}
        elif self.is_jstest():
            test_info = {'test': self.filename, 'jstest': True}
        elif self.is_reference_filename():
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
        if self.filename.find('-manual.') != -1:
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

    def is_wpt_crashtest(self):
        return self.filename.find('-crash.') != -1

    def is_reference_filename(self):
        # From tools/manifest/sourcefile.py in WPT repository
        # https://github.com/web-platform-tests/wpt/blob/22f29564bb82b407aeaf6507c8efffdbd51b9974/tools/manifest/sourcefile.py#L405
        reference_file_re = re.compile(r'(^|[\-_])(not)?ref[0-9]*([\-_]|$)')
        return "/reference/" in self.filename or bool(reference_file_re.search(self.filename))

    def is_slow_test(self):
        return any([match.name == 'meta' and match['name'] == 'timeout' for match in self.test_doc.findAll(content='long')])

    def has_fuzzy_metadata(self):
        return any([match['name'] == 'fuzzy' for match in self.test_doc.findAll('meta')])

    def fuzzy_metadata(self):
        fuzzy_nodes = self.test_doc.findAll('meta', attrs={"name": "fuzzy"})
        if not fuzzy_nodes:
            return None

        args = [u"maxDifference", u"totalPixels"]
        result = {}

        # Taken from wpt/tools/manifest/sourcefile.py, and copied to avoid having webkitpy depend on wpt.
        for node in fuzzy_nodes:
            content = node['content']
            key = None
            # from parse_ref_keyed_meta; splits out the optional reference prefix.
            parts = content.rsplit(u":", 1)
            if len(parts) == 1:
                fuzzy_data = parts[0]
            else:
                ref_file = parts[0]
                key = ref_file
                fuzzy_data = parts[1]

            ranges = fuzzy_data.split(u";")
            if len(ranges) != 2:
                raise ValueError("Malformed fuzzy value \"%s\"" % fuzzy_data)

            arg_values = {}  # type: Dict[Text, List[int]]
            positional_args = deque()  # type: Deque[List[int]]

            for range_str_value in ranges:  # type: Text
                name = None  # type: Optional[Text]
                if u"=" in range_str_value:
                    name, range_str_value = [part.strip() for part in range_str_value.split(u"=", 1)]
                    if name not in args:
                        raise ValueError("%s is not a valid fuzzy property" % name)
                    if arg_values.get(name):
                        raise ValueError("Got multiple values for argument %s" % name)

                if u"-" in range_str_value:
                    range_min, range_max = range_str_value.split(u"-")
                else:
                    range_min = range_str_value
                    range_max = range_str_value
                try:
                    range_value = [int(x.strip()) for x in (range_min, range_max)]
                except ValueError:
                    raise ValueError("Fuzzy value %s must be a range of integers" % range_str_value)

                if name is None:
                    positional_args.append(range_value)
                else:
                    arg_values[name] = range_value

            result[key] = []
            for arg_name in args:
                if arg_values.get(arg_name):
                    arg_value = arg_values.pop(arg_name)
                else:
                    arg_value = positional_args.popleft()
                result[key].append(arg_value)
            assert len(arg_values) == 0 and len(positional_args) == 0

        return result

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
