/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
#import "CrashReporterInfo.h"

#import "StringFunctions.h"
#import <WebKit/WKURLCF.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/CrashReporter.h>
#import <wtf/text/MakeString.h>

namespace WTR {

static RefPtr<StringImpl> testPathFromURL(WKURLRef url)
{
    if (!url)
        return "(null)"_si;
    auto cfURL = adoptCF(WKURLCopyCFURL(kCFAllocatorDefault, url));
    if (!cfURL)
        return nullptr;

    auto schemeCFString = adoptCF(CFURLCopyScheme(cfURL.get()));
    auto pathCFString = adoptCF(CFURLCopyPath(cfURL.get()));

    RefPtr<StringImpl> schemeString(StringImpl::create(schemeCFString.get()));
    RefPtr<StringImpl> pathString(StringImpl::create(pathCFString.get()));
    
    if (equalLettersIgnoringASCIICase(schemeString, "file"_s)) {
        RefPtr<StringImpl> layoutTests("/LayoutTests/"_si);
        size_t layoutTestsOffset = find(pathString, layoutTests);
        if (layoutTestsOffset == notFound)
            return nullptr;

        return substring(pathString, layoutTestsOffset + length(layoutTests));
    }

    if (!equalLettersIgnoringASCIICase(schemeString, "http"_s) && !equalLettersIgnoringASCIICase(schemeString, "https"_s))
        return nullptr;

    auto hostCFString = adoptCF(CFURLCopyHostName(cfURL.get()));
    RefPtr<StringImpl> hostString(StringImpl::create(hostCFString.get()));
    if (equal(hostString, "127.0.0.1"_s))
        return pathString;

    return nullptr;
}

void setCrashReportApplicationSpecificInformationToURL(WKURLRef url)
{
    RefPtr<StringImpl> testPath = testPathFromURL(url);
    if (testPath) {
        auto message = makeString("CRASHING TEST: "_s, testPath);
        WTF::setCrashLogMessage(utf8(message));
    }
}

} // namespace WTR
