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

#include <optional>
#include <wtf/FastMalloc.h>
#include <wtf/Vector.h>
#include <wtf/glib/GUniquePtr.h>

namespace WPE {

class CursorTheme {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<CursorTheme> create(const char* path, uint32_t size);
    static std::unique_ptr<CursorTheme> create();

    CursorTheme(GUniquePtr<char>&&, uint32_t, Vector<GUniquePtr<char>>&&);
    ~CursorTheme() = default;

    uint32_t size() const { return m_size; }

    struct CursorImage {
        uint32_t width { 0 };
        uint32_t height { 0 };
        uint32_t hotspotX { 0 };
        uint32_t hotspotY { 0 };
        uint32_t delay { 0 };
        Vector<uint32_t> pixels;
    };
    Vector<CursorImage> loadCursor(const char*, uint32_t size, std::optional<uint32_t> maxImages = std::nullopt);

private:
    GUniquePtr<char> m_path;
    uint32_t m_size { 0 };
    Vector<GUniquePtr<char>> m_inherited;
};

} // namespace WPE
