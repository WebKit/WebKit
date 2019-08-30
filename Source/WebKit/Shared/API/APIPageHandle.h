/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef APIPageHandle_h
#define APIPageHandle_h

#include "APIObject.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/PageIdentifier.h>
#include <wtf/Ref.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace API {

class PageHandle : public ObjectImpl<Object::Type::PageHandle> {
public:
    static Ref<PageHandle> create(WebKit::WebPageProxyIdentifier, WebCore::PageIdentifier);
    static Ref<PageHandle> createAutoconverting(WebKit::WebPageProxyIdentifier, WebCore::PageIdentifier);
    virtual ~PageHandle();

    WebKit::WebPageProxyIdentifier pageProxyID() const { return m_pageProxyID; }
    WebCore::PageIdentifier webPageID() const { return m_webPageID; }
    bool isAutoconverting() const { return m_isAutoconverting; }

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, RefPtr<Object>&);

private:
    PageHandle(WebKit::WebPageProxyIdentifier, WebCore::PageIdentifier, bool isAutoconverting);

    const WebKit::WebPageProxyIdentifier m_pageProxyID;
    const WebCore::PageIdentifier m_webPageID;
    const bool m_isAutoconverting;
};

} // namespace API

#endif // APIPageHandle_h
