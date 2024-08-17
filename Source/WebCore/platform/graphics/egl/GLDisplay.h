/*
 * Copyright (C) 2024 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#pragma once

#include <optional>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

typedef intptr_t EGLAttrib;
typedef void* EGLClientBuffer;
typedef void* EGLContext;
typedef void* EGLDisplay;
typedef void* EGLImage;
typedef unsigned EGLenum;

namespace WebCore {

class GLDisplay {
    WTF_MAKE_NONCOPYABLE(GLDisplay); WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<GLDisplay> create(EGLDisplay);
    explicit GLDisplay(EGLDisplay);
    ~GLDisplay() = default;

    EGLDisplay eglDisplay() const { return m_display; }
    bool checkVersion(int major, int minor) const;

    void terminate();

    EGLImage createImage(EGLContext, EGLenum, EGLClientBuffer, const Vector<EGLAttrib>&) const;
    bool destroyImage(EGLImage) const;

    struct Extensions {
        bool KHR_image_base { false };
        bool KHR_fence_sync { false };
        bool KHR_surfaceless_context { false };
        bool KHR_wait_sync { false };
        bool EXT_image_dma_buf_import { false };
        bool EXT_image_dma_buf_import_modifiers { false };
        bool MESA_image_dma_buf_export { false };
        bool ANDROID_native_fence_sync { false };
    };
    const Extensions& extensions() const { return m_extensions; }

#if USE(GBM)
    struct DMABufFormat {
        uint32_t fourcc { 0 };
        Vector<uint64_t, 1> modifiers;
    };
    const Vector<DMABufFormat>& dmabufFormats();
#endif

private:
    EGLDisplay m_display { nullptr };
    struct {
        int major { 0 };
        int minor { 0 };
    } m_version;
    Extensions m_extensions;

#if USE(GBM)
    Vector<DMABufFormat> m_dmabufFormats;
#endif
};

} // namespace WebCore
