/*
 * Copyright (C) 2021 Metrological Group B.V.
 * Copyright (C) 2021 Igalia S.L.
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CachedFont.h"
#include "CachedFontClient.h"
#include "CachedResourceHandle.h"
#include "FontLoadRequest.h"
#include "FontSelectionAlgorithm.h"

namespace WebCore {

class FontCreationContext;

class CachedFontLoadRequest final : public FontLoadRequest, public CachedFontClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CachedFontLoadRequest(CachedFont& font)
        : m_font(&font)
    {
    }

    ~CachedFontLoadRequest()
    {
        if (m_fontLoadRequestClient)
            m_font->removeClient(*this);
    }

    CachedFont& cachedFont() const { return *m_font; }

private:
    const URL& url() const final { return m_font->url(); }
    bool isPending() const final { return m_font->status() == CachedResource::Status::Pending; }
    bool isLoading() const final { return m_font->isLoading(); }
    bool errorOccurred() const final { return m_font->errorOccurred(); }

    bool ensureCustomFontData() final { return m_font->ensureCustomFontData(); }
    RefPtr<Font> createFont(const FontDescription& description, bool syntheticBold, bool syntheticItalic, const FontCreationContext& fontCreationContext) final
    {
        return m_font->createFont(description, syntheticBold, syntheticItalic, fontCreationContext);
    }

    void setClient(FontLoadRequestClient* client) final
    {
        auto* oldClient = m_fontLoadRequestClient;
        m_fontLoadRequestClient = client;

        if (!client && oldClient)
            m_font->removeClient(*this);
        else if (client && !oldClient)
            m_font->addClient(*this);
    }

    bool isCachedFontLoadRequest() const final { return true; }

    void fontLoaded(CachedFont& font) final
    {
        ASSERT_UNUSED(font, &font == m_font.get());
        if (m_fontLoadRequestClient)
            m_fontLoadRequestClient->fontLoaded(*this);
    }

    CachedResourceHandle<CachedFont> m_font;
    FontLoadRequestClient* m_fontLoadRequestClient { nullptr };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_FONTLOADREQUEST(WebCore::CachedFontLoadRequest, isCachedFontLoadRequest())
