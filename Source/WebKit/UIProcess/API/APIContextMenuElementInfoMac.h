/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(MAC)

#include "APIObject.h"
#include "ContextMenuContextData.h"
#include "WebHitTestResultData.h"
#include <wtf/WeakPtr.h>

namespace WebKit {
class WebPageProxy;
}

namespace API {

class ContextMenuElementInfoMac final : public ObjectImpl<Object::Type::ContextMenuElementInfoMac> {
public:
    template<typename... Args> static Ref<ContextMenuElementInfoMac> create(Args&&... args)
    {
        return adoptRef(*new ContextMenuElementInfoMac(std::forward<Args>(args)...));
    }

    const WebKit::WebHitTestResultData& hitTestResultData() const { return m_hitTestResultData; }
    WebKit::WebPageProxy* page() { return m_page.get(); }
    const WTF::String& qrCodePayloadString() const { return m_qrCodePayloadString; }
    bool hasEntireImage() const { return m_hasEntireImage; }

private:
    ContextMenuElementInfoMac(const WebKit::ContextMenuContextData& data, WebKit::WebPageProxy& page)
        : m_hitTestResultData(data.webHitTestResultData().value())
        , m_page(page)
        , m_qrCodePayloadString(data.qrCodePayloadString())
        , m_hasEntireImage(data.hasEntireImage()) { }

    WebKit::WebHitTestResultData m_hitTestResultData;
    WeakPtr<WebKit::WebPageProxy> m_page;
    WTF::String m_qrCodePayloadString;
    bool m_hasEntireImage { false };
};

} // namespace API

#endif // PLATFORM(MAC)
