//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayWgpu.cpp:
//    Implements the class methods for DisplayWgpu.
//

#include "libANGLE/renderer/wgpu/DisplayWgpu.h"

#include <dawn/dawn_proc.h>

#include "common/debug.h"
#include "common/platform.h"

#include "libANGLE/Display.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/DeviceWgpu.h"
#include "libANGLE/renderer/wgpu/DisplayWgpu_api.h"
#include "libANGLE/renderer/wgpu/ImageWgpu.h"
#include "libANGLE/renderer/wgpu/SurfaceWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_proc_utils.h"

#if defined(ANGLE_PLATFORM_LINUX)
#    if defined(ANGLE_USE_X11)
#        define ANGLE_WEBGPU_HAS_WINDOW_SURFACE_TYPE 1
#        define ANGLE_WEBGPU_WINDOW_SYSTEM angle::NativeWindowSystem::X11
#    elif defined(ANGLE_USE_WAYLAND)
#        define ANGLE_WEBGPU_HAS_WINDOW_SURFACE_TYPE 1
#        define ANGLE_WEBGPU_WINDOW_SYSTEM angle::NativeWindowSystem::Wayland
#    else
#        define ANGLE_WEBGPU_HAS_WINDOW_SURFACE_TYPE 0
#        define ANGLE_WEBGPU_WINDOW_SYSTEM angle::NativeWindowSystem::Other
#    endif
#else
#    define ANGLE_WEBGPU_HAS_WINDOW_SURFACE_TYPE 1
#    define ANGLE_WEBGPU_WINDOW_SYSTEM angle::NativeWindowSystem::Other
#endif

namespace rx
{

#if !ANGLE_WEBGPU_HAS_WINDOW_SURFACE_TYPE
WindowSurfaceWgpu *CreateWgpuWindowSurface(const egl::SurfaceState &surfaceState,
                                           EGLNativeWindowType window)
{
    UNIMPLEMENTED();
    return nullptr;
}
#endif

DisplayWgpu::DisplayWgpu(const egl::DisplayState &state) : DisplayImpl(state) {}

DisplayWgpu::~DisplayWgpu() {}

egl::Error DisplayWgpu::initialize(egl::Display *display)
{
    const egl::AttributeMap &attribs = display->getAttributeMap();
    mProcTable                       = *reinterpret_cast<DawnProcTable *>(
        attribs.get(EGL_PLATFORM_ANGLE_DAWN_PROC_TABLE_ANGLE,
                                          reinterpret_cast<EGLAttrib>(&webgpu::GetDefaultProcTable())));

    WGPUDevice providedDevice =
        reinterpret_cast<WGPUDevice>(attribs.get(EGL_PLATFORM_ANGLE_WEBGPU_DEVICE_ANGLE, 0));
    if (providedDevice)
    {
        mProcTable.deviceAddRef(providedDevice);
        mDevice = webgpu::DeviceHandle::Acquire(&mProcTable, providedDevice);

        mAdapter =
            webgpu::AdapterHandle::Acquire(&mProcTable, mProcTable.deviceGetAdapter(mDevice.get()));
        mInstance = webgpu::InstanceHandle::Acquire(&mProcTable,
                                                    mProcTable.adapterGetInstance(mAdapter.get()));
    }
    else
    {
        ANGLE_TRY(createWgpuDevice());
    }

    mQueue = webgpu::QueueHandle::Acquire(&mProcTable, mProcTable.deviceGetQueue(mDevice.get()));

    mFormatTable.initialize();

    mLimitsWgpu = WGPU_LIMITS_INIT;
    mProcTable.deviceGetLimits(mDevice.get(), &mLimitsWgpu);

    initializeFeatures();

    webgpu::GenerateCaps(mLimitsWgpu, &mGLCaps, &mGLTextureCaps, &mGLExtensions, &mGLLimitations,
                         &mEGLCaps, &mEGLExtensions, &mMaxSupportedClientVersion);

    return egl::NoError();
}

void DisplayWgpu::terminate()
{
    mAdapter  = nullptr;
    mInstance = nullptr;
    mDevice   = nullptr;
    mQueue    = nullptr;
}

egl::Error DisplayWgpu::makeCurrent(egl::Display *display,
                                    egl::Surface *drawSurface,
                                    egl::Surface *readSurface,
                                    gl::Context *context)
{
    // Ensure that the correct global DebugAnnotator is installed when the end2end tests change
    // the ANGLE back-end (done frequently).
    display->setGlobalDebugAnnotator();

    return egl::NoError();
}

egl::ConfigSet DisplayWgpu::generateConfigs()
{
    egl::Config config;
    config.renderTargetFormat    = GL_BGRA8_EXT;
    config.depthStencilFormat    = GL_DEPTH24_STENCIL8;
    config.bufferSize            = 32;
    config.redSize               = 8;
    config.greenSize             = 8;
    config.blueSize              = 8;
    config.alphaSize             = 8;
    config.alphaMaskSize         = 0;
    config.bindToTextureRGB      = EGL_FALSE;
    config.bindToTextureRGBA     = EGL_FALSE;
    config.colorBufferType       = EGL_RGB_BUFFER;
    config.configCaveat          = EGL_NONE;
    config.conformant            = EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT;
    config.depthSize             = 24;
    config.level                 = 0;
    config.matchNativePixmap     = EGL_NONE;
    config.maxPBufferWidth       = 0;
    config.maxPBufferHeight      = 0;
    config.maxPBufferPixels      = 0;
    config.maxSwapInterval       = 1;
    config.minSwapInterval       = 1;
    config.nativeRenderable      = EGL_TRUE;
    config.nativeVisualID        = 0;
    config.nativeVisualType      = EGL_NONE;
    config.renderableType        = EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT;
    config.sampleBuffers         = 0;
    config.samples               = 0;
    config.stencilSize           = 8;
    config.surfaceType           = EGL_PBUFFER_BIT;
#if ANGLE_WEBGPU_HAS_WINDOW_SURFACE_TYPE
    config.surfaceType |= EGL_WINDOW_BIT;
#endif
    config.optimalOrientation    = 0;
    config.transparentType       = EGL_NONE;
    config.transparentRedValue   = 0;
    config.transparentGreenValue = 0;
    config.transparentBlueValue  = 0;

    egl::ConfigSet configSet;
    configSet.add(config);
    return configSet;
}

bool DisplayWgpu::testDeviceLost()
{
    return false;
}

egl::Error DisplayWgpu::restoreLostDevice(const egl::Display *display)
{
    return egl::NoError();
}

egl::Error DisplayWgpu::validateClientBuffer(const egl::Config *configuration,
                                             EGLenum buftype,
                                             EGLClientBuffer clientBuffer,
                                             const egl::AttributeMap &attribs) const
{
    switch (buftype)
    {
        case EGL_WEBGPU_TEXTURE_ANGLE:
            return validateExternalWebGPUTexture(clientBuffer, attribs);
        default:
            return DisplayImpl::validateClientBuffer(configuration, buftype, clientBuffer, attribs);
    }
}

egl::Error DisplayWgpu::validateImageClientBuffer(const gl::Context *context,
                                                  EGLenum target,
                                                  EGLClientBuffer clientBuffer,
                                                  const egl::AttributeMap &attribs) const
{
    switch (target)
    {
        case EGL_WEBGPU_TEXTURE_ANGLE:
            return validateExternalWebGPUTexture(clientBuffer, attribs);
        default:
            return DisplayImpl::validateImageClientBuffer(context, target, clientBuffer, attribs);
    }
}

egl::Error DisplayWgpu::validateExternalWebGPUTexture(EGLClientBuffer buffer,
                                                      const egl::AttributeMap &attribs) const
{
    WGPUTexture externalTexture = reinterpret_cast<WGPUTexture>(buffer);
    if (externalTexture == nullptr)
    {
        return egl::Error(EGL_BAD_PARAMETER, "NULL Buffer");
    }

    WGPUTextureFormat externalTextureFormat = mProcTable.textureGetFormat(externalTexture);
    const webgpu::Format *webgpuFormat =
        getFormatForImportedTexture(attribs, externalTextureFormat);
    if (webgpuFormat == nullptr)
    {
        return egl::Error(EGL_BAD_PARAMETER, "Invalid format.");
    }

    return egl::NoError();
}

bool DisplayWgpu::isValidNativeWindow(EGLNativeWindowType window) const
{
    return true;
}

std::string DisplayWgpu::getRendererDescription()
{
    return "WebGPU";
}

std::string DisplayWgpu::getVendorString()
{
    return "WebGPU";
}

std::string DisplayWgpu::getVersionString(bool includeFullVersion)
{
    return std::string();
}

DeviceImpl *DisplayWgpu::createDevice()
{
    return new DeviceWgpu();
}

egl::Error DisplayWgpu::waitClient(const gl::Context *context)
{
    return egl::NoError();
}

egl::Error DisplayWgpu::waitNative(const gl::Context *context, EGLint engine)
{
    return egl::NoError();
}

gl::Version DisplayWgpu::getMaxSupportedESVersion() const
{
    return mMaxSupportedClientVersion;
}

gl::Version DisplayWgpu::getMaxConformantESVersion() const
{
    return mMaxSupportedClientVersion;
}

SurfaceImpl *DisplayWgpu::createWindowSurface(const egl::SurfaceState &state,
                                              EGLNativeWindowType window,
                                              const egl::AttributeMap &attribs)
{
    return CreateWgpuWindowSurface(state, window);
}

SurfaceImpl *DisplayWgpu::createPbufferSurface(const egl::SurfaceState &state,
                                               const egl::AttributeMap &attribs)
{
    return new OffscreenSurfaceWgpu(state, EGL_NONE, nullptr);
}

SurfaceImpl *DisplayWgpu::createPbufferFromClientBuffer(const egl::SurfaceState &state,
                                                        EGLenum buftype,
                                                        EGLClientBuffer buffer,
                                                        const egl::AttributeMap &attribs)
{
    return new OffscreenSurfaceWgpu(state, buftype, buffer);
}

SurfaceImpl *DisplayWgpu::createPixmapSurface(const egl::SurfaceState &state,
                                              NativePixmapType nativePixmap,
                                              const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

ImageImpl *DisplayWgpu::createImage(const egl::ImageState &state,
                                    const gl::Context *context,
                                    EGLenum target,
                                    const egl::AttributeMap &attribs)
{
    return new ImageWgpu(state, context);
}

ExternalImageSiblingImpl *DisplayWgpu::createExternalImageSibling(const gl::Context *context,
                                                                  EGLenum target,
                                                                  EGLClientBuffer buffer,
                                                                  const egl::AttributeMap &attribs)
{
    switch (target)
    {
        case EGL_WEBGPU_TEXTURE_ANGLE:
            return new WebGPUTextureImageSiblingWgpu(buffer, attribs);
        default:
            return DisplayImpl::createExternalImageSibling(context, target, buffer, attribs);
    }
}

rx::ContextImpl *DisplayWgpu::createContext(const gl::State &state,
                                            gl::ErrorSet *errorSet,
                                            const egl::Config *configuration,
                                            const gl::Context *shareContext,
                                            const egl::AttributeMap &attribs)
{
    return new ContextWgpu(state, errorSet, this);
}

StreamProducerImpl *DisplayWgpu::createStreamProducerD3DTexture(
    egl::Stream::ConsumerType consumerType,
    const egl::AttributeMap &attribs)
{
    UNIMPLEMENTED();
    return nullptr;
}

ShareGroupImpl *DisplayWgpu::createShareGroup(const egl::ShareGroupState &state)
{
    return new ShareGroupWgpu(state);
}

void DisplayWgpu::populateFeatureList(angle::FeatureList *features)
{
    mFeatures.populateFeatureList(features);
}

angle::NativeWindowSystem DisplayWgpu::getWindowSystem() const
{
    return ANGLE_WEBGPU_WINDOW_SYSTEM;
}

const webgpu::Format *DisplayWgpu::getFormatForImportedTexture(const egl::AttributeMap &attribs,
                                                               WGPUTextureFormat wgpuFormat) const
{
    GLenum requestedGLFormat = attribs.getAsInt(EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_NONE);
    GLenum requestedGLType   = attribs.getAsInt(EGL_TEXTURE_TYPE_ANGLE, GL_NONE);

    if (requestedGLFormat != GL_NONE)
    {
        const gl::InternalFormat &internalFormat =
            gl::GetInternalFormatInfo(requestedGLFormat, requestedGLType);
        if (internalFormat.internalFormat == GL_NONE)
        {
            return nullptr;
        }

        const webgpu::Format &format = mFormatTable[internalFormat.sizedInternalFormat];
        if (format.getActualWgpuTextureFormat() != wgpuFormat)
        {
            return nullptr;
        }

        return &format;
    }
    else
    {
        return mFormatTable.findClosestTextureFormat(wgpuFormat);
    }
}

void DisplayWgpu::generateExtensions(egl::DisplayExtensions *outExtensions) const
{
    *outExtensions = mEGLExtensions;
}

void DisplayWgpu::generateCaps(egl::Caps *outCaps) const
{
    *outCaps = mEGLCaps;
}

void DisplayWgpu::initializeFeatures()
{
    ApplyFeatureOverrides(&mFeatures, getState().featureOverrides);
    if (mState.featureOverrides.allDisabled)
    {
        return;
    }

    // Disabled by default. Gets explicitly enabled by ANGLE embedders.
    ANGLE_FEATURE_CONDITION((&mFeatures), avoidWaitAny, false);
}

egl::Error DisplayWgpu::createWgpuDevice()
{
    WGPUInstanceDescriptor instanceDescriptor          = WGPU_INSTANCE_DESCRIPTOR_INIT;
    static constexpr auto kTimedWaitAny     = WGPUInstanceFeatureName_TimedWaitAny;
    instanceDescriptor.requiredFeatureCount = 1;
    instanceDescriptor.requiredFeatures     = &kTimedWaitAny;
    mInstance = webgpu::InstanceHandle::Acquire(&mProcTable,
                                                mProcTable.createInstance(&instanceDescriptor));

    struct RequestAdapterResult
    {
        WGPURequestAdapterStatus status;
        webgpu::AdapterHandle adapter;
        std::string message;
    };
    RequestAdapterResult adapterResult;

    WGPURequestAdapterOptions requestAdapterOptions = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;

    WGPURequestAdapterCallbackInfo requestAdapterCallback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    requestAdapterCallback.mode                           = WGPUCallbackMode_WaitAnyOnly;
    requestAdapterCallback.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                                         struct WGPUStringView message, void *userdata1,
                                         void *userdata2) {
        RequestAdapterResult *result = reinterpret_cast<RequestAdapterResult *>(userdata1);
        const DawnProcTable *wgpu    = reinterpret_cast<const DawnProcTable *>(userdata2);

        result->status  = status;
        result->adapter = webgpu::AdapterHandle::Acquire(wgpu, adapter);
        result->message = std::string(message.data, message.length);
    };
    requestAdapterCallback.userdata1 = &adapterResult;
    requestAdapterCallback.userdata2 = &mProcTable;

    WGPUFutureWaitInfo futureWaitInfo;
    futureWaitInfo.future = mProcTable.instanceRequestAdapter(
        mInstance.get(), &requestAdapterOptions, requestAdapterCallback);

    WGPUWaitStatus status = mProcTable.instanceWaitAny(mInstance.get(), 1, &futureWaitInfo, -1);
    if (webgpu::IsWgpuError(status))
    {
        std::ostringstream err;
        err << "Failed to get WebGPU adapter: " << adapterResult.message;
        return egl::Error(EGL_BAD_ALLOC, err.str());
    }

    mAdapter = adapterResult.adapter;

    std::vector<WGPUFeatureName> requiredFeatures;  // empty for now

    WGPUDeviceDescriptor deviceDesc = WGPU_DEVICE_DESCRIPTOR_INIT;
    deviceDesc.requiredFeatureCount = requiredFeatures.size();
    deviceDesc.requiredFeatures     = requiredFeatures.data();
    deviceDesc.uncapturedErrorCallbackInfo.callback =
        [](WGPUDevice const *device, WGPUErrorType type, struct WGPUStringView message,
           void *userdata1, void *userdata2) {
            ASSERT(userdata1 == nullptr);
            ASSERT(userdata2 == nullptr);
            ERR() << "Error: " << static_cast<std::underlying_type<WGPUErrorType>::type>(type)
                  << " - message: " << std::string(message.data, message.length);
        };

    mDevice = webgpu::DeviceHandle::Acquire(
        &mProcTable, mProcTable.adapterCreateDevice(mAdapter.get(), &deviceDesc));
    return egl::NoError();
}

DisplayImpl *CreateWgpuDisplay(const egl::DisplayState &state)
{
    return new DisplayWgpu(state);
}

}  // namespace rx
