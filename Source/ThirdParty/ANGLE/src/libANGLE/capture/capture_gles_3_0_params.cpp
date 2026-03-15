//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// capture_gles_3_0_params.cpp:
//   Pointer parameter capture functions for the OpenGL ES 3.0 entry points.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/capture/capture_gles_2_0_autogen.h"
#include "libANGLE/capture/capture_gles_3_0_autogen.h"

using namespace angle;

namespace gl
{
void CaptureClearBufferfv_value(const State &glState,
                                GLenum buffer,
                                GLint drawbuffer,
                                const GLfloat *value,
                                ParamCapture *paramCapture)
{
    CaptureClearBufferValue<GLfloat>(buffer, value, paramCapture);
}

void CaptureClearBufferiv_value(const State &glState,
                                GLenum buffer,
                                GLint drawbuffer,
                                const GLint *value,
                                ParamCapture *paramCapture)
{
    CaptureClearBufferValue<GLint>(buffer, value, paramCapture);
}

void CaptureClearBufferuiv_value(const State &glState,
                                 GLenum buffer,
                                 GLint drawbuffer,
                                 const GLuint *value,
                                 ParamCapture *paramCapture)
{
    CaptureClearBufferValue<GLuint>(buffer, value, paramCapture);
}

void CaptureCompressedTexImage3D_data(const State &glState,
                                      TextureTarget targetPacked,
                                      GLint level,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth,
                                      GLint border,
                                      GLsizei imageSize,
                                      const void *data,
                                      ParamCapture *paramCapture)
{
    if (glState.getTargetBuffer(gl::BufferBinding::PixelUnpack))
    {
        return;
    }

    if (!data)
    {
        return;
    }

    CaptureMemory(data, imageSize, paramCapture);
}

void CaptureCompressedTexSubImage3D_data(const State &glState,
                                         TextureTarget targetPacked,
                                         GLint level,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint zoffset,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLenum format,
                                         GLsizei imageSize,
                                         const void *data,
                                         ParamCapture *paramCapture)
{
    CaptureCompressedTexImage3D_data(glState, targetPacked, level, 0, width, height, depth, 0,
                                     imageSize, data, paramCapture);
}

void CaptureDeleteQueries_idsPacked(const State &glState,
                                    GLsizei n,
                                    const QueryID *ids,
                                    ParamCapture *paramCapture)
{
    CaptureArray(ids, n, paramCapture);
}

void CaptureDeleteSamplers_samplersPacked(const State &glState,
                                          GLsizei count,
                                          const SamplerID *samplers,
                                          ParamCapture *paramCapture)
{
    CaptureArray(samplers, count, paramCapture);
}

void CaptureDeleteTransformFeedbacks_idsPacked(const State &glState,
                                               GLsizei n,
                                               const TransformFeedbackID *ids,
                                               ParamCapture *paramCapture)
{
    CaptureArray(ids, n, paramCapture);
}

void CaptureDeleteVertexArrays_arraysPacked(const State &glState,
                                            GLsizei n,
                                            const VertexArrayID *arrays,
                                            ParamCapture *paramCapture)
{
    CaptureArray(arrays, n, paramCapture);
}

void CaptureDrawBuffers_bufs(const State &glState,
                             GLsizei n,
                             const GLenum *bufs,
                             ParamCapture *paramCapture)
{
    CaptureArray(bufs, n, paramCapture);
}

void CaptureDrawElementsInstanced_indices(const State &glState,
                                          PrimitiveMode modePacked,
                                          GLsizei count,
                                          DrawElementsType typePacked,
                                          const void *indices,
                                          GLsizei instancecount,
                                          ParamCapture *paramCapture)
{
    CaptureDrawElements_indices(glState, modePacked, count, typePacked, indices, paramCapture);
}

void CaptureDrawRangeElements_indices(const State &glState,
                                      PrimitiveMode modePacked,
                                      GLuint start,
                                      GLuint end,
                                      GLsizei count,
                                      DrawElementsType typePacked,
                                      const void *indices,
                                      ParamCapture *paramCapture)
{
    CaptureDrawElements_indices(glState, modePacked, count, typePacked, indices, paramCapture);
}

void CaptureGenQueries_idsPacked(const State &glState,
                                 GLsizei n,
                                 QueryID *ids,
                                 ParamCapture *paramCapture)
{
    CaptureGenHandles(n, ids, paramCapture);
}

void CaptureGenSamplers_samplersPacked(const State &glState,
                                       GLsizei count,
                                       SamplerID *samplers,
                                       ParamCapture *paramCapture)
{
    CaptureGenHandles(count, samplers, paramCapture);
}

void CaptureGenTransformFeedbacks_idsPacked(const State &glState,
                                            GLsizei n,
                                            TransformFeedbackID *ids,
                                            ParamCapture *paramCapture)
{
    CaptureGenHandles(n, ids, paramCapture);
}

void CaptureGenVertexArrays_arraysPacked(const State &glState,
                                         GLsizei n,
                                         VertexArrayID *arrays,
                                         ParamCapture *paramCapture)
{
    CaptureGenHandles(n, arrays, paramCapture);
}

void CaptureGetActiveUniformBlockName_length(const State &glState,
                                             ShaderProgramID program,
                                             UniformBlockIndex uniformBlockIndex,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLchar *uniformBlockName,
                                             ParamCapture *paramCapture)
{
    // From the OpenGL ES 3.0 spec:
    // The actual number of characters written into uniformBlockName, excluding the null terminator,
    // is returned in length. If length is NULL, no length is returned.
    if (length)
    {
        paramCapture->readBufferSizeBytes = sizeof(GLsizei);
    }
}

void CaptureGetActiveUniformBlockName_uniformBlockName(const State &glState,
                                                       ShaderProgramID program,
                                                       UniformBlockIndex uniformBlockIndex,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       GLchar *uniformBlockName,
                                                       ParamCapture *paramCapture)
{
    // From the OpenGL ES 3.0 spec:
    // bufSize contains the maximum number of characters (including the null terminator) that will
    // be written back to uniformBlockName.
    CaptureStringLimit(uniformBlockName, bufSize, paramCapture);
}

void CaptureGetActiveUniformBlockiv_params(const State &glState,
                                           ShaderProgramID program,
                                           UniformBlockIndex uniformBlockIndex,
                                           GLenum pname,
                                           GLint *params,
                                           ParamCapture *paramCapture)
{
    CaptureGetActiveUniformBlockivParameters(glState, program, uniformBlockIndex, pname,
                                             paramCapture);
}

void CaptureGetActiveUniformsiv_uniformIndices(const State &glState,
                                               ShaderProgramID program,
                                               GLsizei uniformCount,
                                               const GLuint *uniformIndices,
                                               GLenum pname,
                                               GLint *params,
                                               ParamCapture *paramCapture)
{
    // From the OpenGL ES 3.0 spec:
    // For GetActiveUniformsiv, uniformCountindicates both the number of
    // elements in the array of indices uniformIndices and the number of
    // parameters written to params upon successful return.
    CaptureArray(uniformIndices, uniformCount, paramCapture);
}

void CaptureGetActiveUniformsiv_params(const State &glState,
                                       ShaderProgramID program,
                                       GLsizei uniformCount,
                                       const GLuint *uniformIndices,
                                       GLenum pname,
                                       GLint *params,
                                       ParamCapture *paramCapture)
{
    // From the OpenGL ES 3.0 spec:
    // For GetActiveUniformsiv, uniformCountindicates both the number of
    // elements in the array of indices uniformIndices and the number of
    // parameters written to params upon successful return.
    paramCapture->readBufferSizeBytes = sizeof(GLint) * uniformCount;
}

void CaptureGetBufferParameteri64v_params(const State &glState,
                                          BufferBinding targetPacked,
                                          GLenum pname,
                                          GLint64 *params,
                                          ParamCapture *paramCapture)
{
    // This only returns one value
    paramCapture->readBufferSizeBytes = sizeof(GLuint);
}

void CaptureGetBufferPointerv_params(const State &glState,
                                     BufferBinding targetPacked,
                                     GLenum pname,
                                     void **params,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFragDataLocation_name(const State &glState,
                                     ShaderProgramID program,
                                     const GLchar *name,
                                     ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureGetInteger64i_v_data(const State &glState,
                                 GLenum target,
                                 GLuint index,
                                 GLint64 *data,
                                 ParamCapture *paramCapture)
{
    CaptureGetParameter(glState, target, sizeof(GLint64), paramCapture);
}

void CaptureGetInteger64v_data(const State &glState,
                               GLenum pname,
                               GLint64 *data,
                               ParamCapture *paramCapture)
{
    CaptureGetParameter(glState, pname, sizeof(GLint64), paramCapture);
}

void CaptureGetIntegeri_v_data(const State &glState,
                               GLenum target,
                               GLuint index,
                               GLint *data,
                               ParamCapture *paramCapture)
{
    CaptureGetParameter(glState, target, sizeof(GLint), paramCapture);
}

void CaptureGetInternalformativ_params(const State &glState,
                                       GLenum target,
                                       GLenum internalformat,
                                       GLenum pname,
                                       GLsizei count,
                                       GLint *params,
                                       ParamCapture *paramCapture)
{
    if (params != nullptr)
    {
        // GL_NUM_SAMPLE_COUNTS and GL_NUM_SURFACE_COMPRESSION_FIXED_RATES_EXT
        size_t paramCount = 1;
        switch (pname)
        {
            case GL_SAMPLES:
                // Maximum case: 1, 2, 4, 8, 16, 32 (IMPLEMENTATION_MAX_SAMPLES)
                paramCount = rx::Log2(gl::IMPLEMENTATION_MAX_SAMPLES) + 1;
                break;
            case GL_SURFACE_COMPRESSION_EXT:
                // From SURFACE_COMPRESSION_FIXED_RATE_1BPC_EXT to
                // SURFACE_COMPRESSION_FIXED_RATE_12BPC_EXT
                paramCount = 12;
                break;
        }
        paramCapture->readBufferSizeBytes = sizeof(GLint) * paramCount;
    }
}

void CaptureGetProgramBinary_length(const State &glState,
                                    ShaderProgramID program,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLenum *binaryFormat,
                                    void *binary,
                                    ParamCapture *paramCapture)
{
    if (length)
    {
        paramCapture->readBufferSizeBytes = sizeof(GLsizei);
    }
}

void CaptureGetProgramBinary_binaryFormat(const State &glState,
                                          ShaderProgramID program,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLenum *binaryFormat,
                                          void *binary,
                                          ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLenum);
}

void CaptureGetProgramBinary_binary(const State &glState,
                                    ShaderProgramID program,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLenum *binaryFormat,
                                    void *binary,
                                    ParamCapture *paramCapture)
{
    // If we have length, then actual binarySize was written there
    // Otherwise, we don't know how many bytes were written
    if (!length)
    {
        UNIMPLEMENTED();
        return;
    }

    GLsizei binarySize = *length;
    if (binarySize > bufSize)
    {
        // This is a GL error, but clamp it anyway
        binarySize = bufSize;
    }

    paramCapture->readBufferSizeBytes = binarySize;
}

void CaptureGetQueryObjectuiv_params(const State &glState,
                                     QueryID id,
                                     GLenum pname,
                                     GLuint *params,
                                     ParamCapture *paramCapture)
{
    // This only returns one value
    paramCapture->readBufferSizeBytes = sizeof(GLuint);
}

void CaptureGetQueryiv_params(const State &glState,
                              QueryType targetPacked,
                              GLenum pname,
                              GLint *params,
                              ParamCapture *paramCapture)
{
    // This only returns one value
    paramCapture->readBufferSizeBytes = sizeof(GLint);
}

void CaptureGetSamplerParameterfv_params(const State &glState,
                                         SamplerID sampler,
                                         GLenum pname,
                                         GLfloat *params,
                                         ParamCapture *paramCapture)
{
    // page 458 https://www.khronos.org/registry/OpenGL/specs/es/3.2/es_spec_3.2.pdf
    paramCapture->readBufferSizeBytes = 4 * sizeof(GLfloat);
}

void CaptureGetSamplerParameteriv_params(const State &glState,
                                         SamplerID sampler,
                                         GLenum pname,
                                         GLint *params,
                                         ParamCapture *paramCapture)
{
    // page 458 https://www.khronos.org/registry/OpenGL/specs/es/3.2/es_spec_3.2.pdf
    paramCapture->readBufferSizeBytes = 4 * sizeof(GLint);
}

void CaptureGetSynciv_length(const State &glState,
                             SyncID syncPacked,
                             GLenum pname,
                             GLsizei count,
                             GLsizei *length,
                             GLint *values,
                             ParamCapture *paramCapture)
{
    if (length != nullptr)
    {
        paramCapture->readBufferSizeBytes = sizeof(GLsizei);
    }
}

void CaptureGetSynciv_values(const State &glState,
                             SyncID syncPacked,
                             GLenum pname,
                             GLsizei count,
                             GLsizei *length,
                             GLint *values,
                             ParamCapture *paramCapture)
{
    // All sync parameters return only one value.
    if (values != nullptr)
    {
        paramCapture->readBufferSizeBytes = sizeof(GLint);
    }
}

void CaptureGetTransformFeedbackVarying_length(const State &glState,
                                               ShaderProgramID program,
                                               GLuint index,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLsizei *size,
                                               GLenum *type,
                                               GLchar *name,
                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTransformFeedbackVarying_size(const State &glState,
                                             ShaderProgramID program,
                                             GLuint index,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLsizei *size,
                                             GLenum *type,
                                             GLchar *name,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTransformFeedbackVarying_type(const State &glState,
                                             ShaderProgramID program,
                                             GLuint index,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLsizei *size,
                                             GLenum *type,
                                             GLchar *name,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTransformFeedbackVarying_name(const State &glState,
                                             ShaderProgramID program,
                                             GLuint index,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLsizei *size,
                                             GLenum *type,
                                             GLchar *name,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUniformBlockIndex_uniformBlockName(const State &glState,
                                                  ShaderProgramID program,
                                                  const GLchar *uniformBlockName,
                                                  ParamCapture *paramCapture)
{
    CaptureString(uniformBlockName, paramCapture);
}

void CaptureGetUniformIndices_uniformNames(const State &glState,
                                           ShaderProgramID program,
                                           GLsizei uniformCount,
                                           const GLchar *const *uniformNames,
                                           GLuint *uniformIndices,
                                           ParamCapture *paramCapture)
{
    for (GLsizei index = 0; index < uniformCount; ++index)
    {
        CaptureString(uniformNames[index], paramCapture);
    }
}

void CaptureGetUniformIndices_uniformIndices(const State &glState,
                                             ShaderProgramID program,
                                             GLsizei uniformCount,
                                             const GLchar *const *uniformNames,
                                             GLuint *uniformIndices,
                                             ParamCapture *paramCapture)
{
    CaptureMemory(uniformIndices, sizeof(GLuint) * uniformCount, paramCapture);
}

void CaptureGetUniformuiv_params(const State &glState,
                                 ShaderProgramID program,
                                 UniformLocation location,
                                 GLuint *params,
                                 ParamCapture *paramCapture)
{
    /* At most a mat4 can be returned, so use this upper bound as count */
    CaptureArray(params, 16 * sizeof(GLuint), paramCapture);
}

void CaptureGetVertexAttribIiv_params(const State &glState,
                                      GLuint index,
                                      GLenum pname,
                                      GLint *params,
                                      ParamCapture *paramCapture)
{
    int nParams = pname == GL_CURRENT_VERTEX_ATTRIB ? 4 : 1;

    paramCapture->readBufferSizeBytes = nParams * sizeof(GLint);
}

void CaptureGetVertexAttribIuiv_params(const State &glState,
                                       GLuint index,
                                       GLenum pname,
                                       GLuint *params,
                                       ParamCapture *paramCapture)
{
    int nParams = pname == GL_CURRENT_VERTEX_ATTRIB ? 4 : 1;

    paramCapture->readBufferSizeBytes = nParams * sizeof(GLuint);
}

void CaptureInvalidateFramebuffer_attachments(const State &glState,
                                              GLenum target,
                                              GLsizei numAttachments,
                                              const GLenum *attachments,
                                              ParamCapture *paramCapture)
{
    CaptureMemory(attachments, sizeof(GLenum) * numAttachments, paramCapture);
    paramCapture->value.voidConstPointerVal = paramCapture->data[0].data();
}

void CaptureInvalidateSubFramebuffer_attachments(const State &glState,
                                                 GLenum target,
                                                 GLsizei numAttachments,
                                                 const GLenum *attachments,
                                                 GLint x,
                                                 GLint y,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 ParamCapture *paramCapture)
{
    CaptureMemory(attachments, sizeof(GLenum) * numAttachments, paramCapture);
}

void CaptureProgramBinary_binary(const State &glState,
                                 ShaderProgramID program,
                                 GLenum binaryFormat,
                                 const void *binary,
                                 GLsizei length,
                                 ParamCapture *paramCapture)
{
    // Do nothing. glProgramBinary will be overridden in GenerateLinkedProgram.
}

void CaptureSamplerParameterfv_param(const State &glState,
                                     SamplerID sampler,
                                     GLenum pname,
                                     const GLfloat *param,
                                     ParamCapture *paramCapture)
{
    CaptureTextureAndSamplerParameter_params<GLfloat>(pname, param, paramCapture);
}

void CaptureSamplerParameteriv_param(const State &glState,
                                     SamplerID sampler,
                                     GLenum pname,
                                     const GLint *param,
                                     ParamCapture *paramCapture)
{
    CaptureTextureAndSamplerParameter_params<GLint>(pname, param, paramCapture);
}

void CaptureTexImage3D_pixels(const State &glState,
                              TextureTarget targetPacked,
                              GLint level,
                              GLint internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              GLint border,
                              GLenum format,
                              GLenum type,
                              const void *pixels,
                              ParamCapture *paramCapture)
{
    if (glState.getTargetBuffer(gl::BufferBinding::PixelUnpack))
    {
        return;
    }

    if (!pixels)
    {
        return;
    }

    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(format, type);
    const gl::PixelUnpackState &unpack           = glState.getUnpackState();

    const Extents size(width, height, depth);

    GLuint endByte = 0;
    bool unpackSize =
        internalFormatInfo.computePackUnpackEndByte(type, size, unpack, true, &endByte);
    ASSERT(unpackSize);

    CaptureMemory(pixels, static_cast<size_t>(endByte), paramCapture);
}

void CaptureTexSubImage3D_pixels(const State &glState,
                                 TextureTarget targetPacked,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint zoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLenum type,
                                 const void *pixels,
                                 ParamCapture *paramCapture)
{
    CaptureTexImage3D_pixels(glState, targetPacked, level, 0, width, height, depth, 0, format, type,
                             pixels, paramCapture);
}

void CaptureTransformFeedbackVaryings_varyings(const State &glState,
                                               ShaderProgramID program,
                                               GLsizei count,
                                               const GLchar *const *varyings,
                                               GLenum bufferMode,
                                               ParamCapture *paramCapture)
{
    for (GLsizei index = 0; index < count; ++index)
    {
        CaptureString(varyings[index], paramCapture);
    }
}

void CaptureUniform1uiv_value(const State &glState,
                              UniformLocation location,
                              GLsizei count,
                              const GLuint *value,
                              ParamCapture *paramCapture)
{
    CaptureMemory(value, count * sizeof(GLuint), paramCapture);
}

void CaptureUniform2uiv_value(const State &glState,
                              UniformLocation location,
                              GLsizei count,
                              const GLuint *value,
                              ParamCapture *paramCapture)
{
    CaptureMemory(value, count * sizeof(GLuint) * 2, paramCapture);
}

void CaptureUniform3uiv_value(const State &glState,
                              UniformLocation location,
                              GLsizei count,
                              const GLuint *value,
                              ParamCapture *paramCapture)
{
    CaptureMemory(value, count * sizeof(GLuint) * 3, paramCapture);
}

void CaptureUniform4uiv_value(const State &glState,
                              UniformLocation location,
                              GLsizei count,
                              const GLuint *value,
                              ParamCapture *paramCapture)
{
    CaptureMemory(value, count * sizeof(GLuint) * 4, paramCapture);
}

void CaptureUniformMatrix2x3fv_value(const State &glState,
                                     UniformLocation location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value,
                                     ParamCapture *paramCapture)
{
    CaptureMemory(value, count * sizeof(GLfloat) * 6, paramCapture);
}

void CaptureUniformMatrix2x4fv_value(const State &glState,
                                     UniformLocation location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value,
                                     ParamCapture *paramCapture)
{
    CaptureMemory(value, count * sizeof(GLfloat) * 8, paramCapture);
}

void CaptureUniformMatrix3x2fv_value(const State &glState,
                                     UniformLocation location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value,
                                     ParamCapture *paramCapture)
{
    CaptureMemory(value, count * sizeof(GLfloat) * 6, paramCapture);
}

void CaptureUniformMatrix3x4fv_value(const State &glState,
                                     UniformLocation location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value,
                                     ParamCapture *paramCapture)
{
    CaptureMemory(value, count * sizeof(GLfloat) * 12, paramCapture);
}

void CaptureUniformMatrix4x2fv_value(const State &glState,
                                     UniformLocation location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value,
                                     ParamCapture *paramCapture)
{
    CaptureMemory(value, count * sizeof(GLfloat) * 8, paramCapture);
}

void CaptureUniformMatrix4x3fv_value(const State &glState,
                                     UniformLocation location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat *value,
                                     ParamCapture *paramCapture)
{
    CaptureMemory(value, count * sizeof(GLfloat) * 12, paramCapture);
}

void CaptureVertexAttribI4iv_v(const State &glState,
                               GLuint index,
                               const GLint *v,
                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureVertexAttribI4uiv_v(const State &glState,
                                GLuint index,
                                const GLuint *v,
                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureVertexAttribIPointer_pointer(const State &glState,
                                         GLuint index,
                                         GLint size,
                                         VertexAttribType typePacked,
                                         GLsizei stride,
                                         const void *pointer,
                                         ParamCapture *paramCapture)
{
    CaptureVertexAttribPointer_pointer(glState, index, size, typePacked, false, stride, pointer,
                                       paramCapture);
}

}  // namespace gl
