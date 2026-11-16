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
 * \brief EGL config dst->
 *//*--------------------------------------------------------------------*/

#include "egluConfigInfo.hpp"
#include "egluDefs.hpp"
#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"
#include "egluUtil.hpp"
#include "deSTLUtil.hpp"

namespace eglu
{

using namespace eglw;

int32_t ConfigInfo::getAttribute(uint32_t attribute) const
{
    switch (attribute)
    {
    case EGL_BUFFER_SIZE:
        return bufferSize;
    case EGL_RED_SIZE:
        return redSize;
    case EGL_GREEN_SIZE:
        return greenSize;
    case EGL_BLUE_SIZE:
        return blueSize;
    case EGL_LUMINANCE_SIZE:
        return luminanceSize;
    case EGL_ALPHA_SIZE:
        return alphaSize;
    case EGL_ALPHA_MASK_SIZE:
        return alphaMaskSize;
    case EGL_BIND_TO_TEXTURE_RGB:
        return bindToTextureRGB;
    case EGL_BIND_TO_TEXTURE_RGBA:
        return bindToTextureRGBA;
    case EGL_COLOR_BUFFER_TYPE:
        return colorBufferType;
    case EGL_CONFIG_CAVEAT:
        return configCaveat;
    case EGL_CONFIG_ID:
        return configId;
    case EGL_CONFORMANT:
        return conformant;
    case EGL_DEPTH_SIZE:
        return depthSize;
    case EGL_LEVEL:
        return level;
    case EGL_MAX_PBUFFER_WIDTH:
        return maxPbufferWidth;
    case EGL_MAX_PBUFFER_HEIGHT:
        return maxPbufferHeight;
    case EGL_MAX_SWAP_INTERVAL:
        return maxSwapInterval;
    case EGL_MIN_SWAP_INTERVAL:
        return minSwapInterval;
    case EGL_NATIVE_RENDERABLE:
        return nativeRenderable;
    case EGL_NATIVE_VISUAL_ID:
        return nativeVisualId;
    case EGL_NATIVE_VISUAL_TYPE:
        return nativeVisualType;
    case EGL_RENDERABLE_TYPE:
        return renderableType;
    case EGL_SAMPLE_BUFFERS:
        return sampleBuffers;
    case EGL_SAMPLES:
        return samples;
    case EGL_STENCIL_SIZE:
        return stencilSize;
    case EGL_SURFACE_TYPE:
        return surfaceType;
    case EGL_TRANSPARENT_TYPE:
        return transparentType;
    case EGL_TRANSPARENT_RED_VALUE:
        return transparentRedValue;
    case EGL_TRANSPARENT_GREEN_VALUE:
        return transparentGreenValue;
    case EGL_TRANSPARENT_BLUE_VALUE:
        return transparentBlueValue;

    // EGL_EXT_yuv_surface
    case EGL_YUV_ORDER_EXT:
        return yuvOrder;
    case EGL_YUV_NUMBER_OF_PLANES_EXT:
        return yuvNumberOfPlanes;
    case EGL_YUV_SUBSAMPLE_EXT:
        return yuvSubsample;
    case EGL_YUV_DEPTH_RANGE_EXT:
        return yuvDepthRange;
    case EGL_YUV_CSC_STANDARD_EXT:
        return yuvCscStandard;
    case EGL_YUV_PLANE_BPP_EXT:
        return yuvPlaneBpp;

    // EGL_EXT_pixel_format_float
    case EGL_COLOR_COMPONENT_TYPE_EXT:
        return colorComponentType;

    // EGL_ANDROID_recordable
    case EGL_RECORDABLE_ANDROID:
        return recordableAndroid;

    // EGL_EXT_config_select_group
    case EGL_CONFIG_SELECT_GROUP_EXT:
        return groupId;

    default:
        TCU_THROW(InternalError, "Unknown attribute");
    }
}

void queryCoreConfigInfo(const Library &egl, EGLDisplay display, EGLConfig config, ConfigInfo *dst)
{
    egl.getConfigAttrib(display, config, EGL_BUFFER_SIZE, &dst->bufferSize);
    egl.getConfigAttrib(display, config, EGL_RED_SIZE, &dst->redSize);
    egl.getConfigAttrib(display, config, EGL_GREEN_SIZE, &dst->greenSize);
    egl.getConfigAttrib(display, config, EGL_BLUE_SIZE, &dst->blueSize);
    egl.getConfigAttrib(display, config, EGL_LUMINANCE_SIZE, &dst->luminanceSize);
    egl.getConfigAttrib(display, config, EGL_ALPHA_SIZE, &dst->alphaSize);
    egl.getConfigAttrib(display, config, EGL_ALPHA_MASK_SIZE, &dst->alphaMaskSize);
    egl.getConfigAttrib(display, config, EGL_BIND_TO_TEXTURE_RGB, (EGLint *)&dst->bindToTextureRGB);
    egl.getConfigAttrib(display, config, EGL_BIND_TO_TEXTURE_RGBA, (EGLint *)&dst->bindToTextureRGBA);
    egl.getConfigAttrib(display, config, EGL_COLOR_BUFFER_TYPE, (EGLint *)&dst->colorBufferType);
    egl.getConfigAttrib(display, config, EGL_CONFIG_CAVEAT, (EGLint *)&dst->configCaveat);
    egl.getConfigAttrib(display, config, EGL_CONFIG_ID, &dst->configId);
    egl.getConfigAttrib(display, config, EGL_CONFORMANT, &dst->conformant);
    egl.getConfigAttrib(display, config, EGL_DEPTH_SIZE, &dst->depthSize);
    egl.getConfigAttrib(display, config, EGL_LEVEL, &dst->level);
    egl.getConfigAttrib(display, config, EGL_MAX_PBUFFER_WIDTH, &dst->maxPbufferWidth);
    egl.getConfigAttrib(display, config, EGL_MAX_PBUFFER_HEIGHT, &dst->maxPbufferHeight);
    egl.getConfigAttrib(display, config, EGL_MAX_SWAP_INTERVAL, &dst->maxSwapInterval);
    egl.getConfigAttrib(display, config, EGL_MIN_SWAP_INTERVAL, &dst->minSwapInterval);
    egl.getConfigAttrib(display, config, EGL_NATIVE_RENDERABLE, (EGLint *)&dst->nativeRenderable);
    egl.getConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &dst->nativeVisualId);
    egl.getConfigAttrib(display, config, EGL_NATIVE_VISUAL_TYPE, &dst->nativeVisualType);
    egl.getConfigAttrib(display, config, EGL_RENDERABLE_TYPE, &dst->renderableType);
    egl.getConfigAttrib(display, config, EGL_SAMPLE_BUFFERS, &dst->sampleBuffers);
    egl.getConfigAttrib(display, config, EGL_SAMPLES, &dst->samples);
    egl.getConfigAttrib(display, config, EGL_STENCIL_SIZE, &dst->stencilSize);
    egl.getConfigAttrib(display, config, EGL_SURFACE_TYPE, &dst->surfaceType);
    egl.getConfigAttrib(display, config, EGL_TRANSPARENT_TYPE, (EGLint *)&dst->transparentType);
    egl.getConfigAttrib(display, config, EGL_TRANSPARENT_RED_VALUE, &dst->transparentRedValue);
    egl.getConfigAttrib(display, config, EGL_TRANSPARENT_GREEN_VALUE, &dst->transparentGreenValue);
    egl.getConfigAttrib(display, config, EGL_TRANSPARENT_BLUE_VALUE, &dst->transparentBlueValue);
    EGLU_CHECK_MSG(egl, "Failed to query config info");
}

void queryExtConfigInfo(const eglw::Library &egl, eglw::EGLDisplay display, eglw::EGLConfig config, ConfigInfo *dst)
{
    const std::vector<std::string> extensions = getDisplayExtensions(egl, display);

    if (de::contains(extensions.begin(), extensions.end(), "EGL_EXT_yuv_surface"))
    {
        egl.getConfigAttrib(display, config, EGL_YUV_ORDER_EXT, (EGLint *)&dst->yuvOrder);
        egl.getConfigAttrib(display, config, EGL_YUV_NUMBER_OF_PLANES_EXT, (EGLint *)&dst->yuvNumberOfPlanes);
        egl.getConfigAttrib(display, config, EGL_YUV_SUBSAMPLE_EXT, (EGLint *)&dst->yuvSubsample);
        egl.getConfigAttrib(display, config, EGL_YUV_DEPTH_RANGE_EXT, (EGLint *)&dst->yuvDepthRange);
        egl.getConfigAttrib(display, config, EGL_YUV_CSC_STANDARD_EXT, (EGLint *)&dst->yuvCscStandard);
        egl.getConfigAttrib(display, config, EGL_YUV_PLANE_BPP_EXT, (EGLint *)&dst->yuvPlaneBpp);

        EGLU_CHECK_MSG(egl, "Failed to query EGL_EXT_yuv_surface config attribs");
    }

    if (de::contains(extensions.begin(), extensions.end(), "EGL_ANDROID_recordable"))
    {
        egl.getConfigAttrib(display, config, EGL_RECORDABLE_ANDROID, (EGLint *)&dst->recordableAndroid);

        EGLU_CHECK_MSG(egl, "Failed to query EGL_ANDROID_recordable config attribs");
    }

    if (de::contains(extensions.begin(), extensions.end(), "EGL_EXT_pixel_format_float"))
    {
        egl.getConfigAttrib(display, config, EGL_COLOR_COMPONENT_TYPE_EXT, (EGLint *)&dst->colorComponentType);

        EGLU_CHECK_MSG(egl, "Failed to query EGL_EXT_pixel_format_float config attribs");
    }
    else
        dst->colorComponentType = EGL_COLOR_COMPONENT_TYPE_FIXED_EXT;

    if (hasExtension(egl, display, "EGL_EXT_config_select_group"))
    {
        egl.getConfigAttrib(display, config, EGL_CONFIG_SELECT_GROUP_EXT, (EGLint *)&dst->groupId);

        EGLU_CHECK_MSG(egl, "Failed to query EGL_EXT_config_select_group config attribs");
    }
}

} // namespace eglu
