#!/usr/bin/env python
# Copyright (C) 2012 Google Inc. All rights reserved.
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

import logging
import unittest

import generate_builders_json


class GenerateBuildersJsonTest(unittest.TestCase):

    def test_master_json_url(self):
        self.assertEqual(generate_builders_json.master_json_url('http://base'), 'http://base/json/builders')

    def test_builder_json_url(self):
        self.assertEqual(generate_builders_json.builder_json_url('http://base', 'dummybuilder'), 'http://base/json/builders/dummybuilder')

    def test_cached_build_json_url(self):
        self.assertEqual(generate_builders_json.cached_build_json_url('http://base', 'dummybuilder', 12345), 'http://base/json/builders/dummybuilder/builds/12345')
        self.assertEqual(generate_builders_json.cached_build_json_url('http://base', 'dummybuilder', '12345'), 'http://base/json/builders/dummybuilder/builds/12345')

    def test_generate_json_data(self):
        try:
            old_fetch_json = generate_builders_json.fetch_json

            fetched_urls = []

            def dummy_fetch_json(url):
                fetched_urls.append(url)

                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders':
                    return {'WebKit Win': None, 'WebKit Linux': None, 'WebKit Mac': None}
                if url == 'http://build.webkit.org/json/builders':
                    return {'Apple Mac SnowLeopard Tests': None, 'Chromium Mac Builder': None, 'GTK': None}

                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Linux':
                    return {'cachedBuilds': [1, 2], 'currentBuilds': []}
                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Win':
                    return {'cachedBuilds': [1, 2], 'currentBuilds': []}
                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Mac':
                    return {'cachedBuilds': [1, 2], 'currentBuilds': []}
                if url == 'http://build.webkit.org/json/builders/Apple%20Mac%20SnowLeopard%20Tests':
                    return {'cachedBuilds': [1, 2], 'currentBuilds': []}
                if url == 'http://build.webkit.org/json/builders/Chromium%20Mac%20Builder':
                    return {'cachedBuilds': [1, 2, 3], 'currentBuilds': [3]}
                if url == 'http://build.webkit.org/json/builders/GTK':
                    return {'cachedBuilds': [2], 'currentBuilds': []}

                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Linux/builds/2':
                    return {'steps': [{'name': 'webkit_tests'}, {'name': 'browser_tests'}, {'name': 'mini_installer_test'}, {'name': 'archive_test_results'}, {'name': 'compile'}]}
                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Win/builds/2':
                    return {'steps': [{'name': 'webkit_tests'}, {'name': 'mini_installer_test'}, {'name': 'archive_test_results'}, {'name': 'compile'}]}
                if url == 'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Mac/builds/2':
                    return {'steps': [{'name': 'browser_tests'}, {'name': 'mini_installer_test'}, {'name': 'archive_test_results'}, {'name': 'compile'}]}
                if url == 'http://build.webkit.org/json/builders/Apple%20Mac%20SnowLeopard%20Tests/builds/2':
                    return {'steps': [{'name': 'layout-test'}, {'name': 'archive_test_results'}, {'name': 'compile'}]}
                if url == 'http://build.webkit.org/json/builders/Chromium%20Mac%20Builder/builds/2':
                    return {'steps': [{'name': 'compile'}]}
                if url == 'http://build.webkit.org/json/builders/GTK/builds/2':
                    return {'steps': [{'name': 'layout-test'}, {'name': 'archive_test_results'}, {'name': 'compile'}]}

                logging.error('Cannot fetch fake url: %s' % url)

            generate_builders_json.fetch_json = dummy_fetch_json

            masters = [
                {'name': 'ChromiumWebkit', 'url': 'http://build.chromium.org/p/chromium.webkit'},
                {'name': 'webkit.org', 'url': 'http://build.webkit.org'},
            ]

            generate_builders_json.insert_builder_and_test_data(masters)

            expected_fetched_urls = [
                'http://build.chromium.org/p/chromium.webkit/json/builders',
                'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Linux',
                'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Linux/builds/2',
                'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Mac',
                'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Mac/builds/2',
                'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Win',
                'http://build.chromium.org/p/chromium.webkit/json/builders/WebKit%20Win/builds/2',
                'http://build.webkit.org/json/builders',
                'http://build.webkit.org/json/builders/Apple%20Mac%20SnowLeopard%20Tests',
                'http://build.webkit.org/json/builders/Apple%20Mac%20SnowLeopard%20Tests/builds/2',
                'http://build.webkit.org/json/builders/GTK',
                'http://build.webkit.org/json/builders/GTK/builds/2',
                'http://build.webkit.org/json/builders/Chromium%20Mac%20Builder',
                'http://build.webkit.org/json/builders/Chromium%20Mac%20Builder/builds/2']
            self.assertEqual(fetched_urls, expected_fetched_urls)

            expected_masters = [
                {
                    'url': 'http://build.chromium.org/p/chromium.webkit',
                    'tests': {
                        'browser_tests': {'builders': ['WebKit Linux', 'WebKit Mac']},
                        'mini_installer_test': {'builders': ['WebKit Linux', 'WebKit Mac', 'WebKit Win']},
                        'layout-tests': {'builders': ['WebKit Linux', 'WebKit Win']}},
                    'name': 'ChromiumWebkit'},
                {
                    'url': 'http://build.webkit.org',
                    'tests': {
                        'layout-tests': {'builders': ['Apple Mac SnowLeopard Tests', 'GTK']}},
                    'name': 'webkit.org'}]
            self.assertEqual(masters, expected_masters)

        finally:
            generate_builders_json.fetch_json = old_fetch_json

if __name__ == '__main__':
    unittest.main()
