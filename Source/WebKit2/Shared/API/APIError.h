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
#include <wtf/PassRefPtr.h>

namespace IPC {
class ArgumentDecoder;
class ArgumentEncoder;
}

namespace API {

class Error : public ObjectImpl<Object::Type::Error> {
public:
    static PassRefPtr<Error> create()
    {
        return adoptRef(new Error);
    }

    static PassRefPtr<Error> create(const WebCore::ResourceError& error)
    {
        return adoptRef(new Error(error));
    }

    static const WTF::String& webKitErrorDomain();

    const WTF::String& domain() const { return m_platformError.domain(); }
    int errorCode() const { return m_platformError.errorCode(); }
    const WTF::String& failingURL() const { return m_platformError.failingURL(); }
    const WTF::String& localizedDescription() const { return m_platformError.localizedDescription(); }

    const WebCore::ResourceError& platformError() const { return m_platformError; }

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, RefPtr<Object>&);

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
