/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GraphicsContextGLTextureMapperAndroid.h"

#if ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && OS(ANDROID)
#include "ANGLEHeaders.h"
#include "GLFence.h"
#include "Logging.h"
#include "PlatformDisplay.h"
#include "TextureMapperFlags.h"
#include <android/hardware_buffer.h>

namespace WebCore {

RefPtr<GraphicsContextGLTextureMapperAndroid> GraphicsContextGLTextureMapperAndroid::create(GraphicsContextGLAttributes&& attributes)
{
    auto context = adoptRef(new GraphicsContextGLTextureMapperAndroid(WTFMove(attributes)));
    if (!context->initialize())
        return nullptr;
    return context;
}

bool GraphicsContextGLTextureMapperAndroid::platformInitializeExtensions()
{
    if (!enableExtension("GL_OES_EGL_image"_s))
        return false;

    const auto& eglExtensions = PlatformDisplay::sharedDisplay().eglExtensions();
    return eglExtensions.KHR_image_base && eglExtensions.ANDROID_get_native_client_buffer && eglExtensions.ANDROID_image_native_buffer;
}

#if ENABLE(WEBXR)
GCGLExternalImage GraphicsContextGLTextureMapperAndroid::createExternalImage(ExternalImageSource&& source, GCGLenum, GCGLint)
{
    if (m_displayObj == EGL_NO_DISPLAY) {
        addError(GCGLErrorCode::InvalidOperation);
        RELEASE_LOG_ERROR(XR, "Invalid display %#06x", EGL_GetError());
        return { };
    }

    static PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC s_eglGetNativeClientBufferANDROID { nullptr };
    if (!s_eglGetNativeClientBufferANDROID) [[unlikely]] {
        s_eglGetNativeClientBufferANDROID = reinterpret_cast<PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC>(EGL_GetProcAddress("eglGetNativeClientBufferANDROID"));
        RELEASE_ASSERT(s_eglGetNativeClientBufferANDROID);
    }

    static constexpr EGLint attributes[] = { EGL_IMAGE_PRESERVED, EGL_TRUE, EGL_NONE };

    auto clientBuffer = s_eglGetNativeClientBufferANDROID(source.hardwareBuffer.get());
    auto eglImage = EGL_CreateImageKHR(m_displayObj, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attributes);
    if (eglImage == EGL_NO_IMAGE_KHR) {
        RELEASE_LOG_ERROR(XR, "Failed to bind AHardwareBuffer to an EGLImage (%#06x). This is typically caused by "
            "a version mismatch between the gralloc implementation and the OpenGL/EGL driver. Please contact your "
            "GPU vendor to resolve this problem.", EGL_GetError());
        addError(GCGLErrorCode::InvalidOperation);
        return { };
    }

    auto newName = ++m_nextExternalImageName;
    m_eglImages.add(newName, eglImage);
    return newName;
}

void GraphicsContextGLTextureMapperAndroid::bindExternalImage(GCGLenum target, GCGLExternalImage image)
{
    if (!makeContextCurrent())
        return;

    EGLImage eglImage = EGL_NO_IMAGE_KHR;
    if (image) {
        eglImage = m_eglImages.get(image);
        if (eglImage == EGL_NO_IMAGE_KHR) {
            addError(GCGLErrorCode::InvalidOperation);
            return;
        }
    }

    if (target == RENDERBUFFER)
        GL_EGLImageTargetRenderbufferStorageOES(RENDERBUFFER, eglImage);
    else
        GL_EGLImageTargetTexture2DOES(target, eglImage);
}

bool GraphicsContextGLTextureMapperAndroid::enableRequiredWebXRExtensions()
{
    if (!makeContextCurrent())
        return false;

    return enableExtension("GL_OES_EGL_image"_s)
        && enableExtension("GL_OES_EGL_image_external"_s)
        && enableExtension("EGL_KHR_image_base"_s)
        && enableExtension("EGL_KHR_surfaceless_context"_s)
        && enableExtension("EGL_ANDROID_get_native_client_buffer"_s)
        && enableExtension("EGL_ANDROID_image_native_buffer"_s);
}
#endif // ENABLE(WEBXR)

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && OS(ANDROID)
