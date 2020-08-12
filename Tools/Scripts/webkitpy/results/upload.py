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

from webkitpy.port.config import apple_additions

import math
import os
import json
import requests
import sys
import time

import platform as host_platform


class Upload(object):
    API_KEY = os.getenv('RESULTS_SERVER_API_KEY')
    UPLOAD_ENDPOINT = '/api/upload'
    ARCHIVE_UPLOAD_ENDPOINT = '/api/upload/archive'
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
                    [obj.BUILDBOT_DETAILS[i] for i in range(len(obj.BUILDBOT_DETAILS)) if buildbot_args[i]],
                )))

            def unpack_test(current, path_to_test, data):
                if len(path_to_test) == 1:
                    current[path_to_test[0]] = data
                    return
                if not current.get(path_to_test[0]):
                    current[path_to_test[0]] = {}
                unpack_test(current[path_to_test[0]], path_to_test[1:], data)

            results = {}

            # FIXME: Python 2 removal, this dictionary is large enough that Python 2 can't just use items
            if sys.version_info > (3, 0):
                for test, data in obj.results.items():
                    unpack_test(results, test.split('/'), data)
            else:
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
        self.timestamp = int(timestamp or time.time())
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
        config.update({key: value for key, value in optional_data.items() if value is not None})
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
        stats.update({key: value for key, value in optional_data.items() if value is not None})
        return stats

    @staticmethod
    def create_test_result(expected=None, actual=None, log=None, **kwargs):
        result = dict(**kwargs)

        # Tests which don't declare expectations or results are assumed to have passed.
        optional_data = dict(expected=expected, actual=actual, log=log)
        result.update({key: value for key, value in optional_data.items() if value is not None})
        return result

    @staticmethod
    def certificate_chain():
        if apple_additions() and getattr(apple_additions(), 'certificate_chain', False):
            return apple_additions().certificate_chain()

        import certifi
        return certifi.where()

    def upload(self, hostname, log_line_func=lambda val: sys.stdout.write(val + '\n')):
        try:
            data = Upload.Encoder().default(self)
            if self.API_KEY:
                data['api_key'] = self.API_KEY
            response = requests.post(
                '{}{}'.format(hostname, self.UPLOAD_ENDPOINT),
                headers={'Content-type': 'application/json'},
                data=json.dumps(data),
                verify=Upload.certificate_chain(),
            )
        except requests.exceptions.ConnectionError:
            log_line_func(' ' * 4 + 'Failed to upload to {}, results server not online'.format(hostname))
            return False
        except ValueError as e:
            log_line_func(' ' * 4 + 'Failed to encode upload data: {}'.format(e))
            return False

        if response.status_code != 200:
            log_line_func(' ' * 4 + 'Error uploading to {}'.format(hostname))
            try:
                log_line_func(' ' * 8 + response.json().get('description'))
            except ValueError:
                for line in response.text.splitlines():
                    log_line_func(' ' * 8 + line)
            return False

        log_line_func(' ' * 4 + 'Uploaded results to {}'.format(hostname))
        return True

    def upload_archive(self, hostname, archive, log_line_func=lambda val: sys.stdout.write(val + '\n')):
        try:
            meta_data = dict(
                version=self.VERSION,
                suite=self.suite,
                configuration=json.dumps(self.configuration or self.create_configuration()),
                commits=json.dumps(self.commits),
            )
            if self.timestamp:
                meta_data['timestamp'] = self.timestamp
            if self.API_KEY:
                meta_data['api_key'] = self.API_KEY
            meta_data['Content-type'] = 'application/octet-stream'
            response = requests.post(
                '{}{}'.format(hostname, self.ARCHIVE_UPLOAD_ENDPOINT),
                data=meta_data,
                files=dict(file=archive),
                verify=Upload.certificate_chain(),
            )

        except requests.exceptions.ConnectionError:
            archive.seek(0, os.SEEK_END)
            log_line_func(' ' * 4 + 'Failed to upload test archive to {}, results server dropped connection, likely due to archive size ({} MB).'.format(
                hostname,
                math.floor(float(archive.tell()) / 1024) / 1024,
            ))
            log_line_func(' ' * 4 + 'This error is not fatal, continuing')
            return True
        except ValueError as e:
            log_line_func(' ' * 4 + 'Failed to encode archive reference data: {}'.format(e))
            return False

        # FIXME: <rdar://problem/56154412> do not fail test runs because of 403 errors
        if response.status_code not in [200, 403, 413]:
            log_line_func(' ' * 4 + 'Error uploading archive to {}'.format(hostname))
            try:
                log_line_func(' ' * 8 + response.json().get('description'))
            except ValueError:
                for line in response.text.splitlines():
                    log_line_func(' ' * 8 + line)
            return False

        if response.status_code == 200:
            log_line_func(' ' * 4 + 'Uploaded test archive to {}'.format(hostname))
        else:
            log_line_func(' ' * 4 + 'Upload to {} failed:'.format(hostname))
            try:
                log_line_func(' ' * 8 + response.json().get('description'))
            except ValueError:
                for line in response.text.splitlines():
                    log_line_func(' ' * 8 + line)
            log_line_func(' ' * 4 + 'This error is not fatal, continuing')
        return True
