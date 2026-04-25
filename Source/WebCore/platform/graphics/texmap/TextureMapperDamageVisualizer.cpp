/*
 * Copyright (C) 2025 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextureMapperDamageVisualizer.h"

#if ENABLE(DAMAGE_TRACKING)
#include "Damage.h"
#include "TextureMapper.h"
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextureMapperDamageVisualizer);

std::unique_ptr<TextureMapperDamageVisualizer> TextureMapperDamageVisualizer::create()
{
    if (const auto* showDamageEnvvar = getenv("WEBKIT_SHOW_DAMAGE")) {
        const auto value = parseInteger<unsigned>(StringView::fromLatin1(showDamageEnvvar));
        if (value && *value > 0)
            return makeUnique<TextureMapperDamageVisualizer>(*value - 1);
    }
    return nullptr;
}

void TextureMapperDamageVisualizer::paintDamage(TextureMapper& textureMapper, const std::optional<Damage>& damage) const
{
    if (!damage)
        return;

    const auto color = Color::red.colorWithAlphaByte(200);
    for (const auto& rect : *damage)
        textureMapper.drawSolidColor(rect + m_margin, { }, color, true);
}

} // namespace WebCore

#endif // ENABLE(DAMAGE_TRACKING)
