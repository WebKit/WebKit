/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CachedSVGFont.h"

#include "FontDescription.h"
#include "FontPlatformData.h"
#include "SVGDocument.h"
#include "SVGFontElement.h"
#include "SVGFontFaceElement.h"
#include "SVGToOTFFontConversion.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"
#include "TypedElementDescendantIterator.h"

namespace WebCore {

CachedSVGFont::CachedSVGFont(CachedResourceRequest&& request, PAL::SessionID sessionID, const CookieJar* cookieJar, const Settings& settings)
    : CachedFont(WTFMove(request), sessionID, cookieJar, Type::SVGFontResource)
    , m_externalSVGFontElement(nullptr)
    , m_settings(settings)
{
}

CachedSVGFont::CachedSVGFont(CachedResourceRequest&& request, CachedSVGFont& resource)
    : CachedSVGFont(WTFMove(request), resource.sessionID(), resource.cookieJar(), resource.m_settings)
{
}

RefPtr<Font> CachedSVGFont::createFont(const FontDescription& fontDescription, const AtomString& remoteURI, bool syntheticBold, bool syntheticItalic, const FontFeatureSettings& fontFaceFeatures, FontSelectionSpecifiedCapabilities fontFaceCapabilities)
{
    ASSERT(firstFontFace(remoteURI));
    return CachedFont::createFont(fontDescription, remoteURI, syntheticBold, syntheticItalic, fontFaceFeatures, fontFaceCapabilities);
}

FontPlatformData CachedSVGFont::platformDataFromCustomData(const FontDescription& fontDescription, bool bold, bool italic, const FontFeatureSettings& fontFaceFeatures, FontSelectionSpecifiedCapabilities fontFaceCapabilities)
{
    if (m_externalSVGDocument)
        return FontPlatformData(fontDescription.computedPixelSize(), bold, italic);
    return CachedFont::platformDataFromCustomData(fontDescription, bold, italic, fontFaceFeatures, fontFaceCapabilities);
}

bool CachedSVGFont::ensureCustomFontData(const AtomString& remoteURI)
{
    if (!m_externalSVGDocument && !errorOccurred() && !isLoading() && m_data) {
        bool sawError = false;
        {
            // We may get here during render tree updates when events are forbidden.
            // Frameless document can't run scripts or call back to the client so this is safe.
            m_externalSVGDocument = SVGDocument::create(nullptr, m_settings, URL());
            auto decoder = TextResourceDecoder::create("application/xml");

            ScriptDisallowedScope::DisableAssertionsInScope disabledScope;

            m_externalSVGDocument->setContent(decoder->decodeAndFlush(m_data->data(), m_data->size()));
            sawError = decoder->sawError();
        }

        if (sawError)
            m_externalSVGDocument = nullptr;
        if (m_externalSVGDocument)
            maybeInitializeExternalSVGFontElement(remoteURI);
        if (!m_externalSVGFontElement || !firstFontFace(remoteURI))
            return false;
        if (auto convertedFont = convertSVGToOTFFont(*m_externalSVGFontElement))
            m_convertedFont = SharedBuffer::create(WTFMove(convertedFont.value()));
        else {
            m_externalSVGDocument = nullptr;
            m_externalSVGFontElement = nullptr;
            return false;
        }
    }

    return m_externalSVGDocument && CachedFont::ensureCustomFontData(m_convertedFont.get());
}

SVGFontElement* CachedSVGFont::getSVGFontById(const String& fontName) const
{
    ASSERT(m_externalSVGDocument);
    auto elements = descendantsOfType<SVGFontElement>(*m_externalSVGDocument);

    if (fontName.isEmpty())
        return elements.first();

    for (auto& element : elements) {
        if (element.getIdAttribute() == fontName)
            return &element;
    }
    return nullptr;
}

SVGFontElement* CachedSVGFont::maybeInitializeExternalSVGFontElement(const AtomString& remoteURI)
{
    if (m_externalSVGFontElement)
        return m_externalSVGFontElement;
    String fragmentIdentifier;
    size_t start = remoteURI.find('#');
    if (start != notFound)
        fragmentIdentifier = remoteURI.string().substring(start + 1);
    m_externalSVGFontElement = getSVGFontById(fragmentIdentifier);
    return m_externalSVGFontElement;
}

SVGFontFaceElement* CachedSVGFont::firstFontFace(const AtomString& remoteURI)
{
    if (!maybeInitializeExternalSVGFontElement(remoteURI))
        return nullptr;

    if (auto* firstFontFace = childrenOfType<SVGFontFaceElement>(*m_externalSVGFontElement).first())
        return firstFontFace;
    return nullptr;
}

}
