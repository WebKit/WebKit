# Copyright (C) 2018 Apple Inc. All rights reserved.
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

import os

BUG_SERVER_HOST = 'bugs.webkit.org'
BUG_SERVER_URL = 'https://{}/'.format(BUG_SERVER_HOST)
PATCH_FOLDER = '/tmp/'

BUILDBOT_SERVER_HOST = 'ews-build.webkit.org'
BUILDBOT_SERVER_PORT = '5555'
BUILDBOT_PB_USERNAME = os.getenv('BUILDBOT_PB_USERNAME', 'sampleuser')
BUILDBOT_PB_PASSWORD = os.getenv('BUILDBOT_PB_PASSWORD', 'samplepass')

SUCCESS = 0
ERR_UNEXPECTED = -1
ERR_EXISTING_PATCH = -2
ERR_NON_EXISTING_PATCH = -3
ERR_INVALID_PATCH_ID = -4
