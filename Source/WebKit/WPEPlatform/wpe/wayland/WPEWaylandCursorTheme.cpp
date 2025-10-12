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
#include "WPEWaylandCursorTheme.h"

#include "WPECursorTheme.h"
#include "WPEWaylandSHMPool.h"
#include <wtf/TZoneMallocInlines.h>

namespace WPE {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WaylandCursorTheme);

std::unique_ptr<WaylandCursorTheme> WaylandCursorTheme::create(const char* name, uint32_t size, struct wl_shm* shm)
{
    auto theme = CursorTheme::create(name, size);
    if (!theme)
        return nullptr;

    auto pool = WaylandSHMPool::create(shm, size * size * 4);
    if (!pool)
        return nullptr;

    return makeUnique<WaylandCursorTheme>(WTFMove(theme), WTFMove(pool));
}

std::unique_ptr<WaylandCursorTheme> WaylandCursorTheme::create(struct wl_shm* shm)
{
    auto theme = CursorTheme::create();
    if (!theme)
        return nullptr;

    auto pool = WaylandSHMPool::create(shm, theme->size() * theme->size() * 4);
    if (!pool)
        return nullptr;

    return makeUnique<WaylandCursorTheme>(WTFMove(theme), WTFMove(pool));
}

WaylandCursorTheme::WaylandCursorTheme(std::unique_ptr<CursorTheme>&& theme, std::unique_ptr<WaylandSHMPool>&& pool)
    : m_theme(WTFMove(theme))
    , m_pool(WTFMove(pool))
{

}

WaylandCursorTheme::~WaylandCursorTheme()
{
}

const Vector<WaylandCursorTheme::Image>& WaylandCursorTheme::cursor(const char* name, double scale, std::optional<uint32_t> maxImages)
{
    uint32_t size = m_theme->size() * static_cast<uint32_t>(scale);
    auto addResult = m_cursors.add({ CString(name), size }, Vector<Image> { });
    if (addResult.isNewEntry)
        loadCursor(name, scale, maxImages, addResult.iterator->value);
    return addResult.iterator->value;
}

void WaylandCursorTheme::loadCursor(const char* name, double scale, std::optional<uint32_t> maxImages, Vector<WaylandCursorTheme::Image>& images)
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

    for (const auto& cursorImage : cursor) {
        Image image = { cursorImage.width * effectiveScale, cursorImage.height * effectiveScale, cursorImage.hotspotX * effectiveScale, cursorImage.hotspotY * effectiveScale, nullptr };
        auto sizeInBytes = image.width * image.height * 4;
        int offset = m_pool->allocate(sizeInBytes);
        if (offset < 0)
            return;

        // FIXME: support upscaling cursor.
        if (effectiveScale == 1)
            m_pool->write(cursorImage.pixels.span(), offset);

        image.buffer = m_pool->createBuffer(offset, image.width, image.height, image.width * 4);
        images.append(WTFMove(image));
    }
}

} // namespace WPE
