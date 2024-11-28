/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DecomposedGlyphs);

Ref<DecomposedGlyphs> DecomposedGlyphs::create(const GlyphBufferGlyph* glyphs, const GlyphBufferAdvance* advances, unsigned count, const FloatPoint& localAnchor, FontSmoothingMode mode, RenderingResourceIdentifier renderingResourceIdentifier)
{
    return adoptRef(*new DecomposedGlyphs({ Vector(std::span { glyphs, count }), Vector(std::span { advances, count }), localAnchor, mode }, renderingResourceIdentifier));
}

Ref<DecomposedGlyphs> DecomposedGlyphs::create(PositionedGlyphs&& positionedGlyphs, RenderingResourceIdentifier renderingResourceIdentifier)
{
    return adoptRef(*new DecomposedGlyphs(WTFMove(positionedGlyphs), renderingResourceIdentifier));
}

DecomposedGlyphs::DecomposedGlyphs(PositionedGlyphs&& positionedGlyphs, RenderingResourceIdentifier renderingResourceIdentifier)
    : RenderingResource(renderingResourceIdentifier)
    , m_positionedGlyphs(WTFMove(positionedGlyphs))
{
    ASSERT(m_positionedGlyphs.glyphs.size() == m_positionedGlyphs.advances.size());
}

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
