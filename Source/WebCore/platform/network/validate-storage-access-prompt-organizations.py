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
from urllib.parse import urlparse


def validateConfiguration(organizationConfig, input_file="does-not-exist"):
    if len(organizationConfig) == 0:
        raise Exception("Failed to parse {}".format(input_file))
    if "version" not in organizationConfig:
        raise Exception("Version not declared")
    if organizationConfig["version"] != 1:
        raise Exception("Unrecognized version: {}".format(organizationConfig["version"]))
    allDomains = dict()
    for organizationName in organizationConfig:
        if organizationName == "version":
            continue
        if type(organizationConfig[organizationName]) is not dict:
            raise Exception("{} should be an object".format(organizationName))
        if "quirkDomains" not in organizationConfig[organizationName]:
            raise Exception("{} must contain a 'quirkDomains' object".format(organizationName))
        quirkedDomains = organizationConfig[organizationName]["quirkDomains"]
        if type(quirkedDomains) is not dict:
            raise Exception("{} quirked domains should be an object".format(organizationName))
        allDomains[organizationName] = set()
        for domain in quirkedDomains:
            allDomains[organizationName].add(domain)
            if type(quirkedDomains[domain]) is not list:
                raise Exception("{}'s {} quirk domains should be a list".format(organizationName, domain))
            if len(quirkedDomains[domain]) < 2 and len(quirkedDomains) < 2:
                raise Exception("{}'s {} quirk domains requires at least two domains".format(organizationName, domain))
            if len(quirkedDomains[domain]) < 1:
                raise Exception("{}'s {} quirk domains requires at least one domain".format(organizationName, domain))
            allDomains[organizationName].update(quirkedDomains[domain])

        if "triggerPages" not in organizationConfig[organizationName]:
            raise Exception("{} must contain a 'triggerPages' list".format(organizationName))
        if type(organizationConfig[organizationName]["triggerPages"]) is not list:
            raise Exception("{} trigger pages should be a list of pages".format(organizationName))
        for page in organizationConfig[organizationName]['triggerPages']:
            parsedURL = urlparse(page)
            foundQuirkDomain = False
            for domain in allDomains[organizationName]:
                if parsedURL.hostname.find(domain) != -1:
                    foundQuirkDomain = True
            if not foundQuirkDomain:
                raise Exception("{} trigger page '{}' is not a quirked domain".format(organizationName, page))
            foundQuirkDomain = False
            for mainFrameDomain in quirkedDomains:
                for subFrameDomain in quirkedDomains[mainFrameDomain]:
                    if parsedURL.hostname.find(subFrameDomain) != -1:
                        foundQuirkDomain = True
            if not foundQuirkDomain:
                raise Exception("{} trigger page '{}' is not a quirked subframe domain".format(organizationName, page))
    for organizationName1 in allDomains:
        for organizationName2 in allDomains:
            if organizationName1 == organizationName2:
                continue
            for domain in allDomains[organizationName1]:
                if domain in allDomains[organizationName2]:
                    raise Exception("{} and {} contain the same domain: '{}'".format(organizationName1, organizationName2, domain))


def runTests():
    try:
        validateConfiguration({})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Failed to parse does-not-exist")

    try:
        validateConfiguration({'example.com': ''})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Version not declared")

    try:
        validateConfiguration({'version': 0})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Unrecognized version: 0")

    try:
        validateConfiguration({'version': 1, 'Example Organization': 0})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example Organization should be an object")

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'notQuirkDomains': 0}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example Organization must contain a 'quirkDomains' object")

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'notQuirkDomains': {}}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example Organization must contain a 'quirkDomains' object")

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'quirkDomains': 0}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example Organization quirked domains should be an object")

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'quirkDomains': []}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example Organization quirked domains should be an object")

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'quirkDomains': {"site1.example": 0}}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example Organization's site1.example quirk domains should be a list")

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'quirkDomains': {"site1.example": []}}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example Organization's site1.example quirk domains requires at least two domains")

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'quirkDomains': {"site1.example": ["site2.example"]}, 'triggerPages': []}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example Organization's site1.example quirk domains requires at least two domains")

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'quirkDomains': {"site1.example": ["site2.example", "site3.example"]}, 'triggerPages': []}})
    except Exception as e:
        assert(False)

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'quirkDomains': {"site1.example": ["site2.example", "site3.example"], "site4.example": []}, 'triggerPages': []}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example Organization's site4.example quirk domains requires at least one domain")

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'quirkDomains': {"site1.example": ["site2.example", "site3.example"], "site4.example": ["site1.example"]}, 'triggerPages': []}})
    except Exception as e:
        assert(False)

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'quirkDomains': {"site1.example": ["site2.example", "site3.example"], "site4.example": ["site1.example"]}}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example Organization must contain a 'triggerPages' list")

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'quirkDomains': {"site1.example": ["site2.example", "site3.example"], "site4.example": ["site1.example"]}, "triggerPages": 0}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example Organization trigger pages should be a list of pages")

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'quirkDomains': {"site1.example": ["site2.example", "site3.example"], "site4.example": ["site1.example"]}, "triggerPages": ["https://not-a-site.example/"]}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example Organization trigger page 'https://not-a-site.example/' is not a quirked domain")

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'quirkDomains': {"site1.example": ["site2.example", "site3.example"], "site4.example": ["site1.example"]}, "triggerPages": ["https://site4.example/"]}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example Organization trigger page 'https://site4.example/' is not a quirked subframe domain")

    try:
        validateConfiguration({'version': 1, 'Example Organization': {'quirkDomains': {"site1.example": ["site2.example", "site3.example"], "site4.example": ["site1.example"]}, "triggerPages": ["https://site2.example/"]}})
    except Exception as e:
        assert(False)

    try:
        validateConfiguration({'version': 1, 'Example1 Organization': {'quirkDomains': {"site1.example": ["site2.example", "site3.example"], "site4.example": ["site1.example"]}, "triggerPages": ["https://site2.example/"]}, 'Example2 Organization': {'quirkDomains': 0}})
        assert(False)
    except Exception as e:
        pass

    try:
        validateConfiguration({'version': 1, 'Example1 Organization': {'quirkDomains': {"site1.example": ["site2.example", "site3.example"], "site4.example": ["site1.example"]}, "triggerPages": ["https://site2.example/"]}, 'Example2 Organization': {'quirkDomains': {"anothersite1.example": ["anothersite2.example", "site3.example"], "anothersite4.example": ["anothersite1.example"]}, "triggerPages": []}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Example1 Organization and Example2 Organization contain the same domain: 'site3.example'")
        pass


def main():
    parser = argparse.ArgumentParser(description='Generate UserAgentOverride.h')
    parser.add_argument('--input')
    parser.add_argument('--test', action='store_const', const=True)
    args = parser.parse_args()

    if args.input and args.test:
        raise Exception("Both input file and testing were specified. Only specify one of them.")

    if args.input:
        with open(args.input, 'r', encoding='utf-8') as input_file:
            validateConfiguration(json.load(input_file), input_file)
        return

    if args.test:
        runTests()
        return

    raise Exception("No action specified. Specify either --input or --test")


if __name__ == '__main__':
    main()
