//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DisplayD3D.cpp: D3D implementation of egl::Display

#include "libANGLE/renderer/d3d/DisplayD3D.h"

#include <EGL/eglext.h>

#include "libANGLE/Context.h"
#include "libANGLE/Config.h"
#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/histogram_macros.h"
#include "libANGLE/renderer/d3d/EGLImageD3D.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/SurfaceD3D.h"
#include "libANGLE/renderer/d3d/SwapChainD3D.h"
#include "libANGLE/renderer/d3d/DeviceD3D.h"

#if defined (ANGLE_ENABLE_D3D9)
#   include "libANGLE/renderer/d3d/d3d9/Renderer9.h"
#endif // ANGLE_ENABLE_D3D9

#if defined (ANGLE_ENABLE_D3D11)
#   include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#endif // ANGLE_ENABLE_D3D11

#if !defined(ANGLE_DEFAULT_D3D11)
// Enables use of the Direct3D 11 API for a default display, when available
#   define ANGLE_DEFAULT_D3D11 1
#endif

namespace rx
{

typedef RendererD3D *(*CreateRendererD3DFunction)(egl::Display*);

template <typename RendererType>
static RendererD3D *CreateTypedRendererD3D(egl::Display *display)
{
    return new RendererType(display);
}

egl::Error CreateRendererD3D(egl::Display *display, RendererD3D **outRenderer)
{
    ASSERT(outRenderer != nullptr);

    std::vector<CreateRendererD3DFunction> rendererCreationFunctions;

    if (display->getPlatform() == EGL_PLATFORM_ANGLE_ANGLE)
    {
        const auto &attribMap              = display->getAttributeMap();
        EGLNativeDisplayType nativeDisplay = display->getNativeDisplayId();

        EGLint requestedDisplayType = static_cast<EGLint>(
            attribMap.get(EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE));

#   if defined(ANGLE_ENABLE_D3D11)
        if (nativeDisplay == EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE ||
            nativeDisplay == EGL_D3D11_ONLY_DISPLAY_ANGLE ||
            requestedDisplayType == EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
        {
            rendererCreationFunctions.push_back(CreateTypedRendererD3D<Renderer11>);
        }
#   endif

#   if defined(ANGLE_ENABLE_D3D9)
        if (nativeDisplay == EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE ||
            requestedDisplayType == EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE)
        {
            rendererCreationFunctions.push_back(CreateTypedRendererD3D<Renderer9>);
        }
#   endif

        if (nativeDisplay != EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE &&
            nativeDisplay != EGL_D3D11_ONLY_DISPLAY_ANGLE &&
            requestedDisplayType == EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE)
        {
        // The default display is requested, try the D3D9 and D3D11 renderers, order them using
        // the definition of ANGLE_DEFAULT_D3D11
#       if ANGLE_DEFAULT_D3D11
#           if defined(ANGLE_ENABLE_D3D11)
            rendererCreationFunctions.push_back(CreateTypedRendererD3D<Renderer11>);
#           endif
#           if defined(ANGLE_ENABLE_D3D9)
            rendererCreationFunctions.push_back(CreateTypedRendererD3D<Renderer9>);
#           endif
#       else
#           if defined(ANGLE_ENABLE_D3D9)
            rendererCreationFunctions.push_back(CreateTypedRendererD3D<Renderer9>);
#           endif
#           if defined(ANGLE_ENABLE_D3D11)
            rendererCreationFunctions.push_back(CreateTypedRendererD3D<Renderer11>);
#           endif
#       endif
        }
    }
    else if (display->getPlatform() == EGL_PLATFORM_DEVICE_EXT)
    {
#if defined(ANGLE_ENABLE_D3D11)
        if (display->getDevice()->getType() == EGL_D3D11_DEVICE_ANGLE)
        {
            rendererCreationFunctions.push_back(CreateTypedRendererD3D<Renderer11>);
        }
#endif
    }
    else
    {
        UNIMPLEMENTED();
    }

    for (size_t i = 0; i < rendererCreationFunctions.size(); i++)
    {
        RendererD3D *renderer = rendererCreationFunctions[i](display);
        egl::Error result     = renderer->initialize();

#       if defined(ANGLE_ENABLE_D3D11)
            if (renderer->getRendererClass() == RENDERER_D3D11)
            {
                ASSERT(result.getID() >= 0 && result.getID() < NUM_D3D11_INIT_ERRORS);
                ANGLE_HISTOGRAM_ENUMERATION("GPU.ANGLE.D3D11InitializeResult",
                                            result.getID(),
                                            NUM_D3D11_INIT_ERRORS);
            }
#       endif

#       if defined(ANGLE_ENABLE_D3D9)
            if (renderer->getRendererClass() == RENDERER_D3D9)
            {
                ASSERT(result.getID() >= 0 && result.getID() < NUM_D3D9_INIT_ERRORS);
                ANGLE_HISTOGRAM_ENUMERATION("GPU.ANGLE.D3D9InitializeResult",
                                            result.getID(),
                                            NUM_D3D9_INIT_ERRORS);
            }
#       endif

        if (!result.isError())
        {
            *outRenderer = renderer;
            return result;
        }

        // Failed to create the renderer, try the next
        SafeDelete(renderer);
    }

    return egl::Error(EGL_NOT_INITIALIZED, "No available renderers.");
}

DisplayD3D::DisplayD3D(const egl::DisplayState &state) : DisplayImpl(state), mRenderer(nullptr)
{
}

SurfaceImpl *DisplayD3D::createWindowSurface(const egl::SurfaceState &state,
                                             EGLNativeWindowType window,
                                             const egl::AttributeMap &attribs)
{
    ASSERT(mRenderer != nullptr);
    return new WindowSurfaceD3D(state, mRenderer, mDisplay, window, attribs);
}

SurfaceImpl *DisplayD3D::createPbufferSurface(const egl::SurfaceState &state,
                                              const egl::AttributeMap &attribs)
{
    ASSERT(mRenderer != nullptr);
    return new PbufferSurfaceD3D(state, mRenderer, mDisplay, 0, nullptr, attribs);
}

SurfaceImpl *DisplayD3D::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                       EGLenum buftype,
                                                       EGLClientBuffer clientBuffer,
                                                       const egl::AttributeMap &attribs)
{
    ASSERT(mRenderer != nullptr);
    return new PbufferSurfaceD3D(state, mRenderer, mDisplay, buftype, clientBuffer, attribs);
}

SurfaceImpl *DisplayD3D::createPixmapSurface(const egl::SurfaceState &state,
                                             NativePixmapType nativePixmap,
                                             const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

ImageImpl *DisplayD3D::createImage(EGLenum target,
                                   egl::ImageSibling *buffer,
                                   const egl::AttributeMap &attribs)
{
    return new EGLImageD3D(mRenderer, target, buffer, attribs);
}

egl::Error DisplayD3D::getDevice(DeviceImpl **device)
{
    return mRenderer->getEGLDevice(device);
}

ContextImpl *DisplayD3D::createContext(const gl::ContextState &state)
{
    ASSERT(mRenderer != nullptr);
    return mRenderer->createContext(state);
}

StreamProducerImpl *DisplayD3D::createStreamProducerD3DTextureNV12(
    egl::Stream::ConsumerType consumerType,
    const egl::AttributeMap &attribs)
{
    ASSERT(mRenderer != nullptr);
    return mRenderer->createStreamProducerD3DTextureNV12(consumerType, attribs);
}

egl::Error DisplayD3D::makeCurrent(egl::Surface *drawSurface, egl::Surface *readSurface, gl::Context *context)
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error DisplayD3D::initialize(egl::Display *display)
{
    ASSERT(mRenderer == nullptr && display != nullptr);
    mDisplay = display;
    ANGLE_TRY(CreateRendererD3D(display, &mRenderer));
    return egl::Error(EGL_SUCCESS);
}

void DisplayD3D::terminate()
{
    SafeDelete(mRenderer);
}

egl::ConfigSet DisplayD3D::generateConfigs()
{
    ASSERT(mRenderer != nullptr);
    return mRenderer->generateConfigs();
}

bool DisplayD3D::testDeviceLost()
{
    ASSERT(mRenderer != nullptr);
    return mRenderer->testDeviceLost();
}

egl::Error DisplayD3D::restoreLostDevice()
{
    // Release surface resources to make the Reset() succeed
    for (auto &surface : mState.surfaceSet)
    {
        if (surface->getBoundTexture())
        {
            surface->releaseTexImage(EGL_BACK_BUFFER);
        }
        SurfaceD3D *surfaceD3D = GetImplAs<SurfaceD3D>(surface);
        surfaceD3D->releaseSwapChain();
    }

    if (!mRenderer->resetDevice())
    {
        return egl::Error(EGL_BAD_ALLOC);
    }

    // Restore any surfaces that may have been lost
    for (const auto &surface : mState.surfaceSet)
    {
        SurfaceD3D *surfaceD3D = GetImplAs<SurfaceD3D>(surface);

        egl::Error error = surfaceD3D->resetSwapChain();
        if (error.isError())
        {
            return error;
        }
    }

    return egl::Error(EGL_SUCCESS);
}

bool DisplayD3D::isValidNativeWindow(EGLNativeWindowType window) const
{
    return mRenderer->isValidNativeWindow(window);
}

egl::Error DisplayD3D::validateClientBuffer(const egl::Config *configuration,
                                            EGLenum buftype,
                                            EGLClientBuffer clientBuffer,
                                            const egl::AttributeMap &attribs) const
{
    switch (buftype)
    {
        case EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE:
            return mRenderer->validateShareHandle(configuration, static_cast<HANDLE>(clientBuffer),
                                                  attribs);

        case EGL_D3D_TEXTURE_ANGLE:
            return mRenderer->getD3DTextureInfo(static_cast<IUnknown *>(clientBuffer), nullptr,
                                                nullptr, nullptr);

        default:
            return DisplayImpl::validateClientBuffer(configuration, buftype, clientBuffer, attribs);
    }
}

void DisplayD3D::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    mRenderer->generateDisplayExtensions(outExtensions);
}

std::string DisplayD3D::getVendorString() const
{
    std::string vendorString = "Google Inc.";
    if (mRenderer)
    {
        vendorString += " " + mRenderer->getVendorString();
    }

    return vendorString;
}

void DisplayD3D::generateCaps(egl::Caps *outCaps) const
{
    // Display must be initialized to generate caps
    ASSERT(mRenderer != nullptr);

    outCaps->textureNPOT = mRenderer->getNativeExtensions().textureNPOT;
}

egl::Error DisplayD3D::waitClient() const
{
    for (auto &surface : mState.surfaceSet)
    {
        SurfaceD3D *surfaceD3D = GetImplAs<SurfaceD3D>(surface);
        surfaceD3D->checkForOutOfDateSwapChain();
    }

    return egl::Error(EGL_SUCCESS);
}

egl::Error DisplayD3D::waitNative(EGLint engine,
                                  egl::Surface *drawSurface,
                                  egl::Surface *readSurface) const
{
    if (drawSurface != nullptr)
    {
        SurfaceD3D *drawSurfaceD3D = GetImplAs<SurfaceD3D>(drawSurface);
        drawSurfaceD3D->checkForOutOfDateSwapChain();
    }

    if (readSurface != nullptr)
    {
        SurfaceD3D *readurfaceD3D = GetImplAs<SurfaceD3D>(readSurface);
        readurfaceD3D->checkForOutOfDateSwapChain();
    }

    return egl::Error(EGL_SUCCESS);
}

gl::Version DisplayD3D::getMaxSupportedESVersion() const
{
    return mRenderer->getMaxSupportedESVersion();
}

}  // namespace rx
