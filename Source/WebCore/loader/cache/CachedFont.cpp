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
#include "CachedFont.h"

#include "CachedFontClient.h"
#include "CachedResourceClientWalker.h"
#include "CachedResourceLoader.h"
#include "FontCustomPlatformData.h"
#include "FontPlatformData.h"
#include "MemoryCache.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"
#include "TypedElementDescendantIterator.h"
#include "WOFFFileFormat.h"
#include <wtf/Vector.h>

#if ENABLE(SVG_FONTS)
#include "NodeList.h"
#include "SVGDocument.h"
#include "SVGElement.h"
#include "SVGFontElement.h"
#include "SVGGElement.h"
#include "SVGNames.h"
#endif

namespace WebCore {

CachedFont::CachedFont(const ResourceRequest& resourceRequest, SessionID sessionID)
    : CachedResource(resourceRequest, FontResource, sessionID)
    , m_loadInitiated(false)
    , m_hasCreatedFontDataWrappingResource(false)
{
}

CachedFont::~CachedFont()
{
}

void CachedFont::load(CachedResourceLoader*, const ResourceLoaderOptions& options)
{
    // Don't load the file yet.  Wait for an access before triggering the load.
    setLoading(true);
    m_options = options;
}

void CachedFont::didAddClient(CachedResourceClient* c)
{
    ASSERT(c->resourceClientType() == CachedFontClient::expectedType());
    if (!isLoading())
        static_cast<CachedFontClient*>(c)->fontLoaded(this);
}

void CachedFont::finishLoading(SharedBuffer* data)
{
    m_data = data;
    setEncodedSize(m_data.get() ? m_data->size() : 0);
    setLoading(false);
    checkNotify();
}

void CachedFont::beginLoadIfNeeded(CachedResourceLoader* dl)
{
    if (!m_loadInitiated) {
        m_loadInitiated = true;
        CachedResource::load(dl, m_options);
    }
}

bool CachedFont::ensureCustomFontData()
{
    if (!m_fontData && !errorOccurred() && !isLoading() && m_data) {
        RefPtr<SharedBuffer> buffer = m_data;
        bool fontIsWOFF = false;

#if (!PLATFORM(MAC) || __MAC_OS_X_VERSION_MIN_REQUIRED <= 1090) && (!PLATFORM(IOS) || __IPHONE_OS_VERSION_MIN_REQUIRED < 80000)
        if (isWOFF(buffer.get())) {
            Vector<char> convertedFont;
            if (!convertWOFFToSfnt(buffer.get(), convertedFont))
                buffer = nullptr;
            else {
                buffer = SharedBuffer::adoptVector(convertedFont);
                fontIsWOFF = true;
            }
        }
#endif

        m_fontData = buffer ? createFontCustomPlatformData(*buffer) : nullptr;
        m_hasCreatedFontDataWrappingResource = m_fontData && !fontIsWOFF;
        if (!m_fontData)
            setStatus(DecodeError);
    }

    return m_fontData.get();
}

FontPlatformData CachedFont::platformDataFromCustomData(float size, bool bold, bool italic, FontOrientation orientation, FontWidthVariant widthVariant, FontRenderingMode renderingMode)
{
#if ENABLE(SVG_FONTS)
    if (m_externalSVGDocument)
        return FontPlatformData(size, bold, italic);
#endif
    ASSERT(m_fontData);
    return m_fontData->fontPlatformData(static_cast<int>(size), bold, italic, orientation, widthVariant, renderingMode);
}

#if ENABLE(SVG_FONTS)

bool CachedFont::ensureSVGFontData()
{
    if (!m_externalSVGDocument && !errorOccurred() && !isLoading() && m_data) {
        m_externalSVGDocument = SVGDocument::create(nullptr, URL());
        RefPtr<TextResourceDecoder> decoder = TextResourceDecoder::create("application/xml");
        m_externalSVGDocument->setContent(decoder->decodeAndFlush(m_data->data(), m_data->size()));
        if (decoder->sawError())
            m_externalSVGDocument = nullptr;
    }
    return m_externalSVGDocument;
}

SVGFontElement* CachedFont::getSVGFontById(const String& fontName) const
{
    auto elements = descendantsOfType<SVGFontElement>(*m_externalSVGDocument);

    if (fontName.isEmpty())
        return elements.first();

    for (auto& element : elements) {
        if (element.getIdAttribute() == fontName)
            return &element;
    }
    return nullptr;
}

#endif

void CachedFont::allClientsRemoved()
{
    m_fontData = nullptr;
}

void CachedFont::checkNotify()
{
    if (isLoading())
        return;
    
    CachedResourceClientWalker<CachedFontClient> w(m_clients);
    while (CachedFontClient* c = w.next())
         c->fontLoaded(this);
}

bool CachedFont::mayTryReplaceEncodedData() const
{
    // If a FontCustomPlatformData has ever been constructed to wrap the internal resource buffer then it still might be in use somewhere.
    // That platform font object might directly reference the encoded data buffer behind this CachedFont,
    // so replacing it is unsafe.

    return !m_hasCreatedFontDataWrappingResource;
}

}
