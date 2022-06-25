#! /usr/bin/env python
#
# Copyright (C) 2017 Sony Interactive Entertainment Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import importlib
import json
import os
import sys
import zipfile

download = importlib.import_module('download-github-release')

repo = 'WebKitForWindows/WebKitRequirements'
file = 'WebKitRequirementsWin64.zip'
output = os.getenv('WEBKIT_LIBRARIES', 'WebKitLibraries/win')
options = [repo, file, '-o', output]

# Check if there's a specific version to request
config_path = os.path.join(output, file) + '.config'
if os.path.exists(config_path):
    with open(config_path) as config_file:
        options += ['-r', json.load(config_file)['tag_name']]

result = download.main(options)

# Only unzip if required
if result == download.Status.DOWNLOADED:
    print('Extracting release to {}...'.format(output))
    zip = zipfile.ZipFile(os.path.join(output, file), 'r')
    zip.extractall(output)
    zip.close()
elif result == download.Status.COULD_NOT_FIND:
    sys.exit(1)
