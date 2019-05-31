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

#ifndef APIError_h
#define APIError_h

#include "APIObject.h"
#include <WebCore/ResourceError.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace API {

class Error : public ObjectImpl<Object::Type::Error> {
public:
    static Ref<Error> create()
    {
        return adoptRef(*new Error);
    }

    static Ref<Error> create(const WebCore::ResourceError& error)
    {
        return adoptRef(*new Error(error));
    }

    enum General {
        Internal = 300
    };
    static const WTF::String& webKitErrorDomain();

    enum Network {
        Cancelled = 302,
        FileDoesNotExist = 303
    };
    static const WTF::String& webKitNetworkErrorDomain();

    enum Policy {
        CannotShowMIMEType = 100,
        CannotShowURL = 101,
        FrameLoadInterruptedByPolicyChange = 102,
        CannotUseRestrictedPort = 103,
        FrameLoadBlockedByContentBlocker = 104,
        FrameLoadBlockedByContentFilter = 105,
        FrameLoadBlockedByRestrictions = 106,
    };
    static const WTF::String& webKitPolicyErrorDomain();

    enum Plugin {
        CannotFindPlugIn = 200,
        CannotLoadPlugIn = 201,
        JavaUnavailable = 202,
        PlugInCancelledConnection = 203,
        PlugInWillHandleLoad = 204,
        InsecurePlugInVersion = 205
    };
    static const WTF::String& webKitPluginErrorDomain();

#if USE(SOUP)
    enum Download {
        Transport = 499,
        CancelledByUser = 400,
        Destination = 401
    };
    static const WTF::String& webKitDownloadErrorDomain();
#endif

#if PLATFORM(GTK)
    enum Print {
        Generic = 599,
        PrinterNotFound = 500,
        InvalidPageRange = 501
    };
    static const WTF::String& webKitPrintErrorDomain();
#endif

    const WTF::String& domain() const { return m_platformError.domain(); }
    int errorCode() const { return m_platformError.errorCode(); }
    const WTF::String& failingURL() const { return m_platformError.failingURL(); }
    const WTF::String& localizedDescription() const { return m_platformError.localizedDescription(); }

    const WebCore::ResourceError& platformError() const { return m_platformError; }

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, RefPtr<Object>&);

private:
    Error()
    {
    }

    Error(const WebCore::ResourceError& error)
        : m_platformError(error)
    {
    }

    WebCore::ResourceError m_platformError;
};

} // namespace API

#endif // APIError_h
