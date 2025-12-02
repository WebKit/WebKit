/*
 * Copyright (C) 2015 Igalia S.L
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GLDisplay.h"
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

#if ENABLE(VIDEO) && USE(GSTREAMER_GL)
#include "GRefPtrGStreamer.h"

typedef struct _GstGLContext GstGLContext;
typedef struct _GstGLDisplay GstGLDisplay;
#endif // ENABLE(VIDEO) && USE(GSTREAMER_GL)

#if USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/gpu/ganesh/GrDirectContext.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/ThreadSafeWeakHashSet.h>
#endif

namespace WebCore {

class GLContext;
#if USE(SKIA)
class SkiaGLContext;
#endif

class PlatformDisplay {
    WTF_MAKE_TZONE_ALLOCATED(PlatformDisplay);
    WTF_MAKE_NONCOPYABLE(PlatformDisplay);
public:
    WEBCORE_EXPORT static PlatformDisplay& sharedDisplay();
#if !PLATFORM(WIN)
    WEBCORE_EXPORT static void setSharedDisplay(std::unique_ptr<PlatformDisplay>&&);
    WEBCORE_EXPORT static PlatformDisplay* sharedDisplayIfExists();
#endif
    virtual ~PlatformDisplay();

    enum class Type {
#if PLATFORM(WIN)
        Windows,
#endif
#if USE(WPE_RENDERER)
        WPE,
#endif
        Surfaceless,
#if USE(GBM)
        GBM,
#endif
#if OS(ANDROID)
        Android,
#endif
#if PLATFORM(GTK) || OS(ANDROID)
        Default,
#endif
    };

    virtual Type type() const = 0;

    WEBCORE_EXPORT GLContext* sharingGLContext();
    void clearGLContexts();
    EGLDisplay eglDisplay() const;
    GLDisplay& glDisplay() const { return m_eglDisplay.get(); }
    bool eglCheckVersion(int major, int minor) const;

    const GLDisplay::Extensions& eglExtensions() const;

    EGLImage createEGLImage(EGLContext, EGLenum target, EGLClientBuffer, const Vector<EGLAttrib>&) const;
    bool destroyEGLImage(EGLImage) const;
#if USE(GBM)
    const Vector<GLDisplay::BufferFormat>& bufferFormats();
#if USE(GSTREAMER)
    const Vector<GLDisplay::BufferFormat>& bufferFormatsForVideo();
#endif
#endif

#if ENABLE(WEBGL)
    EGLDisplay angleEGLDisplay() const;
    EGLContext angleSharingGLContext();
#endif

#if ENABLE(VIDEO) && USE(GSTREAMER_GL)
    GstGLDisplay* gstGLDisplay() const;
    GstGLContext* gstGLContext() const;
    void clearGStreamerGLState();
#endif

#if USE(SKIA)
    GLContext* skiaGLContext();
    GrDirectContext* skiaGrContext() const;
    unsigned msaaSampleCount() const;
#endif

protected:
    explicit PlatformDisplay(Ref<GLDisplay>&&);

    Ref<GLDisplay> m_eglDisplay;
    std::unique_ptr<GLContext> m_sharingGLContext;

#if ENABLE(WEBGL) && !PLATFORM(WIN)
    std::optional<int> m_anglePlatform;
    void* m_angleNativeDisplay { nullptr };
#endif

private:
#if USE(SKIA)
    void clearSkiaGLContext();
#endif

    void terminateEGLDisplay();

#if ENABLE(WEBGL) && !PLATFORM(WIN)
    mutable EGLDisplay m_angleEGLDisplay { nullptr };
#endif

#if ENABLE(VIDEO) && USE(GSTREAMER_GL)
    mutable GRefPtr<GstGLDisplay> m_gstGLDisplay;
    mutable GRefPtr<GstGLContext> m_gstGLContext;
#endif

#if USE(SKIA)
    ThreadSafeWeakHashSet<SkiaGLContext> m_skiaGLContexts;
#endif
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_PLATFORM_DISPLAY(ToClassName, DisplayType) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToClassName) \
    static bool isType(const WebCore::PlatformDisplay& display) { return display.type() == WebCore::PlatformDisplay::Type::DisplayType; } \
SPECIALIZE_TYPE_TRAITS_END()
