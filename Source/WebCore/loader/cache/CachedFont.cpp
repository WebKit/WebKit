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
#include "FontDescription.h"
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

CachedFont::CachedFont(const ResourceRequest& resourceRequest, SessionID sessionID, Type type)
    : CachedResource(resourceRequest, type, sessionID)
    , m_loadInitiated(false)
    , m_hasCreatedFontDataWrappingResource(false)
{
}

CachedFont::~CachedFont()
{
}

void CachedFont::load(CachedResourceLoader&, const ResourceLoaderOptions& options)
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

void CachedFont::beginLoadIfNeeded(CachedResourceLoader& loader)
{
    if (!m_loadInitiated) {
        m_loadInitiated = true;
        CachedResource::load(loader, m_options);
    }
}

bool CachedFont::ensureCustomFontData(const AtomicString&)
{
    return ensureCustomFontData(m_data.get());
}

bool CachedFont::ensureCustomFontData(SharedBuffer* data)
{
    if (!m_fontCustomPlatformData && !errorOccurred() && !isLoading() && data) {
        RefPtr<SharedBuffer> buffer(data);

#if !PLATFORM(COCOA)
        if (isWOFF(buffer.get())) {
            Vector<char> convertedFont;
            if (!convertWOFFToSfnt(buffer.get(), convertedFont))
                buffer = nullptr;
            else
                buffer = SharedBuffer::adoptVector(convertedFont);
        }
#endif

        m_fontCustomPlatformData = buffer ? createFontCustomPlatformData(*buffer) : nullptr;
        m_hasCreatedFontDataWrappingResource = m_fontCustomPlatformData && (buffer == m_data);
        if (!m_fontCustomPlatformData)
            setStatus(DecodeError);
    }

    return m_fontCustomPlatformData.get();
}

RefPtr<Font> CachedFont::createFont(const FontDescription& fontDescription, const AtomicString&, bool syntheticBold, bool syntheticItalic, const FontFeatureSettings& fontFaceFeatures, const FontVariantSettings& fontFaceVariantSettings)
{
    return Font::create(platformDataFromCustomData(fontDescription, syntheticBold, syntheticItalic, fontFaceFeatures, fontFaceVariantSettings), true, false);
}

FontPlatformData CachedFont::platformDataFromCustomData(const FontDescription& fontDescription, bool bold, bool italic, const FontFeatureSettings& fontFaceFeatures, const FontVariantSettings& fontFaceVariantSettings)
{
    ASSERT(m_fontCustomPlatformData);
#if PLATFORM(COCOA)
    return m_fontCustomPlatformData->fontPlatformData(fontDescription, bold, italic, fontFaceFeatures, fontFaceVariantSettings);
#else
    UNUSED_PARAM(fontFaceFeatures);
    UNUSED_PARAM(fontFaceVariantSettings);
    return m_fontCustomPlatformData->fontPlatformData(fontDescription, bold, italic);
#endif
}

void CachedFont::allClientsRemoved()
{
    m_fontCustomPlatformData = nullptr;
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
