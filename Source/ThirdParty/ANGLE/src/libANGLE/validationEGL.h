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

namespace gl
{
class Context;
}

namespace egl
{

class AttributeMap;
struct Config;
class Display;
class Surface;

// Object validation
Error ValidateDisplay(const Display *display);
Error ValidateSurface(const Display *display, Surface *surface);
Error ValidateConfig(const Display *display, const Config *config);
Error ValidateContext(const Display *display, gl::Context *context);

// Entry point validation
Error ValidateCreateContext(Display *display, Config *configuration, gl::Context *shareContext,
                            const AttributeMap& attributes);

Error ValidateCreateWindowSurface(Display *display, Config *config, EGLNativeWindowType window,
                                  const AttributeMap& attributes);

Error ValidateCreatePbufferSurface(Display *display, Config *config, const AttributeMap& attributes);
Error ValidateCreatePbufferFromClientBuffer(Display *display, EGLenum buftype, EGLClientBuffer buffer,
                                            Config *config, const AttributeMap& attributes);


}

#endif // LIBANGLE_VALIDATIONEGL_H_
