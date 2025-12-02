/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "PlatformDisplay.h"

#if USE(SKIA)
#include "FontRenderOptions.h"
#include "GLContext.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <skia/gpu/ganesh/gl/GrGLDirectContext.h>
#include <skia/gpu/ganesh/gl/GrGLInterface.h>

#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#include <skia/gpu/ganesh/gl/epoxy/GrGLMakeEpoxyEGLInterface.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <skia/gpu/ganesh/gl/egl/GrGLMakeEGLInterface.h>
#endif
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN

#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/text/StringToIntegerConversion.h>

#if USE(LIBDRM)
#include <fcntl.h>
#include <unistd.h>
#include <wtf/unix/UnixFileDescriptor.h>
#include <xf86drm.h>
#endif

namespace WebCore {

#if PLATFORM(GTK) || PLATFORM(WPE)
#if CPU(X86) || CPU(X86_64)
// On x86 ot x86_64 we need at least 8 samples for the antialiasing result to be similar
// to non MSAA.
static const unsigned s_defaultSampleCount = 8;
#else
// On embedded, we sacrifice a bit of antialiasing quality to save memory and improve
// performance.
static const unsigned s_defaultSampleCount = 4;
#endif
#else
// Disable MSAA by default.
static const unsigned s_defaultSampleCount = 0;
#endif

#if !(PLATFORM(PLAYSTATION) && USE(COORDINATED_GRAPHICS))
static sk_sp<const GrGLInterface> skiaGLInterface()
{
    static NeverDestroyed<sk_sp<const GrGLInterface>> grGLInterface {
#if USE(LIBEPOXY)
        GrGLInterfaces::MakeEpoxyEGL()
#else
        GrGLInterfaces::MakeEGL()
#endif
    };

    return grGLInterface.get();
}

static thread_local RefPtr<SkiaGLContext> s_skiaGLContext;

#if USE(LIBDRM)
static inline bool isNewIntelDevice()
{
    auto eglDisplay = eglGetCurrentDisplay();
    if (eglDisplay == EGL_NO_DISPLAY)
        return false;

    if (!GLContext::isExtensionSupported(eglQueryString(nullptr, EGL_EXTENSIONS), "EGL_EXT_device_query"))
        return false;

    EGLDeviceEXT eglDevice;
    if (!eglQueryDisplayAttribEXT(eglDisplay, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&eglDevice)))
        return false;

    if (!GLContext::isExtensionSupported(eglQueryDeviceStringEXT(eglDevice, EGL_EXTENSIONS), "EGL_EXT_device_drm"))
        return false;

    const char* device = eglQueryDeviceStringEXT(eglDevice, EGL_DRM_DEVICE_FILE_EXT);
    if (!device || !*device)
        return false;

    auto fd = UnixFileDescriptor { open(device, O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
    if (!fd)
        return false;

    drmDevicePtr drmDevice;
    if (drmGetDevice2(fd.value(), 0, &drmDevice))
        return false;

    if (drmDevice->bustype != DRM_BUS_PCI) {
        drmFreeDevice(&drmDevice);
        return false;
    }

    auto vendorID = drmDevice->deviceinfo.pci->vendor_id;
    auto deviceID = drmDevice->deviceinfo.pci->device_id;
    drmFreeDevice(&drmDevice);

    static constexpr uint32_t intelVendorID = 0x8086;
    if (vendorID != intelVendorID)
        return false;

    // On pre-Ice Lake Intel GPUs MSAA performance is not acceptable.
    uint32_t maskedDeviceID = deviceID & 0xFF00;
    switch (maskedDeviceID) {
    case 0x2900: // Broadwater
    case 0x2A00: // Broadwater or Eaglelake
    case 0x2E00: // Eaglelake
    case 0x0000: // Ironlake
    case 0x0100: // Ivybridge, Baytrail or Sandybridge
    case 0x0F00: // Baytrail
    case 0x0A00: // Apollolake or Haswell
    case 0x0400: // Haswell
    case 0x0C00: // Haswell
    case 0x0D00: // Haswell
    case 0x2200: // Cherrytrail
    case 0x1600: // Broadwell
    case 0x5A00: // Apollolake or Cannonlake
    case 0x1900: // Skylake
    case 0x1A00: // Apollolake
    case 0x3100: // Geminilake
    case 0x5900: // Amberlake or Kabylake
    case 0x8700: // Kabylake or Coffeelake
    case 0x3E00: // Whiskeylake or Coffeelake
    case 0x9B00: // Cometlake
        return false;
    case 0x8A00: // Icelake
    case 0x4500: // Elkhartlake
    case 0x4E00: // Jasperlake
    case 0x9A00: // Tigerlake
    case 0x4c00: // Rocketlake
    case 0x4900: // DG1
    case 0x4600: // Alderlake
    case 0x4F00: // Alchemist
    case 0x5600: // Alchemist
    case 0xA700: // Raptorlake
    case 0x7D00: // Arrowlake or Meteorlake
    case 0xB600: // Arrowlake or Meteorlake
    case 0x6400: // Lunarlake
    case 0xE200: // Battlemage
    case 0xB000: // Pantherlake
        return true;
    default:
        break;
    }

    return false;
}
#endif

static bool shouldAllowMSAAOnNewIntel()
{
#if USE(LIBDRM)
    static std::once_flag onceFlag;
    static bool allowMSAAOnNewIntel;
    std::call_once(onceFlag, [] {
        allowMSAAOnNewIntel = isNewIntelDevice();
    });
    return allowMSAAOnNewIntel;
#else
    return false;
#endif
}

static unsigned initializeMSAASampleCount(GrDirectContext* grContext)
{
    static std::once_flag onceFlag;
    static int sampleCount = s_defaultSampleCount;

    std::call_once(onceFlag, [grContext] {
        // Let the user override the default sample count if they want to.
        String envString = String::fromLatin1(getenv("WEBKIT_SKIA_MSAA_SAMPLE_COUNT"));
        if (!envString.isEmpty())
            sampleCount = parseInteger<unsigned>(envString).value_or(0);

        if (sampleCount <= 1) {
            // Values of 0 or 1 mean disabling MSAA.
            sampleCount = 0;
            return;
        }

        // Skia checks internally whether MSAA is supported, but also disables it for several platforms where it
        // knows there are bugs. The only way to know whether our sample count will work is trying to create a
        // surface with that value and check whether it works.
        auto imageInfo = SkImageInfo::Make(512, 512, kRGBA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
        SkSurfaceProps properties = { 0, FontRenderOptions::singleton().subpixelOrder() };
        auto surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kNo, imageInfo, sampleCount, kTopLeft_GrSurfaceOrigin, &properties);

        // If the creation of the surface failed, disable MSAA.
        if (!surface)
            sampleCount = 0;
    });

    return sampleCount;
}

class SkiaGLContext : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<SkiaGLContext> {
public:
    static Ref<SkiaGLContext> create(PlatformDisplay& display)
    {
        return adoptRef(*new SkiaGLContext(display));
    }

    ~SkiaGLContext() = default;

    GLContext* skiaGLContext() const
    {
        Locker locker { m_lock };
        return m_skiaGLContext.get();
    }

    GrDirectContext* skiaGrContext() const
    {
        Locker locker { m_lock };
        return m_skiaGrContext.get();
    }

    unsigned sampleCount() const
    {
        return m_sampleCount;
    }

private:
    explicit SkiaGLContext(PlatformDisplay& display)
    {
        auto glContext = GLContext::createOffscreen(display);
        if (!glContext || !glContext->makeContextCurrent())
            return;

        // FIXME: add GrContextOptions, shader cache, etc.
        GrContextOptions options;
        options.fAllowMSAAOnNewIntel = shouldAllowMSAAOnNewIntel();
        if (auto grContext = GrDirectContexts::MakeGL(skiaGLInterface(), options)) {
            m_skiaGLContext = WTFMove(glContext);
            m_skiaGrContext = WTFMove(grContext);
            m_sampleCount = initializeMSAASampleCount(m_skiaGrContext.get());
        }
    }

    std::unique_ptr<GLContext> m_skiaGLContext WTF_GUARDED_BY_LOCK(m_lock);
    sk_sp<GrDirectContext> m_skiaGrContext WTF_GUARDED_BY_LOCK(m_lock);
    mutable Lock m_lock;
    unsigned m_sampleCount { 0 };
};
#endif

GLContext* PlatformDisplay::skiaGLContext()
{
#if PLATFORM(GTK) || PLATFORM(WPE) || (PLATFORM(PLAYSTATION) && !USE(COORDINATED_GRAPHICS))
    if (!s_skiaGLContext) {
        s_skiaGLContext = SkiaGLContext::create(*this);
        m_skiaGLContexts.add(*s_skiaGLContext);
    }
    return s_skiaGLContext->skiaGLContext();
#else
    // The PlayStation OpenGL implementation does not dispatch to the context bound to
    // the current thread so Skia cannot use OpenGL with coordinated graphics.
    return nullptr;
#endif
}

GrDirectContext* PlatformDisplay::skiaGrContext() const
{
    RELEASE_ASSERT(s_skiaGLContext);
    return s_skiaGLContext->skiaGrContext();
}

unsigned PlatformDisplay::msaaSampleCount() const
{
    return s_skiaGLContext ? s_skiaGLContext->sampleCount() : 0;
}

void PlatformDisplay::clearSkiaGLContext()
{
    s_skiaGLContext = nullptr;
}

} // namespace WebCore

#endif // USE(SKIA)
