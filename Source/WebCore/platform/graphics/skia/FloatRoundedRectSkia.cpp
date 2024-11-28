/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "FloatRoundedRect.h"

#if USE(SKIA)

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkRRect.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

FloatRoundedRect::FloatRoundedRect(const SkRRect& skRect)
    : m_rect(skRect.rect())
{
    SkVector corner = skRect.radii(SkRRect::kUpperLeft_Corner);
    m_radii.setTopLeft({ corner.x(), corner.y() });
    corner = skRect.radii(SkRRect::kUpperRight_Corner);
    m_radii.setTopRight({ corner.x(), corner.y() });
    corner = skRect.radii(SkRRect::kLowerRight_Corner);
    m_radii.setBottomRight({ corner.x(), corner.y() });
    corner = skRect.radii(SkRRect::kLowerLeft_Corner);
    m_radii.setBottomLeft({ corner.x(), corner.y() });
}

FloatRoundedRect::operator SkRRect() const
{
    if (!isRounded())
        return SkRRect::MakeRect(rect());

    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib/Win port
    SkVector radii[4];
    radii[SkRRect::kUpperLeft_Corner].set(m_radii.topLeft().width(), m_radii.topLeft().height());
    radii[SkRRect::kUpperRight_Corner].set(m_radii.topRight().width(), m_radii.topRight().height());
    radii[SkRRect::kLowerRight_Corner].set(m_radii.bottomRight().width(), m_radii.bottomRight().height());
    radii[SkRRect::kLowerLeft_Corner].set(m_radii.bottomLeft().width(), m_radii.bottomLeft().height());
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    SkRRect skRect;
    skRect.setRectRadii(rect(), radii);
    return skRect;
}

} // namespace WebCore

#endif // USE(SKIA)
