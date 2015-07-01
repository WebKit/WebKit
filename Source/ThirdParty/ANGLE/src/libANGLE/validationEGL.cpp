//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationEGL.cpp: Validation functions for generic EGL entry point parameters

#include "libANGLE/validationEGL.h"

#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"

#include <EGL/eglext.h>

namespace egl
{

Error ValidateDisplay(const Display *display)
{
    if (display == EGL_NO_DISPLAY)
    {
        return Error(EGL_BAD_DISPLAY);
    }

    if (!display->isInitialized())
    {
        return Error(EGL_NOT_INITIALIZED);
    }

    return Error(EGL_SUCCESS);
}

Error ValidateSurface(const Display *display, Surface *surface)
{
    Error error = ValidateDisplay(display);
    if (error.isError())
    {
        return error;
    }

    if (!display->isValidSurface(surface))
    {
        return Error(EGL_BAD_SURFACE);
    }

    return Error(EGL_SUCCESS);
}

Error ValidateConfig(const Display *display, const Config *config)
{
    Error error = ValidateDisplay(display);
    if (error.isError())
    {
        return error;
    }

    if (!display->isValidConfig(config))
    {
        return Error(EGL_BAD_CONFIG);
    }

    return Error(EGL_SUCCESS);
}

Error ValidateContext(const Display *display, gl::Context *context)
{
    Error error = ValidateDisplay(display);
    if (error.isError())
    {
        return error;
    }

    if (!display->isValidContext(context))
    {
        return Error(EGL_BAD_CONTEXT);
    }

    return Error(EGL_SUCCESS);
}

Error ValidateCreateContext(Display *display, Config *configuration, gl::Context *shareContext,
                            const AttributeMap& attributes)
{
    Error error = ValidateConfig(display, configuration);
    if (error.isError())
    {
        return error;
    }

    // Get the requested client version (default is 1) and check it is 2 or 3.
    EGLint clientMajorVersion = 1;
    EGLint clientMinorVersion = 0;
    EGLint contextFlags = 0;
    bool resetNotification = false;
    bool robustAccess = false;
    for (AttributeMap::const_iterator attributeIter = attributes.begin(); attributeIter != attributes.end(); attributeIter++)
    {
        EGLint attribute = attributeIter->first;
        EGLint value = attributeIter->second;

        switch (attribute)
        {
          case EGL_CONTEXT_CLIENT_VERSION:
            clientMajorVersion = value;
            break;

          case EGL_CONTEXT_MINOR_VERSION:
            clientMinorVersion = value;
            break;

          case EGL_CONTEXT_FLAGS_KHR:
            contextFlags = value;
            break;

          case EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR:
            // Only valid for OpenGL (non-ES) contexts
            return Error(EGL_BAD_ATTRIBUTE);

          case EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT:
            if (!display->getExtensions().createContextRobustness)
            {
                return Error(EGL_BAD_ATTRIBUTE);
            }
            if (value != EGL_TRUE && value != EGL_FALSE)
            {
                return Error(EGL_BAD_ATTRIBUTE);
            }
            robustAccess = (value == EGL_TRUE);
            break;

          case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR:
            static_assert(EGL_LOSE_CONTEXT_ON_RESET_EXT == EGL_LOSE_CONTEXT_ON_RESET_KHR, "EGL extension enums not equal.");
            static_assert(EGL_NO_RESET_NOTIFICATION_EXT == EGL_NO_RESET_NOTIFICATION_KHR, "EGL extension enums not equal.");
            // same as EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT, fall through
          case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT:
            if (!display->getExtensions().createContextRobustness)
            {
                return Error(EGL_BAD_ATTRIBUTE);
            }
            if (value == EGL_LOSE_CONTEXT_ON_RESET_EXT)
            {
                resetNotification = true;
            }
            else if (value != EGL_NO_RESET_NOTIFICATION_EXT)
            {
                return Error(EGL_BAD_ATTRIBUTE);
            }
            break;

          default:
            return Error(EGL_BAD_ATTRIBUTE);
        }
    }

    if ((clientMajorVersion != 2 && clientMajorVersion != 3) || clientMinorVersion != 0)
    {
        return Error(EGL_BAD_CONFIG);
    }

    if (clientMajorVersion == 3 && !(configuration->conformant & EGL_OPENGL_ES3_BIT_KHR))
    {
        return Error(EGL_BAD_CONFIG);
    }

    // Note: EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR does not apply to ES
    const EGLint validContextFlags = (EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR |
                                      EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR);
    if ((contextFlags & ~validContextFlags) != 0)
    {
        return Error(EGL_BAD_ATTRIBUTE);
    }

    if ((contextFlags & EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR) > 0)
    {
        robustAccess = true;
    }

    if (robustAccess)
    {
        // Unimplemented
        return Error(EGL_BAD_CONFIG);
    }

    if (shareContext)
    {
        // Shared context is invalid or is owned by another display
        if (!display->isValidContext(shareContext))
        {
            return Error(EGL_BAD_MATCH);
        }

        if (shareContext->isResetNotificationEnabled() != resetNotification)
        {
            return Error(EGL_BAD_MATCH);
        }

        if (shareContext->getClientVersion() != clientMajorVersion)
        {
            return Error(EGL_BAD_CONTEXT);
        }
    }

    return Error(EGL_SUCCESS);
}

Error ValidateCreateWindowSurface(Display *display, Config *config, EGLNativeWindowType window,
                                  const AttributeMap& attributes)
{
    Error error = ValidateConfig(display, config);
    if (error.isError())
    {
        return error;
    }

    if (!display->isValidNativeWindow(window))
    {
        return Error(EGL_BAD_NATIVE_WINDOW);
    }

    const DisplayExtensions &displayExtensions = display->getExtensions();

    for (AttributeMap::const_iterator attributeIter = attributes.begin(); attributeIter != attributes.end(); attributeIter++)
    {
        EGLint attribute = attributeIter->first;
        EGLint value = attributeIter->second;

        switch (attribute)
        {
          case EGL_RENDER_BUFFER:
            switch (value)
            {
              case EGL_BACK_BUFFER:
                break;
              case EGL_SINGLE_BUFFER:
                return Error(EGL_BAD_MATCH);   // Rendering directly to front buffer not supported
              default:
                return Error(EGL_BAD_ATTRIBUTE);
            }
            break;

          case EGL_POST_SUB_BUFFER_SUPPORTED_NV:
            if (!displayExtensions.postSubBuffer)
            {
                return Error(EGL_BAD_ATTRIBUTE);
            }
            break;

          case EGL_WIDTH:
          case EGL_HEIGHT:
            if (!displayExtensions.windowFixedSize)
            {
                return Error(EGL_BAD_ATTRIBUTE);
            }
            if (value < 0)
            {
                return Error(EGL_BAD_PARAMETER);
            }
            break;

          case EGL_FIXED_SIZE_ANGLE:
            if (!displayExtensions.windowFixedSize)
            {
                return Error(EGL_BAD_ATTRIBUTE);
            }
            break;

          case EGL_VG_COLORSPACE:
            return Error(EGL_BAD_MATCH);

          case EGL_VG_ALPHA_FORMAT:
            return Error(EGL_BAD_MATCH);

          default:
            return Error(EGL_BAD_ATTRIBUTE);
        }
    }

    if (Display::hasExistingWindowSurface(window))
    {
        return Error(EGL_BAD_ALLOC);
    }

    return Error(EGL_SUCCESS);
}

Error ValidateCreatePbufferSurface(Display *display, Config *config, const AttributeMap& attributes)
{
    Error error = ValidateConfig(display, config);
    if (error.isError())
    {
        return error;
    }
    
    for (AttributeMap::const_iterator attributeIter = attributes.begin(); attributeIter != attributes.end(); attributeIter++)
    {
        EGLint attribute = attributeIter->first;
        EGLint value = attributeIter->second;

        switch (attribute)
        {
          case EGL_WIDTH:
          case EGL_HEIGHT:
            if (value < 0)
            {
                return Error(EGL_BAD_PARAMETER);
            }
            break;

          case EGL_LARGEST_PBUFFER:
            break;

          case EGL_TEXTURE_FORMAT:
            switch (value)
            {
              case EGL_NO_TEXTURE:
              case EGL_TEXTURE_RGB:
              case EGL_TEXTURE_RGBA:
                break;
              default:
                return Error(EGL_BAD_ATTRIBUTE);
            }
            break;

          case EGL_TEXTURE_TARGET:
            switch (value)
            {
              case EGL_NO_TEXTURE:
              case EGL_TEXTURE_2D:
                break;
              default:
                return Error(EGL_BAD_ATTRIBUTE);
            }
            break;

          case EGL_MIPMAP_TEXTURE:
            break;

          case EGL_VG_COLORSPACE:
            break;

          case EGL_VG_ALPHA_FORMAT:
            break;

          default:
            return Error(EGL_BAD_ATTRIBUTE);
        }
    }

    if (!(config->surfaceType & EGL_PBUFFER_BIT))
    {
        return Error(EGL_BAD_MATCH);
    }

    const Caps &caps = display->getCaps();

    EGLenum textureFormat = attributes.get(EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE);
    EGLenum textureTarget = attributes.get(EGL_TEXTURE_TARGET, EGL_NO_TEXTURE);

    if ((textureFormat != EGL_NO_TEXTURE && textureTarget == EGL_NO_TEXTURE) ||
        (textureFormat == EGL_NO_TEXTURE && textureTarget != EGL_NO_TEXTURE))
    {
        return Error(EGL_BAD_MATCH);
    }

    if ((textureFormat == EGL_TEXTURE_RGB  && config->bindToTextureRGB != EGL_TRUE) ||
        (textureFormat == EGL_TEXTURE_RGBA && config->bindToTextureRGBA != EGL_TRUE))
    {
        return Error(EGL_BAD_ATTRIBUTE);
    }

    EGLint width = attributes.get(EGL_WIDTH, 0);
    EGLint height = attributes.get(EGL_HEIGHT, 0);
    if (textureFormat != EGL_NO_TEXTURE && !caps.textureNPOT && (!gl::isPow2(width) || !gl::isPow2(height)))
    {
        return Error(EGL_BAD_MATCH);
    }

    return Error(EGL_SUCCESS);
}

Error ValidateCreatePbufferFromClientBuffer(Display *display, EGLenum buftype, EGLClientBuffer buffer,
                                            Config *config, const AttributeMap& attributes)
{
    Error error = ValidateConfig(display, config);
    if (error.isError())
    {
        return error;
    }

    const DisplayExtensions &displayExtensions = display->getExtensions();

    switch (buftype)
    {
      case EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE:
        if (!displayExtensions.d3dShareHandleClientBuffer)
        {
            return Error(EGL_BAD_PARAMETER);
        }
        if (buffer == nullptr)
        {
            return Error(EGL_BAD_PARAMETER);
        }
        break;

      default:
        return Error(EGL_BAD_PARAMETER);
    }

    for (AttributeMap::const_iterator attributeIter = attributes.begin(); attributeIter != attributes.end(); attributeIter++)
    {
        EGLint attribute = attributeIter->first;
        EGLint value = attributeIter->second;

        switch (attribute)
        {
          case EGL_WIDTH:
          case EGL_HEIGHT:
            if (!displayExtensions.d3dShareHandleClientBuffer)
            {
                return Error(EGL_BAD_PARAMETER);
            }
            if (value < 0)
            {
                return Error(EGL_BAD_PARAMETER);
            }
            break;

          case EGL_TEXTURE_FORMAT:
            switch (value)
            {
              case EGL_NO_TEXTURE:
              case EGL_TEXTURE_RGB:
              case EGL_TEXTURE_RGBA:
                break;
              default:
                return Error(EGL_BAD_ATTRIBUTE);
            }
            break;

          case EGL_TEXTURE_TARGET:
            switch (value)
            {
              case EGL_NO_TEXTURE:
              case EGL_TEXTURE_2D:
                break;
              default:
                return Error(EGL_BAD_ATTRIBUTE);
            }
            break;

          case EGL_MIPMAP_TEXTURE:
            break;

          default:
            return Error(EGL_BAD_ATTRIBUTE);
        }
    }

    if (!(config->surfaceType & EGL_PBUFFER_BIT))
    {
        return Error(EGL_BAD_MATCH);
    }

    EGLenum textureFormat = attributes.get(EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE);
    EGLenum textureTarget = attributes.get(EGL_TEXTURE_TARGET, EGL_NO_TEXTURE);
    if ((textureFormat != EGL_NO_TEXTURE && textureTarget == EGL_NO_TEXTURE) ||
        (textureFormat == EGL_NO_TEXTURE && textureTarget != EGL_NO_TEXTURE))
    {
        return Error(EGL_BAD_MATCH);
    }

    if ((textureFormat == EGL_TEXTURE_RGB  && config->bindToTextureRGB  != EGL_TRUE) ||
        (textureFormat == EGL_TEXTURE_RGBA && config->bindToTextureRGBA != EGL_TRUE))
    {
        return Error(EGL_BAD_ATTRIBUTE);
    }

    if (buftype == EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE)
    {
        EGLint width = attributes.get(EGL_WIDTH, 0);
        EGLint height = attributes.get(EGL_HEIGHT, 0);

        if (width == 0 || height == 0)
        {
            return Error(EGL_BAD_ATTRIBUTE);
        }

        const Caps &caps = display->getCaps();
        if (textureFormat != EGL_NO_TEXTURE && !caps.textureNPOT && (!gl::isPow2(width) || !gl::isPow2(height)))
        {
            return Error(EGL_BAD_MATCH);
        }
    }

    return Error(EGL_SUCCESS);
}

}
