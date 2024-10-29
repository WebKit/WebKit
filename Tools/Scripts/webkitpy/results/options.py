# Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

import optparse
import argparse


def add_optargs(func, suite_name_flag='suite'):
    return [
        func('--report', action='append', dest='report_urls', help='URL (or URLs) to report test results to'),
        func('--buildbot-master', help='The url of the buildbot master.'),
        func('--builder-name', help='The name of the buildbot builder tests were run on.'),
        func('--build-number', help='The buildbot build number tests are associated with.'),
        func('--buildbot-worker', help='The buildbot worker tests were run on.'),
        func('--result-report-flavor', help='Optional flag for categorizing test runs which do not fit into other configuration options.'),
        func('--' + suite_name_flag, help='Optional flag for overriding reported suite name.', default=None),
    ]


def upload_options(suite_name_flag='suite'):
    return add_optargs(optparse.make_option)


def upload_args(suite_name_flag='suite'):
    parser = argparse.ArgumentParser(add_help=False)
    upload_options = parser.add_argument_group('Upload options')
    add_optargs(upload_options.add_argument)
    return parser
