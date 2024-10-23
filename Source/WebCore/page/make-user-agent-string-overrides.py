#!/usr/bin/env python3
#
# Copyright (C) 2024 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import json

platforms = ['mac', 'iphone', 'ipad']
user_agents = {
    'chrome': '110.0.0.0',
    'safari12': '12.1.2',
    'safari13': '13.1.2',
    'safari': '18.1',
}


def main():
    parser = argparse.ArgumentParser(description='Generate UserAgentOverride.h')
    parser.add_argument('--input')
    args = parser.parse_args()

    # Version 1 input JSON file contains an object with:
    #   - a version, currently the only supported value is the integer 1.
    #   - objects where the key is a domain:
    #     - objects where the key is either the target platform (e.g., "macos") or "all", and the value is either:
    #       - an array of attributes that should be used in the overridden string:
    #         - user agent (e.g., chrome)
    #         - platform (e.g., mac)
    #       - a string specifying a hard-coded user agent string
    #
    # For example, overriding iPhone with the Mac Chrome user agent string, and Mac with Chrome's could look like:
    #   "target.site.example": { "ios": [ "chrome", "mac" ], "mac": [ "chrome" ] }
    with open(args.input, 'r', encoding='utf-8') as input_file:
        overrides = json.load(input_file)
        if len(overrides) == 0:
            raise Exception("Failed to parse {}".format(args.input))
        if "version" not in overrides:
            raise Exception("Version not declared")
        if overrides["version"] != 1:
            raise Exception("Unrecognized version: {}".format(overrides["version"]))
        for domain in overrides:
            if domain == "version":
                continue
            for path in overrides[domain]:
                if type(overrides[domain][path]) is not dict:
                    raise Exception("{} attributes for path {} was not parsed as a dict".format(domain, path))
                for platform in overrides[domain][path]:
                    if platform not in platforms and platform != "all":
                        raise Exception("Unknown platform: {}".format(platform))
                    attributes = overrides[domain][path][platform]
                    if type(attributes) is not list:
                        if type(attributes) is not str:
                            raise Exception("{} attributes for {} with path {} is not a list or string".format(domain, platform, path))
                        continue
                    if len(attributes) > 2:
                        raise Exception("Too many attributes for {} on {} with path: {}".format(domain, platform, path, len(attributes)))
                    for attribute in attributes:
                        if attribute not in platforms and attribute not in user_agents:
                            raise Exception("Unknown attribute for {} on {}: {}".format(domain, platform, attribute))


if __name__ == '__main__':
    main()
