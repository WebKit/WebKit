/*
 * Copyright (C) 2007, 2008, 2011, 2013 Apple Inc. All rights reserved.
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
#include "CSSFontFace.h"

#include "CSSFontFaceSource.h"
#include "CSSFontSelector.h"
#include "CSSSegmentedFontFace.h"
#include "Document.h"
#include "Font.h"
#include "FontDescription.h"
#include "FontLoader.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

bool CSSFontFace::allSourcesFailed() const
{
    for (auto& source : m_sources) {
        if (source->status() != CSSFontFaceSource::Status::Failure)
            return false;
    }
    return true;
}

void CSSFontFace::addedToSegmentedFontFace(CSSSegmentedFontFace& segmentedFontFace)
{
    m_segmentedFontFaces.add(&segmentedFontFace);
}

void CSSFontFace::removedFromSegmentedFontFace(CSSSegmentedFontFace& segmentedFontFace)
{
    m_segmentedFontFaces.remove(&segmentedFontFace);
}

void CSSFontFace::adoptSource(std::unique_ptr<CSSFontFaceSource>&& source)
{
    m_sources.append(WTFMove(source));
}

void CSSFontFace::fontLoaded(CSSFontFaceSource&)
{
    // FIXME: Can we assert that m_segmentedFontFaces is not empty? That may
    // require stopping in-progress font loading when the last
    // CSSSegmentedFontFace is removed.
    if (m_segmentedFontFaces.isEmpty())
        return;

    (*m_segmentedFontFaces.begin())->fontSelector().fontLoaded();

    for (auto* face : m_segmentedFontFaces)
        face->fontLoaded(*this);
}

RefPtr<Font> CSSFontFace::font(const FontDescription& fontDescription, bool syntheticBold, bool syntheticItalic)
{
    if (allSourcesFailed())
        return nullptr;

    ASSERT(!m_segmentedFontFaces.isEmpty());
    CSSFontSelector& fontSelector = (*m_segmentedFontFaces.begin())->fontSelector();

    for (auto& source : m_sources) {
        if (source->status() == CSSFontFaceSource::Status::Pending)
            source->load(fontSelector);

        switch (source->status()) {
        case CSSFontFaceSource::Status::Pending:
            ASSERT_NOT_REACHED();
            break;
        case CSSFontFaceSource::Status::Loading:
            return Font::create(FontCache::singleton().lastResortFallbackFont(fontDescription)->platformData(), true, true);
        case CSSFontFaceSource::Status::Success:
            if (RefPtr<Font> result = source->font(fontDescription, syntheticBold, syntheticItalic, m_featureSettings, m_variantSettings))
                return WTFMove(result);
            break;
        case CSSFontFaceSource::Status::Failure:
            break;
        }
    }

    return nullptr;
}

#if ENABLE(SVG_FONTS)
bool CSSFontFace::hasSVGFontFaceSource() const
{
    size_t size = m_sources.size();
    for (size_t i = 0; i < size; i++) {
        if (m_sources[i]->isSVGFontFaceSource())
            return true;
    }
    return false;
}
#endif

}
