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

#pragma once

#include <gbm.h>
#include <wtf/FastMalloc.h>
#include <wtf/glib/GUniquePtr.h>

namespace WPE {

namespace DRM {

class Buffer;
class CursorTheme;
class Plane;

class Cursor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Cursor(std::unique_ptr<Plane>&&, struct gbm_device*, uint32_t cursorWidth, uint32_t cursorHeight);
    ~Cursor();

    void setFromName(const char*, double);
    void setFromBytes(GBytes*, uint32_t width, uint32_t height, uint32_t stride, uint32_t hotspotX, uint32_t hotspotY);
    bool setPosition(uint32_t x, uint32_t y);
    uint32_t x() const { return m_position.x - m_hotspot.x; }
    uint32_t y() const { return m_position.y - m_hotspot.y; }
    uint32_t width() const { return m_deviceWidth; }
    uint32_t height() const { return m_deviceHeight; }

    const Plane& plane() const { return *m_plane; }
    Buffer* buffer() const { return m_isHidden ? nullptr : m_buffer.get(); }

private:
    bool tryEnsureBuffer();
    void updateBuffer(const uint8_t*, uint32_t width, uint32_t height, uint32_t stride);

    std::unique_ptr<Plane> m_plane;
    struct gbm_device* m_device { nullptr };
    uint32_t m_deviceWidth { 0 };
    uint32_t m_deviceHeight { 0 };
    std::unique_ptr<CursorTheme> m_theme;
    bool m_isHidden { false };
    GUniquePtr<char> m_name;
    std::unique_ptr<Buffer> m_buffer;
    struct {
        uint32_t x { 0 };
        uint32_t y { 0 };
    } m_position;
    struct {
        uint32_t x { 0 };
        uint32_t y { 0 };
    } m_hotspot;
};

} // namespace DRM

} // namespace WPE
