//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Config.cpp: Implements the egl::Config class, describing the format, type
// and size for an egl::Surface. Implements EGLConfig and related functionality.
// [EGL 1.4] section 3.4 page 15.

#include "libEGL/Config.h"

#include <algorithm>
#include <vector>

#include "common/debug.h"

using namespace std;

namespace egl
{
Config::Config(D3DDISPLAYMODE displayMode, EGLint minInterval, EGLint maxInterval, D3DFORMAT renderTargetFormat, D3DFORMAT depthStencilFormat, EGLint multiSample, EGLint texWidth, EGLint texHeight)
    : mDisplayMode(displayMode), mRenderTargetFormat(renderTargetFormat), mDepthStencilFormat(depthStencilFormat), mMultiSample(multiSample)
{
    set(displayMode, minInterval, maxInterval, renderTargetFormat, depthStencilFormat, multiSample, texWidth, texHeight);
}

void Config::setDefaults()
{
    mBufferSize = 0;
    mRedSize = 0;
    mGreenSize = 0;
    mBlueSize = 0;
    mLuminanceSize = 0;
    mAlphaSize = 0;
    mAlphaMaskSize = 0;
    mBindToTextureRGB = EGL_DONT_CARE;
    mBindToTextureRGBA = EGL_DONT_CARE;
    mColorBufferType = EGL_RGB_BUFFER;
    mConfigCaveat = EGL_DONT_CARE;
    mConfigID = EGL_DONT_CARE;
    mConformant = 0;
    mDepthSize = 0;
    mLevel = 0;
    mMatchNativePixmap = EGL_NONE;
    mMaxPBufferWidth = 0;
    mMaxPBufferHeight = 0;
    mMaxPBufferPixels = 0;
    mMaxSwapInterval = EGL_DONT_CARE;
    mMinSwapInterval = EGL_DONT_CARE;
    mNativeRenderable = EGL_DONT_CARE;
    mNativeVisualID = 0;
    mNativeVisualType = EGL_DONT_CARE;
    mRenderableType = EGL_OPENGL_ES_BIT;
    mSampleBuffers = 0;
    mSamples = 0;
    mStencilSize = 0;
    mSurfaceType = EGL_WINDOW_BIT;
    mTransparentType = EGL_NONE;
    mTransparentRedValue = EGL_DONT_CARE;
    mTransparentGreenValue = EGL_DONT_CARE;
    mTransparentBlueValue = EGL_DONT_CARE;
}

void Config::set(D3DDISPLAYMODE displayMode, EGLint minInterval, EGLint maxInterval, D3DFORMAT renderTargetFormat, D3DFORMAT depthStencilFormat, EGLint multiSample, EGLint texWidth, EGLint texHeight)
{
    mBindToTextureRGB = EGL_FALSE;
    mBindToTextureRGBA = EGL_FALSE;
    switch (renderTargetFormat)
    {
      case D3DFMT_A1R5G5B5:
        mBufferSize = 16;
        mRedSize = 5;
        mGreenSize = 5;
        mBlueSize = 5;
        mAlphaSize = 1;
        break;
      case D3DFMT_A2R10G10B10:
        mBufferSize = 32;
        mRedSize = 10;
        mGreenSize = 10;
        mBlueSize = 10;
        mAlphaSize = 2;
        break;
      case D3DFMT_A8R8G8B8:
        mBufferSize = 32;
        mRedSize = 8;
        mGreenSize = 8;
        mBlueSize = 8;
        mAlphaSize = 8;
        mBindToTextureRGBA = true;
        break;
      case D3DFMT_R5G6B5:
        mBufferSize = 16;
        mRedSize = 5;
        mGreenSize = 6;
        mBlueSize = 5;
        mAlphaSize = 0;
        break;
      case D3DFMT_X8R8G8B8:
        mBufferSize = 32;
        mRedSize = 8;
        mGreenSize = 8;
        mBlueSize = 8;
        mAlphaSize = 0;
        mBindToTextureRGB = true;
        break;
      default:
        UNREACHABLE();   // Other formats should not be valid
    }

    mLuminanceSize = 0;
    mAlphaMaskSize = 0;
    mColorBufferType = EGL_RGB_BUFFER;
    mConfigCaveat = (displayMode.Format == renderTargetFormat) ? EGL_NONE : EGL_SLOW_CONFIG;
    mConfigID = 0;
    mConformant = EGL_OPENGL_ES2_BIT;

    switch (depthStencilFormat)
    {
      case D3DFMT_UNKNOWN:
        mDepthSize = 0;
        mStencilSize = 0;
        break;
//    case D3DFMT_D16_LOCKABLE:
//      mDepthSize = 16;
//      mStencilSize = 0;
//      break;
      case D3DFMT_D32:
        mDepthSize = 32;
        mStencilSize = 0;
        break;
      case D3DFMT_D15S1:
        mDepthSize = 15;
        mStencilSize = 1;
        break;
      case D3DFMT_D24S8:
        mDepthSize = 24;
        mStencilSize = 8;
        break;
      case D3DFMT_D24X8:
        mDepthSize = 24;
        mStencilSize = 0;
        break;
      case D3DFMT_D24X4S4:
        mDepthSize = 24;
        mStencilSize = 4;
        break;
      case D3DFMT_D16:
        mDepthSize = 16;
        mStencilSize = 0;
        break;
//    case D3DFMT_D32F_LOCKABLE:
//      mDepthSize = 32;
//      mStencilSize = 0;
//      break;
//    case D3DFMT_D24FS8:
//      mDepthSize = 24;
//      mStencilSize = 8;
//      break;
      default:
        UNREACHABLE();
    }

    mLevel = 0;
    mMatchNativePixmap = EGL_NONE;
    mMaxPBufferWidth = texWidth;
    mMaxPBufferHeight = texHeight;
    mMaxPBufferPixels = texWidth*texHeight;
    mMaxSwapInterval = maxInterval;
    mMinSwapInterval = minInterval;
    mNativeRenderable = EGL_FALSE;
    mNativeVisualID = 0;
    mNativeVisualType = 0;
    mRenderableType = EGL_OPENGL_ES2_BIT;
    mSampleBuffers = multiSample ? 1 : 0;
    mSamples = multiSample;
    mSurfaceType = EGL_PBUFFER_BIT | EGL_WINDOW_BIT | EGL_SWAP_BEHAVIOR_PRESERVED_BIT;
    mTransparentType = EGL_NONE;
    mTransparentRedValue = 0;
    mTransparentGreenValue = 0;
    mTransparentBlueValue = 0;
}

EGLConfig Config::getHandle() const
{
    return (EGLConfig)(size_t)mConfigID;
}

SortConfig::SortConfig(const EGLint *attribList)
    : mWantRed(false), mWantGreen(false), mWantBlue(false), mWantAlpha(false), mWantLuminance(false)
{
    scanForWantedComponents(attribList);
}

void SortConfig::scanForWantedComponents(const EGLint *attribList)
{
    // [EGL] section 3.4.1 page 24
    // Sorting rule #3: by larger total number of color bits, not considering
    // components that are 0 or don't-care.
    for (const EGLint *attr = attribList; attr[0] != EGL_NONE; attr += 2)
    {
        if (attr[1] != 0 && attr[1] != EGL_DONT_CARE)
        {
            switch (attr[0])
            {
              case EGL_RED_SIZE:       mWantRed = true; break;
              case EGL_GREEN_SIZE:     mWantGreen = true; break;
              case EGL_BLUE_SIZE:      mWantBlue = true; break;
              case EGL_ALPHA_SIZE:     mWantAlpha = true; break;
              case EGL_LUMINANCE_SIZE: mWantLuminance = true; break;
            }
        }
    }
}

EGLint SortConfig::wantedComponentsSize(const Config &config) const
{
    EGLint total = 0;

    if (mWantRed)       total += config.mRedSize;
    if (mWantGreen)     total += config.mGreenSize;
    if (mWantBlue)      total += config.mBlueSize;
    if (mWantAlpha)     total += config.mAlphaSize;
    if (mWantLuminance) total += config.mLuminanceSize;

    return total;
}

bool SortConfig::operator()(const Config *x, const Config *y) const
{
    return (*this)(*x, *y);
}

bool SortConfig::operator()(const Config &x, const Config &y) const
{
    #define SORT(attribute)                        \
        if (x.attribute != y.attribute)            \
        {                                          \
            return x.attribute < y.attribute;      \
        }

    META_ASSERT(EGL_NONE < EGL_SLOW_CONFIG && EGL_SLOW_CONFIG < EGL_NON_CONFORMANT_CONFIG);
    SORT(mConfigCaveat);

    META_ASSERT(EGL_RGB_BUFFER < EGL_LUMINANCE_BUFFER);
    SORT(mColorBufferType);

    // By larger total number of color bits, only considering those that are requested to be > 0.
    EGLint xComponentsSize = wantedComponentsSize(x);
    EGLint yComponentsSize = wantedComponentsSize(y);
    if (xComponentsSize != yComponentsSize)
    {
        return xComponentsSize > yComponentsSize;
    }

    SORT(mBufferSize);
    SORT(mSampleBuffers);
    SORT(mSamples);
    SORT(mDepthSize);
    SORT(mStencilSize);
    SORT(mAlphaMaskSize);
    SORT(mNativeVisualType);
    SORT(mConfigID);

    #undef SORT

    return false;
}

// We'd like to use SortConfig to also eliminate duplicate configs.
// This works as long as we never have two configs with different per-RGB-component layouts,
// but the same total.
// 5551 and 565 are different because R+G+B is different.
// 5551 and 555 are different because bufferSize is different.
const EGLint ConfigSet::mSortAttribs[] =
{
    EGL_RED_SIZE, 1,
    EGL_GREEN_SIZE, 1,
    EGL_BLUE_SIZE, 1,
    EGL_LUMINANCE_SIZE, 1,
    // BUT NOT ALPHA
    EGL_NONE
};

ConfigSet::ConfigSet()
    : mSet(SortConfig(mSortAttribs))
{
}

void ConfigSet::add(D3DDISPLAYMODE displayMode, EGLint minSwapInterval, EGLint maxSwapInterval, D3DFORMAT renderTargetFormat, D3DFORMAT depthStencilFormat, EGLint multiSample, EGLint texWidth, EGLint texHeight)
{
    Config config(displayMode, minSwapInterval, maxSwapInterval, renderTargetFormat, depthStencilFormat, multiSample, texWidth, texHeight);

    mSet.insert(config);
}

size_t ConfigSet::size() const
{
    return mSet.size();
}

bool ConfigSet::getConfigs(EGLConfig *configs, const EGLint *attribList, EGLint configSize, EGLint *numConfig)
{
    vector<const Config*> passed;
    passed.reserve(mSet.size());

    for (Iterator config = mSet.begin(); config != mSet.end(); config++)
    {
        bool match = true;
        const EGLint *attribute = attribList;

        while (attribute[0] != EGL_NONE)
        {
            switch (attribute[0])
            {
              case EGL_BUFFER_SIZE:               match = config->mBufferSize >= attribute[1];                      break;
              case EGL_ALPHA_SIZE:                match = config->mAlphaSize >= attribute[1];                       break;
              case EGL_BLUE_SIZE:                 match = config->mBlueSize >= attribute[1];                        break;
              case EGL_GREEN_SIZE:                match = config->mGreenSize >= attribute[1];                       break;
              case EGL_RED_SIZE:                  match = config->mRedSize >= attribute[1];                         break;
              case EGL_DEPTH_SIZE:                match = config->mDepthSize >= attribute[1];                       break;
              case EGL_STENCIL_SIZE:              match = config->mStencilSize >= attribute[1];                     break;
              case EGL_CONFIG_CAVEAT:             match = config->mConfigCaveat == (EGLenum) attribute[1];          break;
              case EGL_CONFIG_ID:                 match = config->mConfigID == attribute[1];                        break;
              case EGL_LEVEL:                     match = config->mLevel >= attribute[1];                           break;
              case EGL_NATIVE_RENDERABLE:         match = config->mNativeRenderable == (EGLBoolean) attribute[1];   break;
              case EGL_NATIVE_VISUAL_TYPE:        match = config->mNativeVisualType == attribute[1];                break;
              case EGL_SAMPLES:                   match = config->mSamples >= attribute[1];                         break;
              case EGL_SAMPLE_BUFFERS:            match = config->mSampleBuffers >= attribute[1];                   break;
              case EGL_SURFACE_TYPE:              match = (config->mSurfaceType & attribute[1]) == attribute[1];    break;
              case EGL_TRANSPARENT_TYPE:          match = config->mTransparentType == (EGLenum) attribute[1];       break;
              case EGL_TRANSPARENT_BLUE_VALUE:    match = config->mTransparentBlueValue == attribute[1];            break;
              case EGL_TRANSPARENT_GREEN_VALUE:   match = config->mTransparentGreenValue == attribute[1];           break;
              case EGL_TRANSPARENT_RED_VALUE:     match = config->mTransparentRedValue == attribute[1];             break;
              case EGL_BIND_TO_TEXTURE_RGB:       match = config->mBindToTextureRGB == (EGLBoolean) attribute[1];   break;
              case EGL_BIND_TO_TEXTURE_RGBA:      match = config->mBindToTextureRGBA == (EGLBoolean) attribute[1];  break;
              case EGL_MIN_SWAP_INTERVAL:         match = config->mMinSwapInterval == attribute[1];                 break;
              case EGL_MAX_SWAP_INTERVAL:         match = config->mMaxSwapInterval == attribute[1];                 break;
              case EGL_LUMINANCE_SIZE:            match = config->mLuminanceSize >= attribute[1];                   break;
              case EGL_ALPHA_MASK_SIZE:           match = config->mAlphaMaskSize >= attribute[1];                   break;
              case EGL_COLOR_BUFFER_TYPE:         match = config->mColorBufferType == (EGLenum) attribute[1];       break;
              case EGL_RENDERABLE_TYPE:           match = (config->mRenderableType & attribute[1]) == attribute[1]; break;
              case EGL_MATCH_NATIVE_PIXMAP:       match = false; UNIMPLEMENTED();                                   break;
              case EGL_CONFORMANT:                match = (config->mConformant & attribute[1]) == attribute[1];     break;
              case EGL_MAX_PBUFFER_WIDTH:         match = config->mMaxPBufferWidth >= attribute[1];                 break;
              case EGL_MAX_PBUFFER_HEIGHT:        match = config->mMaxPBufferHeight >= attribute[1];                break;
              case EGL_MAX_PBUFFER_PIXELS:        match = config->mMaxPBufferPixels >= attribute[1];                break;
              default:
                return false;
            }

            if (!match)
            {
                break;
            }

            attribute += 2;
        }

        if (match)
        {
            passed.push_back(&*config);
        }
    }

    if (configs)
    {
        sort(passed.begin(), passed.end(), SortConfig(attribList));

        EGLint index;
        for (index = 0; index < configSize && index < static_cast<EGLint>(passed.size()); index++)
        {
            configs[index] = passed[index]->getHandle();
        }

        *numConfig = index;
    }
    else
    {
        *numConfig = passed.size();
    }

    return true;
}

const egl::Config *ConfigSet::get(EGLConfig configHandle)
{
    for (Iterator config = mSet.begin(); config != mSet.end(); config++)
    {
        if (config->getHandle() == configHandle)
        {
            return &(*config);
        }
    }

    return NULL;
}
}
