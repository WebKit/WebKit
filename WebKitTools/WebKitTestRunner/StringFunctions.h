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

#ifndef StringFunctions_h
#define StringFunctions_h

#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JavaScriptCore.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WKString.h>
#include <WebKit2/WKStringCF.h>
#include <WebKit2/WKURL.h>
#include <WebKit2/WKURLCF.h>
#include <sstream>
#include <wtf/Platform.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

namespace WTR {

// Conversion functions

inline RetainPtr<CFStringRef> toCF(JSStringRef string)
{
    return RetainPtr<CFStringRef>(AdoptCF, JSStringCopyCFString(0, string));
}

inline RetainPtr<CFStringRef> toCF(WKStringRef string)
{
    return RetainPtr<CFStringRef>(AdoptCF, WKStringCopyCFString(0, string));
}

inline WKRetainPtr<WKStringRef> toWK(JSStringRef string)
{
    return WKRetainPtr<WKStringRef>(AdoptWK, WKStringCreateWithCFString(toCF(string).get()));
}

inline WKRetainPtr<WKStringRef> toWK(JSRetainPtr<JSStringRef> string)
{
    return WKRetainPtr<WKStringRef>(AdoptWK, WKStringCreateWithCFString(toCF(string.get()).get()));
}

inline JSRetainPtr<JSStringRef> toJS(WKStringRef string)
{
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithCFString(toCF(string).get()));
}

inline JSRetainPtr<JSStringRef> toJS(const WKRetainPtr<WKStringRef>& string)
{
    return toJS(string.get());
}

// Streaming functions

inline std::ostream& operator<<(std::ostream& out, CFStringRef stringRef)
{
    if (!stringRef)
        return out;
    CFIndex bufferLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(stringRef), kCFStringEncodingUTF8) + 1;
    Vector<char> buffer(bufferLength);
    if (!CFStringGetCString(stringRef, buffer.data(), bufferLength, kCFStringEncodingUTF8))
        return out;
    return out << buffer.data();
}

inline std::ostream& operator<<(std::ostream& out, const RetainPtr<CFStringRef>& stringRef)
{
    return out << stringRef.get();
}

inline std::ostream& operator<<(std::ostream& out, WKStringRef stringRef)
{
    if (!stringRef)
        return out;
    return out << toCF(stringRef);
}

inline std::ostream& operator<<(std::ostream& out, const WKRetainPtr<WKStringRef>& stringRef)
{
    return out << stringRef.get();
}

// URL creation

inline WKURLRef createWKURL(const char* pathOrURL)
{
    RetainPtr<CFStringRef> pathOrURLCFString(AdoptCF, CFStringCreateWithCString(0, pathOrURL, kCFStringEncodingUTF8));
    RetainPtr<CFURLRef> cfURL;
    if (CFStringHasPrefix(pathOrURLCFString.get(), CFSTR("http://")) || CFStringHasPrefix(pathOrURLCFString.get(), CFSTR("https://")))
        cfURL.adoptCF(CFURLCreateWithString(0, pathOrURLCFString.get(), 0));
    else
#if PLATFORM(WIN)
        cfURL.adoptCF(CFURLCreateWithFileSystemPath(0, pathOrURLCFString.get(), kCFURLWindowsPathStyle, false));
#else
        cfURL.adoptCF(CFURLCreateWithFileSystemPath(0, pathOrURLCFString.get(), kCFURLPOSIXPathStyle, false));
#endif
    return WKURLCreateWithCFURL(cfURL.get());
}


} // namespace WTR

#endif // StringFunctions_h
