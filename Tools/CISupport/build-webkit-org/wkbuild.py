# Copyright (C) 2010 Apple Inc. All rights reserved.
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

"""Functions relating to building WebKit"""

import re


def _should_file_trigger_build(target_platform, file):
    # The directories and patterns lists below map directory names or
    # regexp patterns to the bot platforms for which they should trigger a
    # build. Mapping to the empty list means that no builds should be
    # triggered on any platforms. Earlier directories/patterns take
    # precendence over later ones.

    # FIXME: The patterns below have only been verified to be correct on
    # the platforms listed below. We should implement this for other platforms
    # and start using it for their bots. Someone familiar with each platform
    # will have to figure out what the right set of directories/patterns is for
    # that platform.
    assert(target_platform in (
        "mac-yosemite",
        "mac-elcapitan",
        "mac-sierra",
        "mac-highsierra",
        "mac-mojave",
        "mac-catalina",
        "mac-bigsur",
        "mac-monterey",
        "win",
        "ios-15",
        "ios-simulator-15",
        "tvos-15",
        "tvos-simulator-15",
        "watchos-8",
        "watchos-simulator-8",
    ))

    directories = [
        # Directories that shouldn't trigger builds on any bots.
        ("Examples", []),
        ("PerformanceTests", []),
        ("ManualTests", []),
        ("Tools/CISupport/build-webkit-org/public_html", []),
        ("Websites", []),
        ("opengl", []),
        ("opentype", []),
        ("openvg", []),
        ("wx", []),

        # Directories that should trigger builds on only some bots.
        ("LayoutTests/platform/ios-simulator", ["ios"]),
        ("LayoutTests/platform/ios-simulator-wk1", ["ios"]),
        ("LayoutTests/platform/ios-simulator-wk2", ["ios"]),
        ("LayoutTests/platform/mac-yosemite", ["mac-yosemite"]),
        ("LayoutTests/platform/mac-elcapitan", ["mac-yosemite", "mac-elcapitan"]),
        ("LayoutTests/platform/mac-sierra", ["mac-yosemite", "mac-elcapitan", "mac-sierra"]),
        ("LayoutTests/platform/mac-highsierra", ["mac-yosemite", "mac-elcapitan", "mac-sierra", "mac-highsierra"]),
        ("LayoutTests/platform/mac-mojave", ["mac-yosemite", "mac-elcapitan", "mac-sierra", "mac-highsierra", "mac-mojave"]),
        ("LayoutTests/platform/mac-catalina", ["mac-yosemite", "mac-elcapitan", "mac-sierra", "mac-highsierra", "mac-mojave", "mac-bigsur"]),
        ("LayoutTests/platform/mac-bigsur", ["mac-catalina", "mac-bigsur"]),
        ("LayoutTests/platform/mac-monterey", ["mac-catalina", "mac-bigsur", "mac-monterey"]),
        ("LayoutTests/platform/mac-wk2", ["mac"]),
        ("LayoutTests/platform/mac-wk1", ["mac"]),
        ("LayoutTests/platform/mac", ["mac", "win"]),
        ("LayoutTests/platform/wk2", ["mac", "ios"]),
        ("cairo", ["gtk", "wincairo"]),
        ("cf", ["mac", "win", "ios", "tvos", "watchos"]),
        ("cocoa", ["mac", "ios", "tvos", "watchos"]),
        ("curl", ["gtk", "wincairo"]),
        ("gobject", ["gtk"]),
        ("gstreamer", ["gtk"]),
        ("gtk", ["gtk"]),
        ("ios", ["ios", "tvos", "watchos"]),
        ("mac", ["mac"]),
        ("objc", ["mac", "ios", "tvos", "watchos"]),
        ("soup", ["gtk"]),
        ("win", ["win"]),
    ]
    patterns = [
        # Patterns that shouldn't trigger builds on any bots.
        (r"(?:^|/)ChangeLog.*$", []),
        (r"(?:^|/)Makefile$", []),
        (r"/ARM", []),
        (r"/CMake.*", []),
        (r"/LICENSE[^/]+$", []),
        (r"ARM(?:v7)?\.(?:cpp|h)$", []),
        (r"MIPS\.(?:cpp|h)$", []),
        (r"\.(?:bkl|mk)$", []),

        # Patterns that should trigger builds on only some bots.
        (r"(?:^|/)PlatformGTK\.cmake$", ["gtk"]),
        (r"Mac\.(?:cpp|h|mm)$", ["mac"]),
        (r"IOS\.(?:cpp|h|mm)$", ["ios", "tvos", "watchos"]),
        (r"\.(?:vcproj|vsprops|sln|vcxproj|props|filters)$", ["win"]),
        (r"\.exp(?:\.in)?$", ["mac", "ios", "tvos", "watchos"]),
        (r"\.order$", ["mac", "ios", "tvos", "watchos"]),
        (r"\.(?:vcproj|vcxproj)/", ["win"]),
        (r"\.xcconfig$", ["mac", "ios", "tvos", "watchos"]),
        (r"\.xcodeproj/", ["mac", "ios", "tvos", "watchos"]),
    ]

    base_platform = target_platform.split("-")[0]

    # See if the file is in one of the known directories.
    for directory, platforms in directories:
        if re.search(r"(?:^|/)%s/" % directory, file):
            return target_platform in platforms or base_platform in platforms

    # See if the file matches a known pattern.
    for pattern, platforms in patterns:
        if re.search(pattern, file):
            return target_platform in platforms or base_platform in platforms

    # See if the file is a platform-specific test result.
    match = re.match("LayoutTests/platform/(?P<platform>[^/]+)/", file)
    if match:
        # See if the file is a test result for this platform, our base
        # platform, or one of our sub-platforms.
        return match.group("platform") in (target_platform, base_platform) or match.group("platform").startswith("%s-" % target_platform)

    # The file isn't one we know about specifically, so we should assume we
    # have to build.
    return True


def should_build(target_platform, changed_files):
    """Returns true if the changed files affect the given platform, and
    thus a build should be performed. target_platform should be one of the
    platforms used in the build.webkit.org master's config.json file."""
    return any(_should_file_trigger_build(target_platform, file) for file in changed_files)
