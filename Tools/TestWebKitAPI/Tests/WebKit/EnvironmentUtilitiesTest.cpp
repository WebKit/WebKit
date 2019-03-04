/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
#include "Test.h"

#include <WebKit/EnvironmentUtilities.h>

namespace TestWebKitAPI {

#define PROCESS_DYLIB "Process.dylib"
const char* const stripValue = "/" PROCESS_DYLIB;

static void testStrip(const char* input, const char* expected)
{
    auto actual = WebKit::EnvironmentUtilities::stripEntriesEndingWith(input, stripValue);
    EXPECT_STREQ(actual.utf8().data(), expected);
}

TEST(WebKit, StripEntriesEndingWith)
{
    testStrip("", "");
    testStrip(":", ":");
    testStrip("::", "::");
    testStrip(":::", ":::");
    testStrip("::::", "::::");
    testStrip(":::::", ":::::");

    testStrip(PROCESS_DYLIB, PROCESS_DYLIB);
    testStrip(":" PROCESS_DYLIB, ":" PROCESS_DYLIB);
    testStrip(PROCESS_DYLIB ":", PROCESS_DYLIB ":");
    testStrip(":" PROCESS_DYLIB ":", ":" PROCESS_DYLIB ":");

    testStrip("/" PROCESS_DYLIB, "");
    testStrip(":/" PROCESS_DYLIB, "");
    testStrip("/" PROCESS_DYLIB ":", "");
    testStrip(":/" PROCESS_DYLIB ":", ":");

    testStrip(PROCESS_DYLIB "/", PROCESS_DYLIB "/");
    testStrip(":" PROCESS_DYLIB "/", ":" PROCESS_DYLIB "/");
    testStrip(PROCESS_DYLIB "/:", PROCESS_DYLIB "/:");
    testStrip(":" PROCESS_DYLIB "/:", ":" PROCESS_DYLIB "/:");

    testStrip("/" PROCESS_DYLIB "/", "/" PROCESS_DYLIB "/");
    testStrip(":/" PROCESS_DYLIB "/", ":/" PROCESS_DYLIB "/");
    testStrip("/" PROCESS_DYLIB "/:", "/" PROCESS_DYLIB "/:");
    testStrip(":/" PROCESS_DYLIB "/:", ":/" PROCESS_DYLIB "/:");

    testStrip("/Before.dylib:/" PROCESS_DYLIB, "/Before.dylib");
    testStrip("/" PROCESS_DYLIB ":/After.dylib", "/After.dylib");
    testStrip("/Before.dylib:/" PROCESS_DYLIB ":/After.dylib", "/Before.dylib:/After.dylib");
    testStrip("/Before.dylib:/" PROCESS_DYLIB ":/Middle.dylib:/" PROCESS_DYLIB ":/After.dylib", "/Before.dylib:/Middle.dylib:/After.dylib");

    testStrip("/" PROCESS_DYLIB ":/" PROCESS_DYLIB, "");
    testStrip("/" PROCESS_DYLIB ":/" PROCESS_DYLIB ":/" PROCESS_DYLIB, "");

    testStrip("/usr/lib/" PROCESS_DYLIB, "");
    testStrip("/" PROCESS_DYLIB "/" PROCESS_DYLIB, "");
}

}
