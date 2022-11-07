/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
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
#include "FontCreationContext.h"
#include "FontCustomPlatformData.h"
#include "FontDescription.h"
#include "FontPlatformData.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"
#include "TypedElementDescendantIterator.h"
#include "WOFFFileFormat.h"
#include <wtf/Vector.h>

namespace WebCore {

CachedFont::CachedFont(CachedResourceRequest&& request, PAL::SessionID sessionID, const CookieJar* cookieJar, Type type)
    : CachedResource(WTFMove(request), type, sessionID, cookieJar)
    , m_loadInitiated(false)
    , m_hasCreatedFontDataWrappingResource(false)
{
}

CachedFont::~CachedFont() = default;

void CachedFont::load(CachedResourceLoader&)
{
    // Don't load the file yet.  Wait for an access before triggering the load.
    setLoading(true);
}

void CachedFont::didAddClient(CachedResourceClient& client)
{
    ASSERT(client.resourceClientType() == CachedFontClient::expectedType());
    if (!isLoading())
        downcast<CachedFontClient>(client).fontLoaded(*this);
}

void CachedFont::finishLoading(const FragmentedSharedBuffer* data, const NetworkLoadMetrics& metrics)
{
    if (data) {
        m_data = data->makeContiguous();
        setEncodedSize(m_data->size());
    } else {
        m_data = nullptr;
        setEncodedSize(0);
    }
    setLoading(false);
    checkNotify(metrics);
}

void CachedFont::beginLoadIfNeeded(CachedResourceLoader& loader)
{
    if (!m_loadInitiated) {
        m_loadInitiated = true;
        CachedResource::load(loader);
    }
}

bool CachedFont::ensureCustomFontData()
{
    if (!m_data)
        return ensureCustomFontData(nullptr);
    if (!m_data->isContiguous())
        m_data = m_data->makeContiguous();
    return ensureCustomFontData(downcast<SharedBuffer>(m_data.get()));
}

String CachedFont::calculateItemInCollection() const
{
    return url().fragmentIdentifier().toString();
}

bool CachedFont::ensureCustomFontData(SharedBuffer* data)
{
    if (!m_fontCustomPlatformData && !errorOccurred() && !isLoading() && data) {
        bool wrapping;
        m_fontCustomPlatformData = createCustomFontData(*data, calculateItemInCollection(), wrapping);
        m_hasCreatedFontDataWrappingResource = m_fontCustomPlatformData && wrapping;
        if (!m_fontCustomPlatformData)
            setStatus(DecodeError);
    }

    return m_fontCustomPlatformData.get();
}

std::unique_ptr<FontCustomPlatformData> CachedFont::createCustomFontData(SharedBuffer& bytes, const String& itemInCollection, bool& wrapping)
{
    RefPtr buffer = { &bytes };
    wrapping = !convertWOFFToSfntIfNecessary(buffer);
    return buffer ? createFontCustomPlatformData(*buffer, itemInCollection) : nullptr;
}

RefPtr<Font> CachedFont::createFont(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic, const FontCreationContext& fontCreationContext)
{
    return Font::create(platformDataFromCustomData(fontDescription, syntheticBold, syntheticItalic, fontCreationContext), Font::Origin::Remote);
}

FontPlatformData CachedFont::platformDataFromCustomData(const FontDescription& fontDescription, bool bold, bool italic, const FontCreationContext& fontCreationContext)
{
    ASSERT(m_fontCustomPlatformData);
    return platformDataFromCustomData(*m_fontCustomPlatformData, fontDescription, bold, italic, fontCreationContext);
}

FontPlatformData CachedFont::platformDataFromCustomData(FontCustomPlatformData& fontCustomPlatformData, const FontDescription& fontDescription, bool bold, bool italic, const FontCreationContext& fontCreationContext)
{
    return fontCustomPlatformData.fontPlatformData(fontDescription, bold, italic, fontCreationContext);
}

void CachedFont::allClientsRemoved()
{
    m_fontCustomPlatformData = nullptr;
}

void CachedFont::checkNotify(const NetworkLoadMetrics&)
{
    if (isLoading())
        return;

    CachedResourceClientWalker<CachedFontClient> walker(*this);
    while (CachedFontClient* client = walker.next())
        client->fontLoaded(*this);
}

bool CachedFont::mayTryReplaceEncodedData() const
{
    // If a FontCustomPlatformData has ever been constructed to wrap the internal resource buffer then it still might be in use somewhere.
    // That platform font object might directly reference the encoded data buffer behind this CachedFont,
    // so replacing it is unsafe.

    return !m_hasCreatedFontDataWrappingResource;
}

}
