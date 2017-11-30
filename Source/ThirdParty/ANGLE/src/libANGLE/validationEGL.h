//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationEGL.h: Validation functions for generic EGL entry point parameters

#ifndef LIBANGLE_VALIDATIONEGL_H_
#define LIBANGLE_VALIDATIONEGL_H_

#include "libANGLE/Error.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace gl
{
class Context;
}

namespace egl
{

class AttributeMap;
struct ClientExtensions;
struct Config;
class Device;
class Display;
class Image;
class Stream;
class Surface;

// Object validation
Error ValidateDisplay(const Display *display);
Error ValidateSurface(const Display *display, const Surface *surface);
Error ValidateConfig(const Display *display, const Config *config);
Error ValidateContext(const Display *display, const gl::Context *context);
Error ValidateImage(const Display *display, const Image *image);

// Entry point validation
Error ValidateCreateContext(Display *display, Config *configuration, gl::Context *shareContext,
                            const AttributeMap& attributes);

Error ValidateCreateWindowSurface(Display *display, Config *config, EGLNativeWindowType window,
                                  const AttributeMap& attributes);

Error ValidateCreatePbufferSurface(Display *display, Config *config, const AttributeMap& attributes);
Error ValidateCreatePbufferFromClientBuffer(Display *display, EGLenum buftype, EGLClientBuffer buffer,
                                            Config *config, const AttributeMap& attributes);

Error ValidateMakeCurrent(Display *display, EGLSurface draw, EGLSurface read, gl::Context *context);

Error ValidateCreateImageKHR(const Display *display,
                             gl::Context *context,
                             EGLenum target,
                             EGLClientBuffer buffer,
                             const AttributeMap &attributes);
Error ValidateDestroyImageKHR(const Display *display, const Image *image);

Error ValidateCreateDeviceANGLE(EGLint device_type,
                                void *native_device,
                                const EGLAttrib *attrib_list);
Error ValidateReleaseDeviceANGLE(Device *device);

Error ValidateCreateStreamKHR(const Display *display, const AttributeMap &attributes);
Error ValidateDestroyStreamKHR(const Display *display, const Stream *stream);
Error ValidateStreamAttribKHR(const Display *display,
                              const Stream *stream,
                              EGLint attribute,
                              EGLint value);
Error ValidateQueryStreamKHR(const Display *display,
                             const Stream *stream,
                             EGLenum attribute,
                             EGLint *value);
Error ValidateQueryStreamu64KHR(const Display *display,
                                const Stream *stream,
                                EGLenum attribute,
                                EGLuint64KHR *value);
Error ValidateStreamConsumerGLTextureExternalKHR(const Display *display,
                                                 gl::Context *context,
                                                 const Stream *stream);
Error ValidateStreamConsumerAcquireKHR(const Display *display,
                                       gl::Context *context,
                                       const Stream *stream);
Error ValidateStreamConsumerReleaseKHR(const Display *display,
                                       gl::Context *context,
                                       const Stream *stream);
Error ValidateStreamConsumerGLTextureExternalAttribsNV(const Display *display,
                                                       gl::Context *context,
                                                       const Stream *stream,
                                                       const AttributeMap &attribs);
Error ValidateCreateStreamProducerD3DTextureNV12ANGLE(const Display *display,
                                                      const Stream *stream,
                                                      const AttributeMap &attribs);
Error ValidateStreamPostD3DTextureNV12ANGLE(const Display *display,
                                            const Stream *stream,
                                            void *texture,
                                            const AttributeMap &attribs);

Error ValidateGetSyncValuesCHROMIUM(const Display *display,
                                    const Surface *surface,
                                    const EGLuint64KHR *ust,
                                    const EGLuint64KHR *msc,
                                    const EGLuint64KHR *sbc);

Error ValidateSwapBuffersWithDamageEXT(const Display *display,
                                       const Surface *surface,
                                       EGLint *rects,
                                       EGLint n_rects);

Error ValidateGetConfigAttrib(const Display *display, const Config *config, EGLint attribute);
Error ValidateChooseConfig(const Display *display,
                           const AttributeMap &attribs,
                           EGLint configSize,
                           EGLint *numConfig);
Error ValidateGetConfigs(const Display *display, EGLint configSize, EGLint *numConfig);

// Other validation
Error ValidateCompatibleConfigs(const Display *display,
                                const Config *config1,
                                const Surface *surface,
                                const Config *config2,
                                EGLint surfaceType);

Error ValidateGetPlatformDisplay(EGLenum platform,
                                 void *native_display,
                                 const EGLAttrib *attrib_list);
Error ValidateGetPlatformDisplayEXT(EGLenum platform,
                                    void *native_display,
                                    const EGLint *attrib_list);

Error ValidateProgramCacheGetAttribANGLE(const Display *display, EGLenum attrib);

Error ValidateProgramCacheQueryANGLE(const Display *display,
                                     EGLint index,
                                     void *key,
                                     EGLint *keysize,
                                     void *binary,
                                     EGLint *binarysize);

Error ValidateProgramCachePopulateANGLE(const Display *display,
                                        const void *key,
                                        EGLint keysize,
                                        const void *binary,
                                        EGLint binarysize);

Error ValidateProgramCacheResizeANGLE(const Display *display, EGLint limit, EGLenum mode);

Error ValidateSurfaceAttrib(const Display *display,
                            const Surface *surface,
                            EGLint attribute,
                            EGLint value);
Error ValidateQuerySurface(const Display *display,
                           const Surface *surface,
                           EGLint attribute,
                           EGLint *value);
Error ValidateQueryContext(const Display *display,
                           const gl::Context *context,
                           EGLint attribute,
                           EGLint *value);

}  // namespace egl

#define ANGLE_EGL_TRY(THREAD, EXPR)                   \
    {                                                 \
        auto ANGLE_LOCAL_VAR = (EXPR);                \
        if (ANGLE_LOCAL_VAR.isError())                \
            return THREAD->setError(ANGLE_LOCAL_VAR); \
    }

#define ANGLE_EGL_TRY_RETURN(THREAD, EXPR, RETVAL) \
    {                                              \
        auto ANGLE_LOCAL_VAR = (EXPR);             \
        if (ANGLE_LOCAL_VAR.isError())             \
        {                                          \
            THREAD->setError(ANGLE_LOCAL_VAR);     \
            return RETVAL;                         \
        }                                          \
    }

#endif // LIBANGLE_VALIDATIONEGL_H_
