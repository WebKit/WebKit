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

#import "config.h"
#import "WKCrashReporter.h"

#import <cstdlib>
#import "CrashReporterClientSPI.h"

// Avoid having to link with libCrashReporterClient.a
#if (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000) || (PLATFORM(TVOS) && __TV_OS_VERSION_MIN_REQUIRED >= 130000)
CRASH_REPORTER_CLIENT_HIDDEN
struct crashreporter_annotations_t gCRAnnotations
__attribute__((section("__DATA," CRASHREPORTER_ANNOTATIONS_SECTION)))
    = { {CRASHREPORTER_ANNOTATIONS_VERSION}, {0}, {0}, {0}, {0}, {0}, {0}, {0} };
#else
CRASH_REPORTER_CLIENT_HIDDEN
struct crashreporter_annotations_t gCRAnnotations
__attribute__((section("__DATA," CRASHREPORTER_ANNOTATIONS_SECTION)))
    = { CRASHREPORTER_ANNOTATIONS_VERSION, 0, 0, 0, 0, 0, 0, 0 };
#endif

namespace WebKit {

static void setCrashLogMessage(const char* message)
{
    // We have to copy the string because CRSetCrashLogMessage doesn't.
    char* copiedMessage = message ? strdup(message) : nullptr;

    CRSetCrashLogMessage(copiedMessage);

    // Delete the message from last time, so we don't keep leaking messages.
    static char* previousCopiedCrashLogMessage;
    std::free(std::exchange(previousCopiedCrashLogMessage, copiedMessage));
}

void setCrashReportApplicationSpecificInformation(CFStringRef infoString)
{
    setCrashLogMessage([(__bridge NSString *)infoString UTF8String]);
}

}
