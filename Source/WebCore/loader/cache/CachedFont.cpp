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
#include "Logging.h"
#include "MemoryCache.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include "TextResourceDecoder.h"
#include "TrustedFonts.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "WOFFFileFormat.h"
#include <pal/crypto/CryptoDigest.h>
#include <wtf/Vector.h>
#include <wtf/text/Base64.h>

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


FontParsingPolicy CachedFont::policyForCustomFont(const Ref<SharedBuffer>& data)
{
    if (!m_loader || !m_loader->frame())
        return FontParsingPolicy::Deny;

    return fontBinaryParsingPolicy(data->span(), m_loader->frame()->settings().downloadableBinaryFontTrustedTypes());
}

void CachedFont::finishLoading(const FragmentedSharedBuffer* data, const NetworkLoadMetrics& metrics)
{
    if (data) {
        Ref dataContiguous = data->makeContiguous();
        m_fontParsingPolicy = policyForCustomFont(dataContiguous);
        if (m_fontParsingPolicy == FontParsingPolicy::Deny) {
            // SafeFontParser failed to parse font, we set a flag to signal it in CachedFontLoadRequest.h
            m_didRefuseToParseCustomFont = true;
            setErrorAndDeleteData();
            return;
        }
        m_data = WTFMove(dataContiguous);
        setEncodedSize(m_data->size());
    } else {
        m_data = nullptr;
        setEncodedSize(0);
    }
    setLoading(false);
    checkNotify(metrics);
}

void CachedFont::setErrorAndDeleteData()
{
    CachedResourceHandle protectedThis { *this };
    setEncodedSize(0);
    error(Status::DecodeError);
    if (inCache())
        MemoryCache::singleton().remove(*this);
    if (RefPtr loader = m_loader)
        loader->cancel();
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
    if (RefPtr data = m_data; !data->isContiguous())
        m_data = data->makeContiguous();
    return ensureCustomFontData(downcast<SharedBuffer>(m_data).get());
}

String CachedFont::calculateItemInCollection() const
{
    return url().fragmentIdentifier().toString();
}

bool CachedFont::ensureCustomFontData(SharedBuffer* data)
{
    if (!m_fontCustomPlatformData && !errorOccurred() && !isLoading() && data) {
        bool wrapping = false;
        switch (m_fontParsingPolicy) {
        case FontParsingPolicy::Deny:
            // This is not supposed to happen: loading should have cancelled
            // back in finishLoading. Nevertheless, we can recover in a healthy
            // manner.
            setErrorAndDeleteData();
            return false;

        case FontParsingPolicy::LoadWithSystemFontParser: {
            m_fontCustomPlatformData = createCustomFontData(*data, calculateItemInCollection(), wrapping);
            if (!m_fontCustomPlatformData)
                RELEASE_LOG(Fonts, "[Font Parser] A font could not be parsed by system font parser.");
            break;
        }
        case FontParsingPolicy::LoadWithSafeFontParser: {
            m_fontCustomPlatformData = createCustomFontDataExperimentalParser(*data, calculateItemInCollection(), wrapping);
            if (!m_fontCustomPlatformData) {
                m_didRefuseToParseCustomFont = true;
                RELEASE_LOG(Fonts, "[Font Parser] A font could not be parsed by safe font parser.");
            }
            break;
        }
        }

        m_hasCreatedFontDataWrappingResource = m_fontCustomPlatformData && wrapping;
        if (!m_fontCustomPlatformData) {
            if (m_fontParsingPolicy == FontParsingPolicy::LoadWithSafeFontParser) {
                m_didRefuseToParseCustomFont = true;
                setErrorAndDeleteData();
            } else
                setStatus(DecodeError);
        }
    }

    return m_fontCustomPlatformData.get();
}

RefPtr<FontCustomPlatformData> CachedFont::createCustomFontData(SharedBuffer& bytes, const String& itemInCollection, bool& wrapping)
{
    RefPtr buffer = { &bytes };
    wrapping = !convertWOFFToSfntIfNecessary(buffer);
    return buffer ? FontCustomPlatformData::create(*buffer, itemInCollection) : nullptr;
}

RefPtr<FontCustomPlatformData> CachedFont::createCustomFontDataExperimentalParser(SharedBuffer& bytes, const String& itemInCollection, bool& wrapping)
{
    RefPtr buffer = { &bytes };
    wrapping = !convertWOFFToSfntIfNecessary(buffer);
    return FontCustomPlatformData::createMemorySafe(*buffer, itemInCollection);
}

RefPtr<Font> CachedFont::createFont(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic, const FontCreationContext& fontCreationContext)
{
    return Font::create(platformDataFromCustomData(fontDescription, syntheticBold, syntheticItalic, fontCreationContext), Font::Origin::Remote);
}

FontPlatformData CachedFont::platformDataFromCustomData(const FontDescription& fontDescription, bool bold, bool italic, const FontCreationContext& fontCreationContext)
{
    RefPtr fontCustomPlatformData = m_fontCustomPlatformData;
    ASSERT(fontCustomPlatformData);
    return platformDataFromCustomData(*fontCustomPlatformData, fontDescription, bold, italic, fontCreationContext);
}

FontPlatformData CachedFont::platformDataFromCustomData(FontCustomPlatformData& fontCustomPlatformData, const FontDescription& fontDescription, bool bold, bool italic, const FontCreationContext& fontCreationContext)
{
    return fontCustomPlatformData.fontPlatformData(fontDescription, bold, italic, fontCreationContext);
}

void CachedFont::allClientsRemoved()
{
    m_fontCustomPlatformData = nullptr;
}

void CachedFont::checkNotify(const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess)
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
