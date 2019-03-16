# Copyright (C) 2019 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import webkitpy.thirdparty.autoinstalled.requests

import collections
import json
import requests
import time
import unittest

from webkitpy.results.upload import Upload
from webkitpy.thirdparty import mock


class UploadTest(unittest.TestCase):

    class Options(object):
        def __init__(self, **kwargs):
            for key, value in kwargs.iteritems():
                setattr(self, key, value)

    class MockResponse(object):
        def __init__(self, status_code=200, text=''):
            self.status_code = status_code
            self.text = text

        def json(self):
            return json.loads(self.text)

    @staticmethod
    def normalize(data):
        if isinstance(data, basestring):
            return str(data)
        elif isinstance(data, collections.Mapping):
            return dict(map(UploadTest.normalize, data.iteritems()))
        elif isinstance(data, collections.Iterable):
            return type(data)(map(UploadTest.normalize, data))
        return data

    @staticmethod
    def raise_requests_ConnectionError():
        raise requests.exceptions.ConnectionError()

    def test_encoding(self):
        start_time, end_time = time.time() - 3, time.time()
        upload = Upload(
            suite='webkitpy-tests',
            configuration=Upload.create_configuration(
                platform='mac',
                version='10.13.0',
                version_name='High Sierra',
                architecture='x86_64',
                sdk='17A405',
            ),
            details=Upload.create_details(link='https://webkit.org'),
            commits=[Upload.create_commit(
                repository_id='webkit',
                id='5',
                branch='trunk',
            )],
            run_stats=Upload.create_run_stats(
                start_time=start_time,
                end_time=end_time,
                tests_skipped=0,
            ),
            results={
                'webkitpy.test1': {},
                'webkitpy.test2': Upload.create_test_result(expected=Upload.Expectations.PASS, actual=Upload.Expectations.FAIL),
            },
        )
        generated_dict = self.normalize(json.loads(json.dumps(upload, cls=Upload.Encoder)))

        self.assertEqual(generated_dict['version'], 0)
        self.assertEqual(generated_dict['suite'], 'webkitpy-tests')
        self.assertEqual(generated_dict['configuration'], self.normalize(dict(
            platform='mac',
            is_simulator=False,
            version='10.13.0',
            version_name='High Sierra',
            architecture='x86_64',
            sdk='17A405',
        )))
        self.assertEqual(generated_dict['commits'], [dict(
            repository_id='webkit',
            id='5',
            branch='trunk',
        )])
        self.assertEqual(generated_dict['test_results']['details'], self.normalize(dict(link='https://webkit.org')))
        self.assertEqual(generated_dict['test_results']['run_stats'], self.normalize(dict(
            start_time=start_time,
            end_time=end_time,
            tests_skipped=0,
        )))
        self.assertEqual(generated_dict['test_results']['results'], self.normalize({
            'webkitpy.test1': {},
            'webkitpy.test2': Upload.create_test_result(expected=Upload.Expectations.PASS, actual=Upload.Expectations.FAIL),
        }))

    def test_upload(self):
        upload = Upload(
            suite='webkitpy-tests',
            commits=[Upload.create_commit(
                repository_id='webkit',
                id='5',
                branch='trunk',
            )],
        )

        with mock.patch('requests.post', new=lambda url, data: self.MockResponse()):
            self.assertTrue(upload.upload('https://webkit.org/results', log_line_func=lambda _: None))

        with mock.patch('requests.post', new=lambda url, data: self.raise_requests_ConnectionError()):
            self.assertFalse(upload.upload('https://webkit.org/results', log_line_func=lambda _: None))

        mock_404 = mock.patch('requests.post', new=lambda url, data: self.MockResponse(
            status_code=404,
            text=json.dumps(dict(description='No such address')),
        ))
        with mock_404:
            self.assertFalse(upload.upload('https://webkit.org/results', log_line_func=lambda _: None))

    def test_packed_test(self):
        upload = Upload(
            suite='webkitpy-tests',
            commits=[Upload.create_commit(
                repository_id='webkit',
                id='5',
                branch='trunk',
            )],
            results={
                'dir1/sub-dir1/test1': Upload.create_test_result(actual=Upload.Expectations.FAIL),
                'dir1/sub-dir1/test2': Upload.create_test_result(actual=Upload.Expectations.TIMEOUT),
                'dir1/sub-dir2/test3': {},
                'dir1/sub-dir2/test4': {},
                'dir2/sub-dir3/test5': {},
                'dir2/test6': {},
            }
        )
        generated_dict = self.normalize(json.loads(json.dumps(upload, cls=Upload.Encoder)))
        self.assertEqual(generated_dict['test_results']['results'], self.normalize({
            'dir1': {
                'sub-dir1': {
                    'test1': {'actual': Upload.Expectations.FAIL},
                    'test2': {'actual': Upload.Expectations.TIMEOUT},
                }, 'sub-dir2': {
                    'test3': {},
                    'test4': {},
                }
            }, 'dir2': {
                'sub-dir3': {'test5': {}},
                'test6': {},
            },
        }))

    def test_no_suite(self):
        upload = Upload(
            commits=[Upload.create_commit(
                repository_id='webkit',
                id='5',
                branch='trunk',
            )],
        )
        with self.assertRaises(ValueError):
            json.dumps(upload, cls=Upload.Encoder)

    def test_no_commits(self):
        upload = Upload(
            suite='webkitpy-tests',
        )
        with self.assertRaises(ValueError):
            json.dumps(upload, cls=Upload.Encoder)

    def test_buildbot(self):
        upload = Upload(
            suite='webkitpy-tests',
            commits=[Upload.create_commit(
                repository_id='webkit',
                id='5',
                branch='trunk',
            )],
            details=Upload.create_details(options=self.Options(
                buildbot_master='webkit.org',
                builder_name='Queue-1',
                build_number=1,
        )))
        with self.assertRaises(ValueError):
            json.dumps(upload, cls=Upload.Encoder)

        upload.details['buildbot-worker'] = 'bot123'
        generated_dict = self.normalize(json.loads(json.dumps(upload, cls=Upload.Encoder)))
        self.assertEqual(generated_dict['test_results']['details'], self.normalize({
            'buildbot-master': 'webkit.org',
            'builder-name': 'Queue-1',
            'build-number': 1,
            'buildbot-worker': 'bot123',
        }))
