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

import argparse
import json
import os
import sys
import urllib2

PUBLIC_GITHUB_API_ENDPOINT = 'https://api.github.com/'

DESCRIPTION = '''Downloads the latest release binary from a GitHub repository.

Intended for download of vswhere.exe and WinCairoRequirements.zip,
but may be used for arbitrary binaries / repositories.

Checks whether the latest version already exists in the output directory
(by looking for a .version file saved alongside the release binary) --
if so, download is skipped; otherwise any existing version is overwritten.
'''


class Status:
    DOWNLOADED = 0
    UP_TO_DATE = 1
    COULD_NOT_FIND = 2
    USE_EXISTING_VERSION = 3


def parse_args(argv):
    parser = argparse.ArgumentParser(description=DESCRIPTION, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('repo', help='GitHub repository slug (e.g., "org/repo")')
    parser.add_argument('filename', help='filename of release binary to download (e.g., "foo.exe", "bar.zip")')
    parser.add_argument('-o', '--output-dir', default='.', help='output directory (defaults to working directory)')
    parser.add_argument('-e', '--endpoint', default=PUBLIC_GITHUB_API_ENDPOINT, help='GitHub API endpoint (defaults to api.github.com)')
    parser.add_argument('-t', '--token', default=None, help='GitHub API OAuth token (for private repos/endpoints)')
    return parser.parse_args(argv)


def find_latest_release(args):
    url = '{}/repos/{}/releases/latest'.format(args.endpoint.rstrip('/'), args.repo)

    request = urllib2.Request(url)
    request.add_header('Accept', 'application/vnd.github.v3+json')
    if args.token:
        request.add_header('Authorization', 'token {}'.format(args.token))

    try:
        response = urllib2.urlopen(request)
    except urllib2.HTTPError, error:
        if error.code != 404:
            print error.code, error.reason
        return None, None

    data = json.loads(response.read())
    for asset in data['assets']:
        if asset['name'] == args.filename:
            version_info = {'tag_name': data['tag_name'], 'updated_at': asset['updated_at']}
            return asset['browser_download_url'], version_info
    return None, None


def existing_version_info(version_info_path):
    if not os.path.exists(version_info_path):
        return None

    with open(version_info_path) as file:
        return json.load(file)


def has_latest_release(version_info_path, version_info):
    return existing_version_info(version_info_path) == version_info


def download_release(source_url, target_path):
    with open(target_path, 'wb') as file:
        file.write(urllib2.urlopen(source_url).read())


def save_version_info(version_info_path, version_info):
    with open(version_info_path, 'w') as file:
        json.dump(version_info, file)


def main(argv):
    args = parse_args(argv)

    binary_path = os.path.join(args.output_dir, args.filename)
    version_info_path = binary_path + '.version'

    print 'Seeking latest release of {} from {}...'.format(args.filename, args.repo)

    try:
        release_url, version_info = find_latest_release(args)

        if not release_url:
            print 'No release found!'
            return Status.COULD_NOT_FIND

        if has_latest_release(version_info_path, version_info):
            print 'Already up-to-date:', existing_version_info(version_info_path)
            return Status.UP_TO_DATE

    except urllib2.URLError, error:
        print error

        version_info = existing_version_info(version_info_path)
        if version_info:
            print 'Use existing version:', version_info
            return Status.USE_EXISTING_VERSION
        else:
            return Status.COULD_NOT_FIND

    if not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)

    print 'Downloading {} to {}...'.format(version_info['tag_name'], os.path.abspath(args.output_dir))
    download_release(release_url, binary_path)
    save_version_info(version_info_path, version_info)
    print 'Done!'

    return Status.DOWNLOADED


if __name__ == '__main__':
    result = main(sys.argv[1:])

    if result == Status.COULD_NOT_FIND:
        sys.exit(1)
