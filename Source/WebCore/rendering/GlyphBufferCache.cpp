/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#include "GlyphBufferCache.h"

#include "platform/graphics/FontCascade.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

GlyphBufferCache& GlyphBufferCache::singleton()
{
    static NeverDestroyed<GlyphBufferCache> cache;
    return cache;
}

void GlyphBufferCache::clear()
{
    m_entries.clear();
}

unsigned GlyphBufferCache::size() const
{
    return m_entries.size();
}

GlyphBuffer GlyphBufferCache::glyphBuffer(const FontCascade& font, FontCascade::CodePath codePath, FontCascade::ForTextEmphasisOrNot forTextEmphasisOrNot, const TextRun& textRun, unsigned from, unsigned to)
{
    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        if (!m_entries.isEmpty()) {
            LOG(MemoryPressure, "GlyphBufferCache::%s - Under memory pressure - size: %d", __FUNCTION__, size());
            clear();
        }
        return font.layoutText(codePath, textRun, from, to, forTextEmphasisOrNot);
    }

    // if (from || to != textRun.length())
    //     return font.layoutText(codePath, textRun, from, to);

    if (font.isLoadingCustomFonts() || !font.fonts())
        return font.layoutText(codePath, textRun, from, to, forTextEmphasisOrNot);

    GlyphBufferCacheKey key { textRun, font, codePath, forTextEmphasisOrNot, from, to };
    if (auto entry = m_entries.find(key); entry != m_entries.end())
        return *entry->value;

    auto glyphBuffer = font.layoutText(codePath, textRun, from, to, forTextEmphasisOrNot);
    return *m_entries.add(key, WTF::makeUniqueRef<GlyphBuffer>(WTFMove((glyphBuffer)))).iterator->value;
}

} // namespace WebCore
