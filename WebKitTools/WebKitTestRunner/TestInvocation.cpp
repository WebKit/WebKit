/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "TestInvocation.h"

#include "PlatformWebView.h"
#include "TestController.h"
#include <WebKit2/WKContextPrivate.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WKStringCF.h>
#include <WebKit2/WKURLCF.h>
#include <wtf/Vector.h>
#include <wtf/RetainPtr.h>

using namespace WebKit;

namespace WTR {

static WKURLRef createWKURL(const char* pathOrURL)
{
    RetainPtr<CFStringRef> pathOrURLCFString(AdoptCF, CFStringCreateWithCString(0, pathOrURL, kCFStringEncodingUTF8));
    RetainPtr<CFURLRef> cfURL;
    if (CFStringHasPrefix(pathOrURLCFString.get(), CFSTR("http://")) || CFStringHasPrefix(pathOrURLCFString.get(), CFSTR("https://")))
        cfURL.adoptCF(CFURLCreateWithString(0, pathOrURLCFString.get(), 0));
    else
        cfURL.adoptCF(CFURLCreateWithFileSystemPath(0, pathOrURLCFString.get(), kCFURLPOSIXPathStyle, false));
    return WKURLCreateWithCFURL(cfURL.get());
}

static std::auto_ptr<Vector<char> > WKStringToUTF8(WKStringRef wkStringRef)
{
    RetainPtr<CFStringRef> cfString(AdoptCF, WKStringCopyCFString(0, wkStringRef));
    CFIndex bufferLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfString.get()), kCFStringEncodingUTF8) + 1;
    std::auto_ptr<Vector<char> > buffer(new Vector<char>(bufferLength));
    if (!CFStringGetCString(cfString.get(), buffer->data(), bufferLength, kCFStringEncodingUTF8)) {
        buffer->shrink(1);
        (*buffer)[0] = 0;
    } else
        buffer->shrink(strlen(buffer->data()) + 1);
    return buffer;
}

TestInvocation::TestInvocation(const char* pathOrURL)
    : m_url(AdoptWK, createWKURL(pathOrURL))
    , m_gotInitialResponse(false)
    , m_gotFinalMessage(false)
    , m_error(false)
{
}

TestInvocation::~TestInvocation()
{
}

void TestInvocation::invoke()
{
    WKRetainPtr<WKStringRef> message(AdoptWK, WKStringCreateWithCFString(CFSTR("BeginTest")));
    WKContextPostMessageToInjectedBundle(TestController::shared().context(), message.get());

    runUntil(m_gotInitialResponse);
    if (m_error) {
        dump("FAIL\n");
        return;
    }

    WKPageLoadURL(TestController::shared().mainWebView()->page(), m_url.get());

    runUntil(m_gotFinalMessage);
    if (m_error) {
        dump("FAIL\n");
        return;
    }
}

void TestInvocation::dump(const char* stringToDump)
{
    printf("Content-Type: text/plain\n");
    printf("%s", stringToDump);

    fputs("#EOF\n", stdout);
    fputs("#EOF\n", stdout);
    fputs("#EOF\n", stderr);

    fflush(stdout);
    fflush(stderr);
}

void TestInvocation::didRecieveMessageFromInjectedBundle(WKStringRef message)
{
    RetainPtr<CFStringRef> cfMessage(AdoptCF, WKStringCopyCFString(0, message));
    
    if (CFEqual(cfMessage.get(), CFSTR("Error"))) {
        // Set all states to true to stop spinning the runloop.
        m_gotInitialResponse = true;
        m_gotFinalMessage = true;
        m_error = true;
        return;
    }

    if (CFEqual(cfMessage.get(), CFSTR("BeginTestAck"))) {
        m_gotInitialResponse = true;
        return;
    }

    std::auto_ptr<Vector<char> > utf8Message = WKStringToUTF8(message);

    dump(utf8Message->data());
    m_gotFinalMessage = true;
}

} // namespace WTR
