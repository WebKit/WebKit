#!/usr/bin/env python

# Copyright (C) 2011 Apple Inc. All rights reserved.
# Copyright (C) 2014 University of Szeged. All rights reserved.
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

import json
import md5


def create_mock_slave_passwords_dict():
    with open('config.json', 'r') as config_json:
        config_dict = json.load(config_json)
    return dict([(slave['name'], '1234') for slave in config_dict['slaves']])

if __name__ == '__main__':
    with open('passwords.json', 'w') as passwords_file:
        passwords_file.write(json.dumps(create_mock_slave_passwords_dict(), indent=4, sort_keys=True))

    with open('auth.json', 'w') as auth_json_file:
        auth_json_file.write("""{
    "trac_credentials": "credentials.cfg",
    "webkit_committers": "committers.cfg"
}""")

    with open('credentials.cfg', 'w') as credentials_file:
        credentials_file.write("committer@webkit.org:Mac OS Forge:%s" % md5.new("committer@webkit.org:Mac OS Forge:committerpassword").hexdigest())

    with open('committers.cfg', 'w') as committers_file:
        committers_file.write("""[groups]
webkit = committer@webkit.org
""")
