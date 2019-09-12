//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationGL42.cpp: Validation functions for OpenGL 4.2 entry point parameters

#include "libANGLE/validationGL42_autogen.h"

namespace gl
{

bool ValidateDrawArraysInstancedBaseInstance(Context *context,
                                             PrimitiveMode mode,
                                             GLint first,
                                             GLsizei count,
                                             GLsizei instancecount,
                                             GLuint baseinstance)
{
    return true;
}

bool ValidateDrawElementsInstancedBaseInstance(Context *context,
                                               GLenum mode,
                                               GLsizei count,
                                               GLenum type,
                                               const void *indices,
                                               GLsizei instancecount,
                                               GLuint baseinstance)
{
    return true;
}

bool ValidateDrawElementsInstancedBaseVertexBaseInstance(Context *context,
                                                         PrimitiveMode mode,
                                                         GLsizei count,
                                                         DrawElementsType type,
                                                         const void *indices,
                                                         GLsizei instancecount,
                                                         GLint basevertex,
                                                         GLuint baseinstance)
{
    return true;
}

bool ValidateDrawTransformFeedbackInstanced(Context *context,
                                            GLenum mode,
                                            TransformFeedbackID id,
                                            GLsizei instancecount)
{
    return true;
}

bool ValidateDrawTransformFeedbackStreamInstanced(Context *context,
                                                  GLenum mode,
                                                  TransformFeedbackID id,
                                                  GLuint stream,
                                                  GLsizei instancecount)
{
    return true;
}

bool ValidateGetActiveAtomicCounterBufferiv(Context *context,
                                            ShaderProgramID program,
                                            GLuint bufferIndex,
                                            GLenum pname,
                                            GLint *params)
{
    return true;
}

bool ValidateTexStorage1D(Context *context,
                          GLenum target,
                          GLsizei levels,
                          GLenum internalformat,
                          GLsizei width)
{
    return true;
}

}  // namespace gl
