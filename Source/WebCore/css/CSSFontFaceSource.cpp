/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
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
#include "CSSFontFaceSource.h"

#include "CSSFontFace.h"
#include "CSSFontSelector.h"
#include "CachedFontLoadRequest.h"
#include "CachedSVGFont.h"
#include "Document.h"
#include "Font.h"
#include "FontCache.h"
#include "FontCascadeDescription.h"
#include "FontCreationContext.h"
#include "FontCustomPlatformData.h"
#include "FontDescription.h"
#include "ResourceLoadObserver.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFontElement.h"
#include "SVGFontFaceElement.h"
#include "SVGToOTFFontConversion.h"
#include "SVGURIReference.h"
#include "SharedBuffer.h"

namespace WebCore {

inline void CSSFontFaceSource::setStatus(Status newStatus)
{
    switch (newStatus) {
    case Status::Pending:
        ASSERT_NOT_REACHED();
        break;
    case Status::Loading:
        ASSERT(status() == Status::Pending);
        break;
    case Status::Success:
        ASSERT(status() == Status::Loading);
        break;
    case Status::Failure:
        ASSERT(status() == Status::Loading);
        break;
    }

    m_status = newStatus;
}

CSSFontFaceSource::CSSFontFaceSource(CSSFontFace& owner, AtomString fontFaceName)
    : m_fontFaceName(WTFMove(fontFaceName))
    , m_face(owner)
{
}

CSSFontFaceSource::CSSFontFaceSource(CSSFontFace& owner, CSSFontSelector& fontSelector, UniqueRef<FontLoadRequest> request)
    : m_face(owner)
    , m_fontSelector(fontSelector)
    , m_fontRequest(request.moveToUniquePtr())
{
    // This may synchronously call fontLoaded().
    m_fontRequest->setClient(this);

    if (status() == Status::Pending && !m_fontRequest->isPending()) {
        setStatus(Status::Loading);
        if (!shouldIgnoreFontLoadCompletions()) {
            if (m_fontRequest->errorOccurred())
                setStatus(Status::Failure);
            else
                setStatus(Status::Success);
        }
    }
}

CSSFontFaceSource::CSSFontFaceSource(CSSFontFace& owner, AtomString fontFaceName, SVGFontFaceElement& fontFace)
    : m_fontFaceName(WTFMove(fontFaceName))
    , m_face(owner)
    , m_svgFontFaceElement(fontFace)
    , m_hasSVGFontFaceElement(true)
{
}

CSSFontFaceSource::CSSFontFaceSource(CSSFontFace& owner, Ref<JSC::ArrayBufferView>&& arrayBufferView)
    : m_face(owner)
    , m_immediateSource(WTFMove(arrayBufferView))
{
}

CSSFontFaceSource::~CSSFontFaceSource()
{
    if (m_fontRequest)
        m_fontRequest->setClient(nullptr);
}

bool CSSFontFaceSource::shouldIgnoreFontLoadCompletions() const
{
    return m_face.shouldIgnoreFontLoadCompletions();
}

void CSSFontFaceSource::opportunisticallyStartFontDataURLLoading()
{
    if (status() == Status::Pending && m_fontRequest && m_fontRequest->url().protocolIsData() && m_fontRequest->url().string().length() < MB)
        load();
}

void CSSFontFaceSource::fontLoaded(FontLoadRequest& fontRequest)
{
    ASSERT_UNUSED(fontRequest, &fontRequest == m_fontRequest.get());

    if (shouldIgnoreFontLoadCompletions())
        return;

    Ref<CSSFontFace> protectedFace(m_face);

    // If the font is in the cache, this will be synchronously called from FontLoadRequest::addClient().
    if (m_status == Status::Pending)
        setStatus(Status::Loading);
    else if (m_status == Status::Failure) {
        // This function may be called twice if loading was cancelled.
        ASSERT(m_fontRequest->errorOccurred());
        return;
    }

    if (m_fontRequest->errorOccurred() || !m_fontRequest->ensureCustomFontData())
        setStatus(Status::Failure);
    else
        setStatus(Status::Success);

    m_face.fontLoaded(*this);
}

void CSSFontFaceSource::load(Document* document)
{
    setStatus(Status::Loading);

    if (m_fontRequest) {
        ASSERT(m_fontSelector);
        if (auto* context = m_fontSelector->scriptExecutionContext())
            context->beginLoadingFontSoon(*m_fontRequest);
    } else {
        bool success = false;
        if (m_hasSVGFontFaceElement) {
            if (m_svgFontFaceElement && is<SVGFontElement>(m_svgFontFaceElement->parentNode())) {
                ASSERT(!m_inDocumentCustomPlatformData);
                SVGFontElement& fontElement = downcast<SVGFontElement>(*m_svgFontFaceElement->parentNode());
                if (auto otfFont = convertSVGToOTFFont(fontElement))
                    m_generatedOTFBuffer = SharedBuffer::create(WTFMove(otfFont.value()));
                if (m_generatedOTFBuffer) {
                    m_inDocumentCustomPlatformData = createFontCustomPlatformData(*m_generatedOTFBuffer, String());
                    success = static_cast<bool>(m_inDocumentCustomPlatformData);
                }
            }
        } else if (m_immediateSource) {
            ASSERT(!m_immediateFontCustomPlatformData);
            bool wrapping;
            auto buffer = SharedBuffer::create(static_cast<const char*>(m_immediateSource->baseAddress()), m_immediateSource->byteLength());
            m_immediateFontCustomPlatformData = CachedFont::createCustomFontData(buffer.get(), String(), wrapping);
            success = static_cast<bool>(m_immediateFontCustomPlatformData);
        } else {
            // We are only interested in whether or not fontForFamily() returns null or not. Luckily, none of
            // the values in the FontDescription other than the family name can cause the function to return
            // null if it wasn't going to otherwise (and vice-versa).
            FontCascadeDescription fontDescription;
            fontDescription.setOneFamily(m_fontFaceName);
            fontDescription.setComputedSize(1);
            fontDescription.setShouldAllowUserInstalledFonts(m_face.allowUserInstalledFonts());
            success = FontCache::forCurrentThread().fontForFamily(fontDescription, m_fontFaceName, { }, true);
            if (document && document->settings().webAPIStatisticsEnabled())
                ResourceLoadObserver::shared().logFontLoad(*document, m_fontFaceName.string(), success);
        }
        setStatus(success ? Status::Success : Status::Failure);
    }
}

RefPtr<Font> CSSFontFaceSource::font(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic, const FontCreationContext& fontCreationContext)
{
    ASSERT(status() == Status::Success);

    bool usesInDocumentSVGFont = m_hasSVGFontFaceElement;

    if (!m_fontRequest && !usesInDocumentSVGFont) {
        if (m_immediateSource) {
            if (!m_immediateFontCustomPlatformData)
                return nullptr;
            return Font::create(CachedFont::platformDataFromCustomData(*m_immediateFontCustomPlatformData, fontDescription, syntheticBold, syntheticItalic, fontCreationContext), Font::Origin::Remote);
        }

        // We're local. Just return a Font from the normal cache.
        // We don't want to check alternate font family names here, so pass true as the checkingAlternateName parameter.
        return FontCache::forCurrentThread().fontForFamily(fontDescription, m_fontFaceName, fontCreationContext, true);
    }

    if (m_fontRequest) {
        auto success = m_fontRequest->ensureCustomFontData();
        ASSERT_UNUSED(success, success);

        ASSERT(status() == Status::Success);
        auto result = m_fontRequest->createFont(fontDescription, syntheticBold, syntheticItalic, fontCreationContext);
        ASSERT(result);
        return result;
    }

    if (!usesInDocumentSVGFont)
        return nullptr;

    if (!m_svgFontFaceElement || !is<SVGFontElement>(m_svgFontFaceElement->parentNode()))
        return nullptr;
    if (!m_inDocumentCustomPlatformData)
        return nullptr;
    return Font::create(m_inDocumentCustomPlatformData->fontPlatformData(fontDescription, syntheticBold, syntheticItalic, fontCreationContext), Font::Origin::Remote);
}

bool CSSFontFaceSource::isSVGFontFaceSource() const
{
    return m_hasSVGFontFaceElement || (is<CachedFontLoadRequest>(m_fontRequest) && is<CachedSVGFont>(downcast<CachedFontLoadRequest>(m_fontRequest.get())->cachedFont()));
}

}
