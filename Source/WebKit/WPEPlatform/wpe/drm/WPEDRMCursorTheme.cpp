/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEDRMCursorTheme.h"

#include "WPECursorTheme.h"
#include "WPEDRM.h"

namespace WPE {

namespace DRM {

std::unique_ptr<CursorTheme> CursorTheme::create(const char* name, uint32_t size)
{
    auto theme = WPE::CursorTheme::create(name, size);
    if (!theme)
        return nullptr;

    return makeUnique<CursorTheme>(WTFMove(theme));
}

std::unique_ptr<CursorTheme> CursorTheme::create()
{
    auto theme = WPE::CursorTheme::create();
    if (!theme)
        return nullptr;

    return makeUnique<CursorTheme>(WTFMove(theme));
}

CursorTheme::CursorTheme(std::unique_ptr<WPE::CursorTheme>&& theme)
    : m_theme(WTFMove(theme))
{
}

CursorTheme::~CursorTheme()
{
}

const Vector<CursorTheme::Image>& CursorTheme::cursor(const char* name, double scale, std::optional<uint32_t> maxImages)
{
    uint32_t size = m_theme->size() * static_cast<uint32_t>(scale);
    auto addResult = m_cursors.add({ CString(name), size }, Vector<Image> { });
    if (addResult.isNewEntry)
        loadCursor(name, scale, maxImages, addResult.iterator->value);
    return addResult.iterator->value;
}

void CursorTheme::loadCursor(const char* name, double scale, std::optional<uint32_t> maxImages, Vector<CursorTheme::Image>& images)
{
    // Try first with the scaled size.
    uint32_t scaledSize = m_theme->size() * static_cast<uint32_t>(scale);
    auto cursor = m_theme->loadCursor(name, scaledSize, maxImages);
    if (cursor.isEmpty())
        return;

    int effectiveScale = 1;
    if (cursor[0].width != scaledSize || cursor[0].height != scaledSize) {
        // Scaled size not found, use the original size.
        cursor = m_theme->loadCursor(name, m_theme->size(), maxImages);
        effectiveScale = scale;
        if (cursor.isEmpty())
            return;
    }

    for (auto& cursorImage : cursor) {
        images.append({ cursorImage.width * effectiveScale, cursorImage.height * effectiveScale,
            cursorImage.hotspotX * effectiveScale, cursorImage.hotspotY * effectiveScale, WTFMove(cursorImage.pixels) });
    }
}

} // namespace DRM

} // namespace WPE
