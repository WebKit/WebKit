/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include <stdlib.h>
#include <wtf/Environment.h>

namespace TestWebKitAPI {

const char* const environmentVariable = "DYLD_INSERT_LIBRARIES";
#define PROCESS_DYLIB "Process.dylib"
const char* const stripValue = "/" PROCESS_DYLIB;

static const char* strip(const char* input)
{
    setenv(environmentVariable, input, 1);
    WebKit::EnvironmentUtilities::stripValuesEndingWithString(environmentVariable, stripValue);
    return Environment::getRaw(environmentVariable);
}

TEST(WebKit, StripValuesEndingWithString)
{
    EXPECT_STREQ(strip(""), "");
    EXPECT_STREQ(strip(":"), ":");
    EXPECT_STREQ(strip("::"), "::");
    EXPECT_STREQ(strip(":::"), ":::");
    EXPECT_STREQ(strip("::::"), "::::");
    EXPECT_STREQ(strip(":::::"), ":::::");

    EXPECT_STREQ(strip(PROCESS_DYLIB), PROCESS_DYLIB);
    EXPECT_STREQ(strip(":" PROCESS_DYLIB), ":" PROCESS_DYLIB);
    EXPECT_STREQ(strip(PROCESS_DYLIB ":"), PROCESS_DYLIB ":");
    EXPECT_STREQ(strip(":" PROCESS_DYLIB ":"), ":" PROCESS_DYLIB ":");

    EXPECT_STREQ(strip("/" PROCESS_DYLIB), nullptr);
    EXPECT_STREQ(strip(":/" PROCESS_DYLIB), nullptr);
    EXPECT_STREQ(strip("/" PROCESS_DYLIB ":"), nullptr);
    EXPECT_STREQ(strip(":/" PROCESS_DYLIB ":"), ":");

    EXPECT_STREQ(strip(PROCESS_DYLIB "/"), PROCESS_DYLIB "/");
    EXPECT_STREQ(strip(":" PROCESS_DYLIB "/"), ":" PROCESS_DYLIB "/");
    EXPECT_STREQ(strip(PROCESS_DYLIB "/:"), PROCESS_DYLIB "/:");
    EXPECT_STREQ(strip(":" PROCESS_DYLIB "/:"), ":" PROCESS_DYLIB "/:");

    EXPECT_STREQ(strip("/" PROCESS_DYLIB "/"), "/" PROCESS_DYLIB "/");
    EXPECT_STREQ(strip(":/" PROCESS_DYLIB "/"), ":/" PROCESS_DYLIB "/");
    EXPECT_STREQ(strip("/" PROCESS_DYLIB "/:"), "/" PROCESS_DYLIB "/:");
    EXPECT_STREQ(strip(":/" PROCESS_DYLIB "/:"), ":/" PROCESS_DYLIB "/:");

    EXPECT_STREQ(strip("/Before.dylib:/" PROCESS_DYLIB), "/Before.dylib");
    EXPECT_STREQ(strip("/" PROCESS_DYLIB ":/After.dylib"), "/After.dylib");
    EXPECT_STREQ(strip("/Before.dylib:/" PROCESS_DYLIB ":/After.dylib"), "/Before.dylib:/After.dylib");
    EXPECT_STREQ(strip("/Before.dylib:/" PROCESS_DYLIB ":/Middle.dylib:/" PROCESS_DYLIB ":/After.dylib"), "/Before.dylib:/Middle.dylib:/After.dylib");

    EXPECT_STREQ(strip("/" PROCESS_DYLIB ":/" PROCESS_DYLIB), nullptr);
    EXPECT_STREQ(strip("/" PROCESS_DYLIB ":/" PROCESS_DYLIB ":/" PROCESS_DYLIB), nullptr);

    EXPECT_STREQ(strip("/usr/lib/" PROCESS_DYLIB), nullptr);
    EXPECT_STREQ(strip("/" PROCESS_DYLIB "/" PROCESS_DYLIB), nullptr);
}

}
