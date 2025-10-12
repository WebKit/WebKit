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

#include "FourCC.h"
#include <optional>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

typedef intptr_t EGLAttrib;
typedef void* EGLClientBuffer;
typedef void* EGLContext;
typedef void* EGLDisplay;
typedef void* EGLImage;
typedef unsigned EGLenum;

namespace WebCore {

class GLDisplay final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GLDisplay, WTF::DestructionThread::MainRunLoop> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(GLDisplay);
public:
    static RefPtr<GLDisplay> create(EGLDisplay);
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
#if OS(ANDROID)
        bool ANDROID_get_native_client_buffer { false };
        bool ANDROID_image_native_buffer { false };
#endif
    };
    const Extensions& extensions() const { return m_extensions; }

#if USE(GBM)
    struct BufferFormat {
        FourCC fourcc { 0 };
        Vector<uint64_t, 1> modifiers;
    };
    const Vector<BufferFormat>& bufferFormats();
#if USE(GSTREAMER)
    const Vector<BufferFormat>& bufferFormatsForVideo();
#endif
#endif

private:
    explicit GLDisplay(EGLDisplay);

    EGLDisplay m_display { nullptr };
    struct {
        int major { 0 };
        int minor { 0 };
    } m_version;
    Extensions m_extensions;

#if USE(GBM)
    Lock m_bufferFormatsLock;
    bool m_bufferFormatsInitialized WTF_GUARDED_BY_LOCK(m_bufferFormatsLock) { false };
    Vector<BufferFormat> m_bufferFormats;
#if USE(GSTREAMER)
    Lock m_bufferFormatsForVideoLock;
    bool m_bufferFormatsForVideoInitialized WTF_GUARDED_BY_LOCK(m_bufferFormatsForVideoLock) { false };
    Vector<BufferFormat> m_bufferFormatsForVideo;
#endif
#endif
};

} // namespace WebCore
