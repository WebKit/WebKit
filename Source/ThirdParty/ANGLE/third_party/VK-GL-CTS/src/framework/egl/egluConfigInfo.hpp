#ifndef _EGLUCONFIGINFO_HPP
#define _EGLUCONFIGINFO_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief EGL config info.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "eglwDefs.hpp"
#include "eglwEnums.hpp"

namespace eglw
{
class Library;
}

namespace eglu
{

class ConfigInfo
{
public:
    // Core attributes
    int32_t bufferSize;
    int32_t redSize;
    int32_t greenSize;
    int32_t blueSize;
    int32_t luminanceSize;
    int32_t alphaSize;
    int32_t alphaMaskSize;
    uint32_t bindToTextureRGB;
    uint32_t bindToTextureRGBA;
    uint32_t colorBufferType;
    uint32_t configCaveat;
    int32_t configId;
    int32_t conformant;
    int32_t depthSize;
    int32_t level;
    int32_t maxPbufferWidth;
    int32_t maxPbufferHeight;
    int32_t maxSwapInterval;
    int32_t minSwapInterval;
    uint32_t nativeRenderable;
    int32_t nativeVisualId;
    int32_t nativeVisualType;
    int32_t renderableType;
    int32_t sampleBuffers;
    int32_t samples;
    int32_t stencilSize;
    int32_t surfaceType;
    uint32_t transparentType;
    int32_t transparentRedValue;
    int32_t transparentGreenValue;
    int32_t transparentBlueValue;

    // Extension attributes - set by queryExtConfigInfo()

    // EGL_EXT_yuv_surface
    uint32_t yuvOrder;
    int32_t yuvNumberOfPlanes;
    uint32_t yuvSubsample;
    uint32_t yuvDepthRange;
    uint32_t yuvCscStandard;
    int32_t yuvPlaneBpp;

    // EGL_EXT_pixel_format_float
    uint32_t colorComponentType;

    // EGL_ANDROID_recordable
    uint32_t recordableAndroid;

    // EGL_EXT_config_select_group
    int32_t groupId;

    ConfigInfo(void)
        : bufferSize(0)
        , redSize(0)
        , greenSize(0)
        , blueSize(0)
        , luminanceSize(0)
        , alphaSize(0)
        , alphaMaskSize(0)
        , bindToTextureRGB(0)
        , bindToTextureRGBA(0)
        , colorBufferType(0)
        , configCaveat(0)
        , configId(0)
        , conformant(0)
        , depthSize(0)
        , level(0)
        , maxPbufferWidth(0)
        , maxPbufferHeight(0)
        , maxSwapInterval(0)
        , minSwapInterval(0)
        , nativeRenderable(0)
        , nativeVisualId(0)
        , nativeVisualType(0)
        , renderableType(0)
        , sampleBuffers(0)
        , samples(0)
        , stencilSize(0)
        , surfaceType(0)
        , transparentType(0)
        , transparentRedValue(0)
        , transparentGreenValue(0)
        , transparentBlueValue(0)
        , yuvOrder(EGL_NONE)
        , yuvNumberOfPlanes(0)
        , yuvSubsample(EGL_NONE)
        , yuvDepthRange(EGL_NONE)
        , yuvCscStandard(EGL_NONE)
        , yuvPlaneBpp(EGL_YUV_PLANE_BPP_0_EXT)
        , colorComponentType(EGL_NONE)
        , recordableAndroid(0)
        , groupId(0)
    {
    }

    int32_t getAttribute(uint32_t attribute) const;
};

void queryCoreConfigInfo(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLConfig config, ConfigInfo *dst);
void queryExtConfigInfo(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLConfig config, ConfigInfo *dst);

} // namespace eglu

#endif // _EGLUCONFIGINFO_HPP
