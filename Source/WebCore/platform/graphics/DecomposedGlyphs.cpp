/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "DecomposedGlyphs.h"

namespace WebCore {

Ref<DecomposedGlyphs> DecomposedGlyphs::create(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode mode, RenderingResourceIdentifier renderingResourceIdentifier)
{
    return adoptRef(*new DecomposedGlyphs(font, glyphs, advances, count, localAnchor, mode, renderingResourceIdentifier));
}

Ref<DecomposedGlyphs> DecomposedGlyphs::create(PositionedGlyphs&& positionedGlyphs, const FloatRect& bounds, RenderingResourceIdentifier renderingResourceIdentifier)
{
    return adoptRef(*new DecomposedGlyphs(WTFMove(positionedGlyphs), bounds, renderingResourceIdentifier));
}

DecomposedGlyphs::DecomposedGlyphs(const Font& font, const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode mode, RenderingResourceIdentifier renderingResourceIdentifier)
    : m_positionedGlyphs({ glyphs, count }, { advances, count }, localAnchor, mode)
    , m_bounds(m_positionedGlyphs.computeBounds(font))
    , m_renderingResourceIdentifier(renderingResourceIdentifier)
{
}

DecomposedGlyphs::DecomposedGlyphs(PositionedGlyphs&& positionedGlyphs, const FloatRect& bounds, RenderingResourceIdentifier renderingResourceIdentifier)
    : m_positionedGlyphs(WTFMove(positionedGlyphs))
    , m_bounds(bounds)
    , m_renderingResourceIdentifier(renderingResourceIdentifier)
{
    ASSERT(m_positionedGlyphs.glyphs.size() == m_positionedGlyphs.advances.size());
}

DecomposedGlyphs::~DecomposedGlyphs()
{
    for (auto observer : m_observers)
        observer->releaseDecomposedGlyphs(m_renderingResourceIdentifier);
}

} // namespace WebCore
