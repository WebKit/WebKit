//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL32.cpp: Validation functions for OpenGL 3.2 entry point parameters

#include "libANGLE/validationGL32_autogen.h"

namespace gl
{

bool ValidateMultiDrawElementsBaseVertex(Context *context,
                                         PrimitiveMode mode,
                                         const GLsizei *count,
                                         DrawElementsType type,
                                         const void *const *indices,
                                         GLsizei drawcount,
                                         const GLint *basevertex)
{
    return true;
}

bool ValidateProvokingVertex(Context *context, ProvokingVertexConvention modePacked)
{
    return true;
}

bool ValidateTexImage2DMultisample(Context *context,
                                   GLenum target,
                                   GLsizei samples,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLboolean fixedsamplelocations)
{
    return true;
}

bool ValidateTexImage3DMultisample(Context *context,
                                   GLenum target,
                                   GLsizei samples,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLboolean fixedsamplelocations)
{
    return true;
}

}  // namespace gl
