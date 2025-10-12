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
#include "WPEDRMCursor.h"

#include "WPEDRM.h"
#include "WPEDRMCursorTheme.h"
#include <wtf/TZoneMallocInlines.h>

namespace WPE {

namespace DRM {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Cursor);

Cursor::Cursor(std::unique_ptr<Plane>&& plane, struct gbm_device* device, uint32_t cursorWidth, uint32_t cursorHeight)
    : m_plane(WTFMove(plane))
    , m_device(device)
    , m_deviceWidth(cursorWidth)
    , m_deviceHeight(cursorHeight)
    , m_theme(CursorTheme::create())
{
}

Cursor::~Cursor()
{
}

bool Cursor::tryEnsureBuffer()
{
    if (m_buffer)
        return true;

    auto* bo = gbm_bo_create(m_device, m_deviceWidth, m_deviceHeight, GBM_FORMAT_ARGB8888, GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);
    if (!bo)
        return false;

    m_buffer = WPE::DRM::Buffer::create(bo);
    if (!m_buffer) {
        gbm_bo_destroy(bo);
        return false;
    }

    return true;
}

void Cursor::updateBuffer(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t stride)
{
    RELEASE_ASSERT(width <= m_deviceWidth);
    RELEASE_ASSERT(height <= m_deviceHeight);

    Vector<uint32_t> deviceBuffer(m_deviceWidth * m_deviceHeight);
    for (uint32_t i = 0; i < height; ++i)
        memcpy(&deviceBuffer[i * m_deviceWidth], pixels + i * stride, stride);
    gbm_bo_write(m_buffer->bufferObject(), deviceBuffer.span().data(), deviceBuffer.sizeInBytes());
}

void Cursor::setFromName(const char* name, double scale)
{
    if (!m_theme)
        return;

    if (!g_strcmp0(m_name.get(), name))
        return;

    m_name.reset(g_strdup(name));
    if (!g_strcmp0(m_name.get(), "none")) {
        m_isHidden = true;
        return;
    }

    // FIXME: support animated cursors.
    const auto& cursor = m_theme->cursor(name, scale, 1);
    if (cursor.isEmpty()) {
        g_warning("Cursor %s not found in theme", name);
        return;
    }

    if (!tryEnsureBuffer())
        return;

    m_isHidden = false;
    updateBuffer(reinterpret_cast<const uint8_t*>(cursor[0].pixels.span().data()), cursor[0].width, cursor[0].height, cursor[0].width * 4);
    m_hotspot.x = cursor[0].hotspotX;
    m_hotspot.y = cursor[0].hotspotY;
}

void Cursor::setFromBytes(GBytes* bytes, uint32_t width, uint32_t height, uint32_t stride, uint32_t hotspotX, uint32_t hotspotY)
{
    if (!tryEnsureBuffer())
        return;

    m_isHidden = false;
    m_name = nullptr;
    updateBuffer(reinterpret_cast<const uint8_t*>(g_bytes_get_data(bytes, nullptr)), width, height, stride);
    m_hotspot.x = hotspotX;
    m_hotspot.y = hotspotY;
}

bool Cursor::setPosition(uint32_t x, uint32_t y)
{
    if (x == m_position.x && y == m_position.y)
        return false;

    m_position.x = x;
    m_position.y = y;
    return true;
}

} // namespace WPE

} // namespace DRM
