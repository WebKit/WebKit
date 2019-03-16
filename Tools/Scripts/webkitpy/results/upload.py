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

import json
import requests
import sys

import platform as host_platform


class Upload(object):
    UPLOAD_ENDPOINT = '/api/upload'
    BUILDBOT_DETAILS = ['buildbot-master', 'builder-name', 'build-number', 'buildbot-worker']
    VERSION = 0

    class Expectations:
        # These are ordered by priority, meaning that a test which both crashes and has
        # a warning should be considered to have crashed.
        ORDER = [
            'CRASH',
            'TIMEOUT',
            'IMAGE',   # Image-diff
            'AUDIO',   # Audio-diff
            'TEXT',    # Text-diff
            'FAIL',
            'ERROR',
            'WARNING',
            'PASS',
        ]
        CRASH, TIMEOUT, IMAGE, AUDIO, TEXT, FAIL, ERROR, WARNING, PASS = ORDER

    class Encoder(json.JSONEncoder):

        def default(self, obj):
            if not isinstance(obj, Upload):
                return super(Upload.Encoder, self).default(obj)

            if not obj.suite:
                raise ValueError('No suite specified to results upload')
            if not obj.commits:
                raise ValueError('No commits specified to results upload')

            details = obj.details or obj.create_details()
            buildbot_args = [details.get(arg, None) is None for arg in obj.BUILDBOT_DETAILS]
            if any(buildbot_args) and not all(buildbot_args):
                raise ValueError('All buildbot details must be defined for upload, details missing: {}'.format(', '.join(
                    [obj.BUILDBOT_DETAILS[i] for i in xrange(len(obj.BUILDBOT_DETAILS)) if buildbot_args[i]],
                )))

            def unpack_test(current, path_to_test, data):
                if len(path_to_test) == 1:
                    current[path_to_test[0]] = data
                    return
                if not current.get(path_to_test[0]):
                    current[path_to_test[0]] = {}
                unpack_test(current[path_to_test[0]], path_to_test[1:], data)

            results = {}
            for test, data in obj.results.iteritems():
                unpack_test(results, test.split('/'), data)

            result = dict(
                version=obj.VERSION,
                suite=obj.suite,
                configuration=obj.configuration or obj.create_configuration(),
                commits=obj.commits,
                test_results=dict(
                    details=details,
                    run_stats=obj.run_stats or obj.create_run_stats(),
                    results=results,
                ),
            )
            if obj.timestamp:
                result['timestamp'] = obj.timestamp
            return result

    def __init__(self, suite=None, configuration=None, commits=[], timestamp=None, details=None, run_stats=None, results={}):
        self.suite = suite
        self.configuration = configuration
        self.commits = commits
        self.timestamp = timestamp
        self.details = details
        self.run_stats = run_stats
        self.results = results

    @staticmethod
    def create_configuration(
            platform=None,
            is_simulator=False,
            version=None,
            architecture=None,
            version_name=None,
            model=None,
            style=None,   # Debug/Production/Release
            flavor=None,  # Dumping ground suite-wide configuration changes (ie, GuardMalloc)
            sdk=None,
        ):

        # This deviates slightly from the rest of webkitpy, but it allows this file to be entirely portable.
        config = dict(
            platform=platform or (host_platform.system() if host_platform.system() != 'Darwin' else 'mac').lower(),
            is_simulator=is_simulator,
            version=version or (host_platform.release() if host_platform.system() != 'Darwin' else host_platform.mac_ver()[0]),
            architecture=architecture or host_platform.machine(),
        )
        optional_data = dict(version_name=version_name, model=model, style=style, flavor=flavor, sdk=sdk)
        config.update({key: value for key, value in optional_data.iteritems() if value is not None})
        return config

    @staticmethod
    def create_commit(repository_id, id, branch=None):
        commit = dict(repository_id=repository_id, id=id)
        if branch:
            commit['branch'] = branch
        return commit

    @staticmethod
    def create_details(link=None, options=None, **kwargs):
        result = dict(**kwargs)
        if link:
            result['link'] = link
        if not options:
            return result

        for element in Upload.BUILDBOT_DETAILS:
            value = getattr(options, element.replace('-', '_'), None)
            if value is not None:
                result[element] = value
        return result

    @staticmethod
    def create_run_stats(start_time=None, end_time=None, tests_skipped=None, **kwargs):
        stats = dict(**kwargs)
        optional_data = dict(start_time=start_time, end_time=end_time, tests_skipped=tests_skipped)
        stats.update({key: value for key, value in optional_data.iteritems() if value is not None})
        return stats

    @staticmethod
    def create_test_result(expected=None, actual=None, log=None, **kwargs):
        result = dict(**kwargs)

        # Tests which don't declare expectations or results are assumed to have passed.
        optional_data = dict(expected=expected, actual=actual, log=log)
        result.update({key: value for key, value in optional_data.iteritems() if value is not None})
        return result

    def upload(self, url, log_line_func=lambda val: sys.stdout.write(val + '\n')):
        try:
            response = requests.post(url + self.UPLOAD_ENDPOINT, data=json.dumps(self, cls=Upload.Encoder))
        except requests.exceptions.ConnectionError:
            log_line_func(' ' * 4 + 'Failed to upload to {}, results server not online'.format(url))
            return False
        except ValueError as e:
            log_line_func(' ' * 4 + 'Failed to encode upload data: {}'.format(e))
            return False

        if response.status_code != 200:
            log_line_func(' ' * 4 + 'Error uploading to {}:'.format(url))
            log_line_func(' ' * 8 + response.json()['description'])
            return False

        log_line_func(' ' * 4 + 'Uploaded results to {}'.format(url))
        return True
