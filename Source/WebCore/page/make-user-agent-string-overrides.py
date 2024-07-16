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

platforms = {
    'macos': {
        'platform': "Macintosh",
        'platformVersion': "Intel Mac OS X 10_15_7",
    }
}

user_agents = {
    'chrome': 'Chrome/110.0.0.0',
}


def validateConfiguration(overrides, input_file="does-not-exist"):
    if len(overrides) == 0:
        raise Exception("Failed to parse {}".format(input_file))
    if "version" not in overrides:
        raise Exception("Version not declared")
    if overrides["version"] != 1:
        raise Exception("Unrecognized version: {}".format(overrides["version"]))
    for domain in overrides:
        if domain == "version":
            continue
        for platform in overrides[domain]:
            if platform not in platforms and platform != "all":
                raise Exception("Unknown platform: {}".format(platform))
            attributes = overrides[domain][platform]
            if type(attributes) is not list:
                if type(attributes) is not str:
                    raise Exception("{} attributes for {} is not a list or string".format(domain, platform))
                continue
            if len(attributes) < 1:
                raise Exception("0 attributes provided for {} on {}".format(domain, platform))
            if len(attributes) > 2:
                raise Exception("Too many attributes for {} on {}: {}".format(domain, platform, len(attributes)))
            for attribute in attributes:
                if attribute not in platforms and attribute not in user_agents:
                    raise Exception("Unknown attribute for {} on {}: {}".format(domain, platform, attribute))


def generateFiles(inputFile):
    # Version 1 input JSON file contains an object with:
    #   - a version, currently the only supported value is the integer 1.
    #   - objects where the key is a domain:
    #     - objects where the key is either the target platform (e.g., "macos") or "all", and the value is either:
    #       - an array of attributes that should be used in the overridden string:
    #         - user agent (e.g., chrome)
    #         - platform (e.g., macos)
    #       - a string specifying a hard-coded user agent string
    #
    # For example, overriding iOS with the macOS Chrome user agent string, and macOS with Chrome's could look like:
    #   "target.site.example": { "ios": [ "chrome", "macos" ], "macos": [ "chrome" ] }
    with open(inputFile, 'r', encoding='utf-8') as input_file:
        validateConfiguration(json.load(input_file), input_file)

    with open('UserAgentStringOverrides.h', 'w') as output_file:
        def write(text):
            output_file.write(text)

        def writeln(text):
            write(text + '\n')

        writeln('''/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/EnumTraits.h>

namespace WebCore {

enum class UserAgentStringPlatform : uint8_t {
    Unknown,
    All,''')
        for platform in sorted(platforms.keys()):
            writeln("    " + platform.upper() + ",")
        writeln('''};

enum class UserAgentStringAgent : uint8_t {
    Unknown,''')
        for user_agent in sorted(user_agents.keys()):
            writeln("    " + user_agent.upper() + ",")
        writeln('''};

using DomainPlatformUserAgentString = std::pair<String, UserAgentStringPlatform>;
using UserAgentOverridesAttributes = std::variant<UserAgentStringPlatform, UserAgentStringAgent>;
using UserAgentOverridesMap = HashMap<DomainPlatformUserAgentString, std::variant<Vector<UserAgentOverridesAttributes>, String>>;

class UserAgentStringOverrides {
    WTF_MAKE_NONCOPYABLE(UserAgentStringOverrides); WTF_MAKE_FAST_ALLOCATED;
public:
    UserAgentStringOverrides() = default;
    std::optional<String> getUserAgentStringOverrideForDomain(const URL&, UserAgentStringPlatform) const;
    void setUserAgentStringQuirks(const UserAgentOverridesMap&);

    static UserAgentStringAgent stringToUserAgent(const String&);
    static UserAgentStringPlatform stringToUserAgentPlatform(const String&);
    static UserAgentStringPlatform getPlatform();

    WEBCORE_EXPORT static UserAgentOverridesMap parseUserAgentOverrides(const String&);

private:
    static UserAgentStringAgent attributesToAgent(const Vector<UserAgentOverridesAttributes>&);
    static StringView attributesToAgentString(const Vector<UserAgentOverridesAttributes>&);
    static StringView attributesToPlatformString(const Vector<UserAgentOverridesAttributes>&);
    static StringView attributesToPlatformVersionString(const Vector<UserAgentOverridesAttributes>&);
    static StringView userAgentToString(UserAgentStringPlatform, const Vector<UserAgentOverridesAttributes>&);

    UserAgentOverridesMap m_overrides;
};
} // namespace WebCore

namespace WTF {
template<typename T> struct DefaultHash;
template<> struct DefaultHash<WebCore::UserAgentStringPlatform> : IntHash<WebCore::UserAgentStringPlatform> { };

template<> struct EnumTraits<WebCore::UserAgentStringPlatform> {
    using values = EnumValues<
        WebCore::UserAgentStringPlatform,
        WebCore::UserAgentStringPlatform::Unknown,
        WebCore::UserAgentStringPlatform::All,''')
        i = 0
        for platform in sorted(platforms.keys()):
            write("        WebCore::UserAgentStringPlatform::" + platform.upper())
            if i != len(user_agents) - 1:
                write(",\n")
                i += 1
            else:
                write("\n")
        writeln("    >;")
        writeln('''};

template<> struct EnumTraits<WebCore::UserAgentStringAgent> {
    using values = EnumValues<
        WebCore::UserAgentStringAgent,
        WebCore::UserAgentStringAgent::Unknown,''')
        i = 0
        for user_agent in sorted(user_agents.keys()):
            write("        WebCore::UserAgentStringAgent::" + user_agent.upper())
            if i != len(user_agents) - 1:
                write(",\n")
                i += 1
            else:
                write("\n")
        writeln("    >;")
        writeln('''};
}''')

    with open('UserAgentStringOverridesGenerated.cpp', 'w') as output_file:
        def writeln(text):
            output_file.write(text + '\n')
        writeln('''/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "UserAgentStringOverrides.h"

#include <wtf/JSONValues.h>

namespace WebCore {

StringView UserAgentStringOverrides::attributesToPlatformString(const Vector<UserAgentOverridesAttributes>& attributes)
{
    for (auto attribute : attributes) {
        if (auto platform = std::get_if<UserAgentStringPlatform>(&attribute)) {
            switch (*platform) {''')
        for platform in sorted(platforms.keys()):
            writeln("            case UserAgentStringPlatform::{}:".format(platform.upper()))
            write("                return \"{}\"_s;".format(platforms[platform]['platform']))
        writeln('''
            case UserAgentStringPlatform::All:
                return "all"_s;
            case UserAgentStringPlatform::Unknown:
                return emptyString();
            }
        }
    }
    return emptyString();
}

StringView UserAgentStringOverrides::attributesToPlatformVersionString(const Vector<UserAgentOverridesAttributes>& attributes)
{
    for (auto attribute : attributes) {
        if (auto platform = std::get_if<UserAgentStringPlatform>(&attribute)) {
            switch (*platform) {''')
        for platform in sorted(platforms.keys()):
            writeln("            case UserAgentStringPlatform::{}:".format(platform.upper()))
            write("                return \"{}\"_s;".format(platforms[platform]['platformVersion']))
        writeln('''
            case UserAgentStringPlatform::All:
            case UserAgentStringPlatform::Unknown:
                return emptyString();
            }
        }
    }
    return emptyString();
}

StringView UserAgentStringOverrides::attributesToAgentString(const Vector<UserAgentOverridesAttributes>& attributes)
{
    for (auto attribute : attributes) {
        if (auto agent = std::get_if<UserAgentStringAgent>(&attribute)) {
            switch (*agent) {''')
        for agent in sorted(user_agents.keys()):
            writeln("            case UserAgentStringAgent::{}:".format(agent.upper()))
            write("                return \"{}\"_s;".format(user_agents[agent]))
        writeln('''
            case UserAgentStringAgent::Unknown:
                return emptyString();
            }
        }
    }
    return emptyString();
}

UserAgentStringPlatform UserAgentStringOverrides::stringToUserAgentPlatform(const String& platform)
{''')
        for platform in sorted(platforms.keys()):
            writeln("    if (platform == \"{}\"_s)".format(platform.lower()))
            write("        return UserAgentStringPlatform::{};".format(platform.upper()))
        writeln('''
    if (platform == "all"_s)
        return UserAgentStringPlatform::All;
    return UserAgentStringPlatform::Unknown;
}

UserAgentStringAgent UserAgentStringOverrides::stringToUserAgent(const String& agent)
{''')
        for agent in sorted(user_agents.keys()):
            writeln("    if (agent == \"{}\"_s)".format(agent.lower()))
            writeln("        return UserAgentStringAgent::{};".format(agent.upper()))
        writeln('''    return UserAgentStringAgent::Unknown;
}

} // namespace WebCore''')


def runTests():
    import unittest

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
        validateConfiguration({'version': 1, 'site.example': 0})
        assert(False)
    except Exception as e:
        pass

    try:
        validateConfiguration({'version': 1, 'site.example': {'macoss': 0}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Unknown platform: macoss")

    try:
        validateConfiguration({'version': 1, 'site.example': {'macos': 0}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "site.example attributes for macos is not a list or string")

    try:
        validateConfiguration({'version': 1, 'site.example': {'macos': "not a real user agent string"}})
    except Exception as e:
        raise e

    try:
        validateConfiguration({'version': 1, 'site.example': {'macos': []}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "0 attributes provided for site.example on macos")
    try:
        validateConfiguration({'version': 1, 'site.example': {'macos': ["macos", "chrome", "macos"]}})
        assert(False)
    except Exception as e:
        assert(e.args[0] == "Too many attributes for site.example on macos: 3")

    try:
        validateConfiguration({'version': 1, 'site.example': {'macos': ["macos", "chrome"]}})
    except Exception as e:
        raise e

    try:
        validateConfiguration({'version': 1, 'site.example': {'all': ["chrome"]}})
    except Exception as e:
        raise e


def main():
    parser = argparse.ArgumentParser(description='Generate UserAgentOverride.h')
    parser.add_argument('--input')
    parser.add_argument('--test', action='store_const', const=True)
    args = parser.parse_args()

    if args.input and args.test:
        raise Exception("Both input file and testing were specified. Only specify one of them.")

    if args.input:
        generateFiles(args.input)
        return

    if args.test:
        runTests()
        return

    raise Exception("No action specified. Specify either --input or --test")


if __name__ == '__main__':
    main()
