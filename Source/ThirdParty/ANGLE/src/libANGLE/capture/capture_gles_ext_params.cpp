//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// capture_gles_ext_params.cpp:
//   Pointer parameter capture functions for the OpenGL ES extension entry points.

#include "libANGLE/capture/capture_gles_ext_autogen.h"

#include "libANGLE/capture/capture_gles_2_0_autogen.h"
#include "libANGLE/capture/capture_gles_3_0_autogen.h"
#include "libANGLE/capture/capture_gles_3_2_autogen.h"

using namespace angle;

namespace gl
{

// GL_AMD_performance_monitor
void CaptureDeletePerfMonitorsAMD_monitors(const State &glState,
                                           GLsizei n,
                                           GLuint *monitors,
                                           angle::ParamCapture *paramCapture)
{
    CaptureArray(monitors, n, paramCapture);
}

void CaptureGenPerfMonitorsAMD_monitors(const State &glState,
                                        GLsizei n,
                                        GLuint *monitors,
                                        angle::ParamCapture *paramCapture)
{
    CaptureArray(monitors, n, paramCapture);
}

void CaptureGetPerfMonitorCounterDataAMD_data(const State &glState,
                                              GLuint monitor,
                                              GLenum pname,
                                              GLsizei dataSize,
                                              GLuint *data,
                                              GLint *bytesWritten,
                                              angle::ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = dataSize;
}

void CaptureGetPerfMonitorCounterDataAMD_bytesWritten(const State &glState,
                                                      GLuint monitor,
                                                      GLenum pname,
                                                      GLsizei dataSize,
                                                      GLuint *data,
                                                      GLint *bytesWritten,
                                                      angle::ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLint);
}

void CaptureGetPerfMonitorCounterInfoAMD_data(const State &glState,
                                              GLuint group,
                                              GLuint counter,
                                              GLenum pname,
                                              void *data,
                                              angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetPerfMonitorCounterStringAMD_length(const State &glState,
                                                  GLuint group,
                                                  GLuint counter,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLchar *counterString,
                                                  angle::ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
}

void CaptureGetPerfMonitorCounterStringAMD_counterString(const State &glState,
                                                         GLuint group,
                                                         GLuint counter,
                                                         GLsizei bufSize,
                                                         GLsizei *length,
                                                         GLchar *counterString,
                                                         angle::ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = bufSize;
}

void CaptureGetPerfMonitorCountersAMD_numCounters(const State &glState,
                                                  GLuint group,
                                                  GLint *numCounters,
                                                  GLint *maxActiveCounters,
                                                  GLsizei counterSize,
                                                  GLuint *counters,
                                                  angle::ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLint);
}

void CaptureGetPerfMonitorCountersAMD_maxActiveCounters(const State &glState,
                                                        GLuint group,
                                                        GLint *numCounters,
                                                        GLint *maxActiveCounters,
                                                        GLsizei counterSize,
                                                        GLuint *counters,
                                                        angle::ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLint);
}

void CaptureGetPerfMonitorCountersAMD_counters(const State &glState,
                                               GLuint group,
                                               GLint *numCounters,
                                               GLint *maxActiveCounters,
                                               GLsizei counterSize,
                                               GLuint *counters,
                                               angle::ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = counterSize * sizeof(GLuint);
}

void CaptureGetPerfMonitorGroupStringAMD_length(const State &glState,
                                                GLuint group,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLchar *groupString,
                                                angle::ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
}

void CaptureGetPerfMonitorGroupStringAMD_groupString(const State &glState,
                                                     GLuint group,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLchar *groupString,
                                                     angle::ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = bufSize;
}

void CaptureGetPerfMonitorGroupsAMD_numGroups(const State &glState,
                                              GLint *numGroups,
                                              GLsizei groupsSize,
                                              GLuint *groups,
                                              angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetPerfMonitorGroupsAMD_groups(const State &glState,
                                           GLint *numGroups,
                                           GLsizei groupsSize,
                                           GLuint *groups,
                                           angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSelectPerfMonitorCountersAMD_counterList(const State &glState,
                                                     GLuint monitor,
                                                     GLboolean enable,
                                                     GLuint group,
                                                     GLint numCounters,
                                                     GLuint *counterList,
                                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_ANGLE_base_vertex_base_instance
void CaptureDrawElementsInstancedBaseVertexBaseInstanceANGLE_indices(
    const State &glState,
    PrimitiveMode modePacked,
    GLsizei count,
    DrawElementsType typePacked,
    const GLvoid *indices,
    GLsizei instanceCount,
    GLint baseVertex,
    GLuint baseInstance,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysInstancedBaseInstanceANGLE_firsts(const State &glState,
                                                             PrimitiveMode modePacked,
                                                             const GLint *firsts,
                                                             const GLsizei *counts,
                                                             const GLsizei *instanceCounts,
                                                             const GLuint *baseInstances,
                                                             GLsizei drawcount,
                                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysInstancedBaseInstanceANGLE_counts(const State &glState,
                                                             PrimitiveMode modePacked,
                                                             const GLint *firsts,
                                                             const GLsizei *counts,
                                                             const GLsizei *instanceCounts,
                                                             const GLuint *baseInstances,
                                                             GLsizei drawcount,
                                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysInstancedBaseInstanceANGLE_instanceCounts(
    const State &glState,
    PrimitiveMode modePacked,
    const GLint *firsts,
    const GLsizei *counts,
    const GLsizei *instanceCounts,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysInstancedBaseInstanceANGLE_baseInstances(
    const State &glState,
    PrimitiveMode modePacked,
    const GLint *firsts,
    const GLsizei *counts,
    const GLsizei *instanceCounts,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE_counts(
    const State &glState,
    PrimitiveMode modePacked,
    const GLsizei *counts,
    DrawElementsType typePacked,
    const GLvoid *const *indices,
    const GLsizei *instanceCounts,
    const GLint *baseVertices,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE_indices(
    const State &glState,
    PrimitiveMode modePacked,
    const GLsizei *counts,
    DrawElementsType typePacked,
    const GLvoid *const *indices,
    const GLsizei *instanceCounts,
    const GLint *baseVertices,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE_instanceCounts(
    const State &glState,
    PrimitiveMode modePacked,
    const GLsizei *counts,
    DrawElementsType typePacked,
    const GLvoid *const *indices,
    const GLsizei *instanceCounts,
    const GLint *baseVertices,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE_baseVertices(
    const State &glState,
    PrimitiveMode modePacked,
    const GLsizei *counts,
    DrawElementsType typePacked,
    const GLvoid *const *indices,
    const GLsizei *instanceCounts,
    const GLint *baseVertices,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedBaseVertexBaseInstanceANGLE_baseInstances(
    const State &glState,
    PrimitiveMode modePacked,
    const GLsizei *counts,
    DrawElementsType typePacked,
    const GLvoid *const *indices,
    const GLsizei *instanceCounts,
    const GLint *baseVertices,
    const GLuint *baseInstances,
    GLsizei drawcount,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_ANGLE_blob_cache
void CaptureBlobCacheCallbacksANGLE_userParam(const State &glState,
                                              GLSETBLOBPROCANGLE set,
                                              GLGETBLOBPROCANGLE get,
                                              const void *userParam,
                                              angle::ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetPointervANGLE_params(const State &glState,
                                    GLenum pname,
                                    void **params,
                                    angle::ParamCapture *paramCapture)
{
    // Skipped
}

// GL_ANGLE_get_image
void CaptureGetTexImageANGLE_pixels(const State &glState,
                                    TextureTarget target,
                                    GLint level,
                                    GLenum format,
                                    GLenum type,
                                    void *pixels,
                                    angle::ParamCapture *paramCapture)
{
    if (glState.getTargetBuffer(gl::BufferBinding::PixelPack))
    {
        // If a pixel pack buffer is bound, this is an offset, not a pointer
        paramCapture->value.voidPointerVal = pixels;
        return;
    }

    const Texture *texture = glState.getTargetTexture(TextureTargetToType(target));
    ASSERT(texture);

    // Use a conservative upper bound instead of an exact size to be simple.
    static constexpr GLsizei kMaxPixelSize = 32;
    size_t width                           = texture->getWidth(target, level);
    size_t height                          = texture->getHeight(target, level);
    size_t depth                           = texture->getDepth(target, level);
    paramCapture->readBufferSizeBytes      = kMaxPixelSize * width * height * depth;
}

void CaptureGetCompressedTexImageANGLE_pixels(const State &glState,
                                              TextureTarget target,
                                              GLint level,
                                              void *pixels,
                                              angle::ParamCapture *paramCapture)
{
    const Texture *texture = glState.getTargetTexture(TextureTargetToType(target));
    ASSERT(texture);
    const gl::InternalFormat *formatInfo = texture->getFormat(target, level).info;
    const gl::Extents &levelExtents      = texture->getExtents(target, level);

    GLuint size;
    bool result = formatInfo->computeCompressedImageSize(levelExtents, &size);
    ASSERT(result);
    paramCapture->readBufferSizeBytes = size;
}

void CaptureGetRenderbufferImageANGLE_pixels(const State &glState,
                                             GLenum target,
                                             GLenum format,
                                             GLenum type,
                                             void *pixels,
                                             angle::ParamCapture *paramCapture)
{
    if (glState.getTargetBuffer(gl::BufferBinding::PixelPack))
    {
        // If a pixel pack buffer is bound, this is an offset, not a pointer
        paramCapture->value.voidPointerVal = pixels;
        return;
    }

    const Renderbuffer *renderbuffer = glState.getCurrentRenderbuffer();
    ASSERT(renderbuffer);

    // Use a conservative upper bound instead of an exact size to be simple.
    static constexpr GLsizei kMaxPixelSize = 32;
    size_t width                           = renderbuffer->getWidth();
    size_t height                          = renderbuffer->getHeight();
    paramCapture->readBufferSizeBytes      = kMaxPixelSize * width * height;
}

// GL_ANGLE_get_tex_level_parameter
void CaptureGetTexLevelParameterivANGLE_params(const State &glState,
                                               TextureTarget targetPacked,
                                               GLint level,
                                               GLenum pname,
                                               GLint *params,
                                               ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLint);
}

void CaptureGetTexLevelParameterfvANGLE_params(const State &glState,
                                               TextureTarget targetPacked,
                                               GLint level,
                                               GLenum pname,
                                               GLfloat *params,
                                               ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLfloat);
}

// GL_ANGLE_instanced_arrays
void CaptureDrawElementsInstancedANGLE_indices(const State &glState,
                                               PrimitiveMode modePacked,
                                               GLsizei count,
                                               DrawElementsType typePacked,
                                               const void *indices,
                                               GLsizei primcount,
                                               ParamCapture *paramCapture)
{
    CaptureDrawElements_indices(glState, modePacked, count, typePacked, indices, paramCapture);
}

// GL_ANGLE_memory_object_flags
void CaptureTexStorageMemFlags2DANGLE_imageCreateInfoPNext(const State &glState,
                                                           TextureType targetPacked,
                                                           GLsizei levels,
                                                           GLenum internalFormat,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           MemoryObjectID memoryPacked,
                                                           GLuint64 offset,
                                                           GLbitfield createFlags,
                                                           GLbitfield usageFlags,
                                                           const void *imageCreateInfoPNext,
                                                           angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexStorageMemFlags2DMultisampleANGLE_imageCreateInfoPNext(
    const State &glState,
    TextureType targetPacked,
    GLsizei samples,
    GLenum internalFormat,
    GLsizei width,
    GLsizei height,
    GLboolean fixedSampleLocations,
    MemoryObjectID memoryPacked,
    GLuint64 offset,
    GLbitfield createFlags,
    GLbitfield usageFlags,
    const void *imageCreateInfoPNext,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexStorageMemFlags3DANGLE_imageCreateInfoPNext(const State &glState,
                                                           TextureType targetPacked,
                                                           GLsizei levels,
                                                           GLenum internalFormat,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLsizei depth,
                                                           MemoryObjectID memoryPacked,
                                                           GLuint64 offset,
                                                           GLbitfield createFlags,
                                                           GLbitfield usageFlags,
                                                           const void *imageCreateInfoPNext,
                                                           angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexStorageMemFlags3DMultisampleANGLE_imageCreateInfoPNext(
    const State &glState,
    TextureType targetPacked,
    GLsizei samples,
    GLenum internalFormat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLboolean fixedSampleLocations,
    MemoryObjectID memoryPacked,
    GLuint64 offset,
    GLbitfield createFlags,
    GLbitfield usageFlags,
    const void *imageCreateInfoPNext,
    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_ANGLE_multi_draw
void CaptureMultiDrawArraysANGLE_firsts(const State &glState,
                                        PrimitiveMode modePacked,
                                        const GLint *firsts,
                                        const GLsizei *counts,
                                        GLsizei drawcount,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysANGLE_counts(const State &glState,
                                        PrimitiveMode modePacked,
                                        const GLint *firsts,
                                        const GLsizei *counts,
                                        GLsizei drawcount,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysInstancedANGLE_firsts(const State &glState,
                                                 PrimitiveMode modePacked,
                                                 const GLint *firsts,
                                                 const GLsizei *counts,
                                                 const GLsizei *instanceCounts,
                                                 GLsizei drawcount,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysInstancedANGLE_counts(const State &glState,
                                                 PrimitiveMode modePacked,
                                                 const GLint *firsts,
                                                 const GLsizei *counts,
                                                 const GLsizei *instanceCounts,
                                                 GLsizei drawcount,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysInstancedANGLE_instanceCounts(const State &glState,
                                                         PrimitiveMode modePacked,
                                                         const GLint *firsts,
                                                         const GLsizei *counts,
                                                         const GLsizei *instanceCounts,
                                                         GLsizei drawcount,
                                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsANGLE_counts(const State &glState,
                                          PrimitiveMode modePacked,
                                          const GLsizei *counts,
                                          DrawElementsType typePacked,
                                          const GLvoid *const *indices,
                                          GLsizei drawcount,
                                          ParamCapture *paramCapture)
{
    CaptureArray(counts, drawcount, paramCapture);
}

void CaptureMultiDrawElementsANGLE_indices(const State &glState,
                                           PrimitiveMode modePacked,
                                           const GLsizei *counts,
                                           DrawElementsType typePacked,
                                           const GLvoid *const *indices,
                                           GLsizei drawcount,
                                           ParamCapture *paramCapture)
{
    CaptureArray(indices, drawcount, paramCapture);
}

void CaptureMultiDrawElementsInstancedANGLE_counts(const State &glState,
                                                   PrimitiveMode modePacked,
                                                   const GLsizei *counts,
                                                   DrawElementsType typePacked,
                                                   const GLvoid *const *indices,
                                                   const GLsizei *instanceCounts,
                                                   GLsizei drawcount,
                                                   ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedANGLE_indices(const State &glState,
                                                    PrimitiveMode modePacked,
                                                    const GLsizei *counts,
                                                    DrawElementsType typePacked,
                                                    const GLvoid *const *indices,
                                                    const GLsizei *instanceCounts,
                                                    GLsizei drawcount,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsInstancedANGLE_instanceCounts(const State &glState,
                                                           PrimitiveMode modePacked,
                                                           const GLsizei *counts,
                                                           DrawElementsType typePacked,
                                                           const GLvoid *const *indices,
                                                           const GLsizei *instanceCounts,
                                                           GLsizei drawcount,
                                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_ANGLE_request_extension
void CaptureRequestExtensionANGLE_name(const State &glState,
                                       const GLchar *name,
                                       ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureDisableExtensionANGLE_name(const State &glState,
                                       const GLchar *name,
                                       ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

// GL_ANGLE_robust_client_memory
void CaptureGetBooleanvRobustANGLE_length(const State &glState,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLboolean *params,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBooleanvRobustANGLE_params(const State &glState,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLboolean *params,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBufferParameterivRobustANGLE_length(const State &glState,
                                                   BufferBinding targetPacked,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   GLint *params,
                                                   ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBufferParameterivRobustANGLE_params(const State &glState,
                                                   BufferBinding targetPacked,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei *length,
                                                   GLint *params,
                                                   ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFloatvRobustANGLE_length(const State &glState,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLfloat *params,
                                        ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
}

void CaptureGetFloatvRobustANGLE_params(const State &glState,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLfloat *params,
                                        ParamCapture *paramCapture)
{
    CaptureGetFloatv_data(glState, pname, params, paramCapture);
}

void CaptureGetFramebufferAttachmentParameterivRobustANGLE_length(const State &glState,
                                                                  GLenum target,
                                                                  GLenum attachment,
                                                                  GLenum pname,
                                                                  GLsizei bufSize,
                                                                  GLsizei *length,
                                                                  GLint *params,
                                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFramebufferAttachmentParameterivRobustANGLE_params(const State &glState,
                                                                  GLenum target,
                                                                  GLenum attachment,
                                                                  GLenum pname,
                                                                  GLsizei bufSize,
                                                                  GLsizei *length,
                                                                  GLint *params,
                                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetIntegervRobustANGLE_length(const State &glState,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *data,
                                          ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
}

void CaptureGetIntegervRobustANGLE_data(const State &glState,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLint *data,
                                        ParamCapture *paramCapture)
{
    CaptureGetParameter(glState, pname, sizeof(GLint), paramCapture);
}

void CaptureGetProgramivRobustANGLE_length(const State &glState,
                                           ShaderProgramID program,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramivRobustANGLE_params(const State &glState,
                                           ShaderProgramID program,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetRenderbufferParameterivRobustANGLE_length(const State &glState,
                                                         GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei *length,
                                                         GLint *params,
                                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetRenderbufferParameterivRobustANGLE_params(const State &glState,
                                                         GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei *length,
                                                         GLint *params,
                                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetShaderivRobustANGLE_length(const State &glState,
                                          ShaderProgramID shader,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params,
                                          ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
}

void CaptureGetShaderivRobustANGLE_params(const State &glState,
                                          ShaderProgramID shader,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params,
                                          ParamCapture *paramCapture)
{
    CaptureGetShaderiv_params(glState, shader, pname, params, paramCapture);
}

void CaptureGetTexParameterfvRobustANGLE_length(const State &glState,
                                                TextureType targetPacked,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLfloat *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterfvRobustANGLE_params(const State &glState,
                                                TextureType targetPacked,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLfloat *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterivRobustANGLE_length(const State &glState,
                                                TextureType targetPacked,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLint *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterivRobustANGLE_params(const State &glState,
                                                TextureType targetPacked,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLint *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUniformfvRobustANGLE_length(const State &glState,
                                           ShaderProgramID program,
                                           UniformLocation location,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLfloat *params,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUniformfvRobustANGLE_params(const State &glState,
                                           ShaderProgramID program,
                                           UniformLocation location,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLfloat *params,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUniformivRobustANGLE_length(const State &glState,
                                           ShaderProgramID program,
                                           UniformLocation location,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUniformivRobustANGLE_params(const State &glState,
                                           ShaderProgramID program,
                                           UniformLocation location,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribfvRobustANGLE_length(const State &glState,
                                                GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLfloat *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribfvRobustANGLE_params(const State &glState,
                                                GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLfloat *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribivRobustANGLE_length(const State &glState,
                                                GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLint *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribivRobustANGLE_params(const State &glState,
                                                GLuint index,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLint *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribPointervRobustANGLE_length(const State &glState,
                                                      GLuint index,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      void **pointer,
                                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribPointervRobustANGLE_pointer(const State &glState,
                                                       GLuint index,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei *length,
                                                       void **pointer,
                                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadPixelsRobustANGLE_length(const State &glState,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLsizei *columns,
                                         GLsizei *rows,
                                         void *pixels,
                                         ParamCapture *paramCapture)
{
    if (!length)
    {
        return;
    }

    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
    CaptureMemory(length, paramCapture->readBufferSizeBytes, paramCapture);
}

void CaptureReadPixelsRobustANGLE_columns(const State &glState,
                                          GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLsizei *columns,
                                          GLsizei *rows,
                                          void *pixels,
                                          ParamCapture *paramCapture)
{
    if (!columns)
    {
        return;
    }

    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
    CaptureMemory(columns, paramCapture->readBufferSizeBytes, paramCapture);
}

void CaptureReadPixelsRobustANGLE_rows(const State &glState,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height,
                                       GLenum format,
                                       GLenum type,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLsizei *columns,
                                       GLsizei *rows,
                                       void *pixels,
                                       ParamCapture *paramCapture)
{
    if (!rows)
    {
        return;
    }

    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
    CaptureMemory(rows, paramCapture->readBufferSizeBytes, paramCapture);
}

void CaptureReadPixelsRobustANGLE_pixels(const State &glState,
                                         GLint x,
                                         GLint y,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLsizei *columns,
                                         GLsizei *rows,
                                         void *pixels,
                                         ParamCapture *paramCapture)
{
    if (glState.getTargetBuffer(gl::BufferBinding::PixelPack))
    {
        // If a pixel pack buffer is bound, this is an offset, not a pointer
        paramCapture->value.voidPointerVal = pixels;
        return;
    }

    paramCapture->readBufferSizeBytes = bufSize;
}

void CaptureTexImage2DRobustANGLE_pixels(const State &glState,
                                         TextureTarget targetPacked,
                                         GLint level,
                                         GLint internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
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

    CaptureMemory(pixels, bufSize, paramCapture);
}

void CaptureTexParameterfvRobustANGLE_params(const State &glState,
                                             TextureType targetPacked,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             const GLfloat *params,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexParameterivRobustANGLE_params(const State &glState,
                                             TextureType targetPacked,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             const GLint *params,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexSubImage2DRobustANGLE_pixels(const State &glState,
                                            TextureTarget targetPacked,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            GLsizei bufSize,
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

    CaptureMemory(pixels, bufSize, paramCapture);
}

void CaptureTexImage3DRobustANGLE_pixels(const State &glState,
                                         TextureTarget targetPacked,
                                         GLint level,
                                         GLint internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLenum format,
                                         GLenum type,
                                         GLsizei bufSize,
                                         const void *pixels,
                                         ParamCapture *paramCapture)
{
    CaptureTexImage3D_pixels(glState, targetPacked, level, internalformat, width, height, depth,
                             border, format, type, pixels, paramCapture);
}

void CaptureTexSubImage3DRobustANGLE_pixels(const State &glState,
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
                                            GLsizei bufSize,
                                            const void *pixels,
                                            ParamCapture *paramCapture)
{
    CaptureTexSubImage3D_pixels(glState, targetPacked, level, xoffset, yoffset, zoffset, width,
                                height, depth, format, type, pixels, paramCapture);
}

void CaptureCompressedTexImage2DRobustANGLE_data(const State &glState,
                                                 TextureTarget targetPacked,
                                                 GLint level,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLint border,
                                                 GLsizei imageSize,
                                                 GLsizei dataSize,
                                                 const GLvoid *data,
                                                 ParamCapture *paramCapture)
{
    CaptureCompressedTexImage2D_data(glState, targetPacked, level, internalformat, width, height,
                                     border, imageSize, data, paramCapture);
}

void CaptureCompressedTexSubImage2DRobustANGLE_data(const State &glState,
                                                    TextureTarget targetPacked,
                                                    GLint level,
                                                    GLsizei xoffset,
                                                    GLsizei yoffset,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLenum format,
                                                    GLsizei imageSize,
                                                    GLsizei dataSize,
                                                    const GLvoid *data,
                                                    ParamCapture *paramCapture)
{
    CaptureCompressedTexSubImage2D_data(glState, targetPacked, level, xoffset, yoffset, width,
                                        height, format, imageSize, data, paramCapture);
}

void CaptureCompressedTexImage3DRobustANGLE_data(const State &glState,
                                                 TextureTarget targetPacked,
                                                 GLint level,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLsizei depth,
                                                 GLint border,
                                                 GLsizei imageSize,
                                                 GLsizei dataSize,
                                                 const GLvoid *data,
                                                 ParamCapture *paramCapture)
{
    CaptureCompressedTexImage3D_data(glState, targetPacked, level, internalformat, width, height,
                                     depth, border, imageSize, data, paramCapture);
}

void CaptureCompressedTexSubImage3DRobustANGLE_data(const State &glState,
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
                                                    GLsizei dataSize,
                                                    const GLvoid *data,
                                                    ParamCapture *paramCapture)
{
    CaptureCompressedTexSubImage3D_data(glState, targetPacked, level, xoffset, yoffset, zoffset,
                                        width, height, depth, format, imageSize, data,
                                        paramCapture);
}

void CaptureGetQueryivRobustANGLE_length(const State &glState,
                                         QueryType targetPacked,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLint *params,
                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryivRobustANGLE_params(const State &glState,
                                         QueryType targetPacked,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLint *params,
                                         ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectuivRobustANGLE_length(const State &glState,
                                                QueryID id,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLuint *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectuivRobustANGLE_params(const State &glState,
                                                QueryID id,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLuint *params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBufferPointervRobustANGLE_length(const State &glState,
                                                BufferBinding targetPacked,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                void **params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBufferPointervRobustANGLE_params(const State &glState,
                                                BufferBinding targetPacked,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                void **params,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetIntegeri_vRobustANGLE_length(const State &glState,
                                            GLenum target,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint *data,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetIntegeri_vRobustANGLE_data(const State &glState,
                                          GLenum target,
                                          GLuint index,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *data,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetInternalformativRobustANGLE_length(const State &glState,
                                                  GLenum target,
                                                  GLenum internalformat,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLint *params,
                                                  ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
}

void CaptureGetInternalformativRobustANGLE_params(const State &glState,
                                                  GLenum target,
                                                  GLenum internalformat,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLint *params,
                                                  ParamCapture *paramCapture)
{
    CaptureGetInternalformativ_params(glState, target, internalformat, pname, bufSize, params,
                                      paramCapture);
}

void CaptureGetVertexAttribIivRobustANGLE_length(const State &glState,
                                                 GLuint index,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei *length,
                                                 GLint *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribIivRobustANGLE_params(const State &glState,
                                                 GLuint index,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei *length,
                                                 GLint *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribIuivRobustANGLE_length(const State &glState,
                                                  GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLuint *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetVertexAttribIuivRobustANGLE_params(const State &glState,
                                                  GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLuint *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUniformuivRobustANGLE_length(const State &glState,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint *params,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUniformuivRobustANGLE_params(const State &glState,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint *params,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetActiveUniformBlockivRobustANGLE_length(const State &glState,
                                                      ShaderProgramID program,
                                                      UniformBlockIndex uniformBlockIndex,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLint *params,
                                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetActiveUniformBlockivRobustANGLE_params(const State &glState,
                                                      ShaderProgramID program,
                                                      UniformBlockIndex uniformBlockIndex,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLint *params,
                                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetInteger64vRobustANGLE_length(const State &glState,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint64 *data,
                                            ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
}

void CaptureGetInteger64vRobustANGLE_data(const State &glState,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint64 *data,
                                          ParamCapture *paramCapture)
{
    CaptureGetParameter(glState, pname, sizeof(GLint64), paramCapture);
}

void CaptureGetInteger64i_vRobustANGLE_length(const State &glState,
                                              GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei *length,
                                              GLint64 *data,
                                              ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetInteger64i_vRobustANGLE_data(const State &glState,
                                            GLenum target,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint64 *data,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBufferParameteri64vRobustANGLE_length(const State &glState,
                                                     BufferBinding targetPacked,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint64 *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBufferParameteri64vRobustANGLE_params(const State &glState,
                                                     BufferBinding targetPacked,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint64 *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSamplerParameterivRobustANGLE_param(const State &glState,
                                                SamplerID sampler,
                                                GLuint pname,
                                                GLsizei bufSize,
                                                const GLint *param,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSamplerParameterfvRobustANGLE_param(const State &glState,
                                                SamplerID sampler,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                const GLfloat *param,
                                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterivRobustANGLE_length(const State &glState,
                                                    SamplerID sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei *length,
                                                    GLint *params,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterivRobustANGLE_params(const State &glState,
                                                    SamplerID sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei *length,
                                                    GLint *params,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterfvRobustANGLE_length(const State &glState,
                                                    SamplerID sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei *length,
                                                    GLfloat *params,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterfvRobustANGLE_params(const State &glState,
                                                    SamplerID sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei *length,
                                                    GLfloat *params,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFramebufferParameterivRobustANGLE_length(const State &glState,
                                                        GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei *length,
                                                        GLint *params,
                                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFramebufferParameterivRobustANGLE_params(const State &glState,
                                                        GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei *length,
                                                        GLint *params,
                                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramInterfaceivRobustANGLE_length(const State &glState,
                                                    ShaderProgramID program,
                                                    GLenum programInterface,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei *length,
                                                    GLint *params,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramInterfaceivRobustANGLE_params(const State &glState,
                                                    ShaderProgramID program,
                                                    GLenum programInterface,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei *length,
                                                    GLint *params,
                                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBooleani_vRobustANGLE_length(const State &glState,
                                            GLenum target,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLboolean *data,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetBooleani_vRobustANGLE_data(const State &glState,
                                          GLenum target,
                                          GLuint index,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLboolean *data,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetMultisamplefvRobustANGLE_length(const State &glState,
                                               GLenum pname,
                                               GLuint index,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLfloat *val,
                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetMultisamplefvRobustANGLE_val(const State &glState,
                                            GLenum pname,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLfloat *val,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexLevelParameterivRobustANGLE_length(const State &glState,
                                                     TextureTarget targetPacked,
                                                     GLint level,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexLevelParameterivRobustANGLE_params(const State &glState,
                                                     TextureTarget targetPacked,
                                                     GLint level,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexLevelParameterfvRobustANGLE_length(const State &glState,
                                                     TextureTarget targetPacked,
                                                     GLint level,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLfloat *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexLevelParameterfvRobustANGLE_params(const State &glState,
                                                     TextureTarget targetPacked,
                                                     GLint level,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLfloat *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetPointervRobustANGLERobustANGLE_length(const State &glState,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     void **params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetPointervRobustANGLERobustANGLE_params(const State &glState,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     void **params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadnPixelsRobustANGLE_length(const State &glState,
                                          GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLsizei *columns,
                                          GLsizei *rows,
                                          void *data,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadnPixelsRobustANGLE_columns(const State &glState,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLsizei *columns,
                                           GLsizei *rows,
                                           void *data,
                                           ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadnPixelsRobustANGLE_rows(const State &glState,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height,
                                        GLenum format,
                                        GLenum type,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLsizei *columns,
                                        GLsizei *rows,
                                        void *data,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadnPixelsRobustANGLE_data(const State &glState,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height,
                                        GLenum format,
                                        GLenum type,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLsizei *columns,
                                        GLsizei *rows,
                                        void *data,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformfvRobustANGLE_length(const State &glState,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLfloat *params,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformfvRobustANGLE_params(const State &glState,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLfloat *params,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformivRobustANGLE_length(const State &glState,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint *params,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformivRobustANGLE_params(const State &glState,
                                            ShaderProgramID program,
                                            UniformLocation location,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint *params,
                                            ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformuivRobustANGLE_length(const State &glState,
                                             ShaderProgramID program,
                                             UniformLocation location,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLuint *params,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformuivRobustANGLE_params(const State &glState,
                                             ShaderProgramID program,
                                             UniformLocation location,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLuint *params,
                                             ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexParameterIivRobustANGLE_params(const State &glState,
                                              TextureType targetPacked,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              const GLint *params,
                                              ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexParameterIuivRobustANGLE_params(const State &glState,
                                               TextureType targetPacked,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLuint *params,
                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterIivRobustANGLE_length(const State &glState,
                                                 TextureType targetPacked,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei *length,
                                                 GLint *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterIivRobustANGLE_params(const State &glState,
                                                 TextureType targetPacked,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei *length,
                                                 GLint *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterIuivRobustANGLE_length(const State &glState,
                                                  TextureType targetPacked,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLuint *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexParameterIuivRobustANGLE_params(const State &glState,
                                                  TextureType targetPacked,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLuint *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSamplerParameterIivRobustANGLE_param(const State &glState,
                                                 SamplerID sampler,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 const GLint *param,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSamplerParameterIuivRobustANGLE_param(const State &glState,
                                                  SamplerID sampler,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  const GLuint *param,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterIivRobustANGLE_length(const State &glState,
                                                     SamplerID sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterIivRobustANGLE_params(const State &glState,
                                                     SamplerID sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei *length,
                                                     GLint *params,
                                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterIuivRobustANGLE_length(const State &glState,
                                                      SamplerID sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLuint *params,
                                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetSamplerParameterIuivRobustANGLE_params(const State &glState,
                                                      SamplerID sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei *length,
                                                      GLuint *params,
                                                      ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectivRobustANGLE_length(const State &glState,
                                               QueryID id,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLint *params,
                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectivRobustANGLE_params(const State &glState,
                                               QueryID id,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLint *params,
                                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjecti64vRobustANGLE_length(const State &glState,
                                                 QueryID id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei *length,
                                                 GLint64 *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjecti64vRobustANGLE_params(const State &glState,
                                                 QueryID id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei *length,
                                                 GLint64 *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectui64vRobustANGLE_length(const State &glState,
                                                  QueryID id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLuint64 *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjectui64vRobustANGLE_params(const State &glState,
                                                  QueryID id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei *length,
                                                  GLuint64 *params,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetFramebufferPixelLocalStorageParameterfvRobustANGLE_length(
    const State &glState,
    GLint plane,
    GLenum pname,
    GLsizei bufSize,
    GLsizei *length,
    GLfloat *params,
    angle::ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
}

void CaptureGetFramebufferPixelLocalStorageParameterfvRobustANGLE_params(
    const State &glState,
    GLint plane,
    GLenum pname,
    GLsizei bufSize,
    GLsizei *length,
    GLfloat *params,
    angle::ParamCapture *paramCapture)
{
    CaptureGetFramebufferPixelLocalStorageParameterfvANGLE_params(glState, plane, pname, params,
                                                                  paramCapture);
}

void CaptureGetFramebufferPixelLocalStorageParameterivRobustANGLE_length(
    const State &glState,
    GLint plane,
    GLenum pname,
    GLsizei bufSize,
    GLsizei *length,
    GLint *params,
    angle::ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
}

void CaptureGetFramebufferPixelLocalStorageParameterivRobustANGLE_params(
    const State &glState,
    GLint plane,
    GLenum pname,
    GLsizei bufSize,
    GLsizei *length,
    GLint *params,
    angle::ParamCapture *paramCapture)
{
    CaptureGetFramebufferPixelLocalStorageParameterivANGLE_params(glState, plane, pname, params,
                                                                  paramCapture);
}

// GL_ANGLE_shader_pixel_local_storage
void CaptureFramebufferPixelLocalClearValuefvANGLE_value(const State &glState,
                                                         GLint plane,
                                                         const GLfloat *value,
                                                         angle::ParamCapture *paramCapture)
{
    CaptureArray(value, 4, paramCapture);
}

void CaptureFramebufferPixelLocalClearValueivANGLE_value(const State &glState,
                                                         GLint plane,
                                                         const GLint *value,
                                                         angle::ParamCapture *paramCapture)
{
    CaptureArray(value, 4, paramCapture);
}

void CaptureFramebufferPixelLocalClearValueuivANGLE_value(const State &glState,
                                                          GLint plane,
                                                          const GLuint *value,
                                                          angle::ParamCapture *paramCapture)
{
    CaptureArray(value, 4, paramCapture);
}

void CaptureBeginPixelLocalStorageANGLE_loadops(const State &glState,
                                                GLsizei n,
                                                const GLenum loadops[],
                                                angle::ParamCapture *paramCapture)
{
    CaptureArray(loadops, n, paramCapture);
}

void CaptureEndPixelLocalStorageANGLE_storeops(const State &glState,
                                               GLsizei n,
                                               const GLenum *storeops,
                                               angle::ParamCapture *paramCapture)
{
    CaptureArray(storeops, n, paramCapture);
}

void CaptureGetFramebufferPixelLocalStorageParameterfvANGLE_params(
    const State &glState,
    GLint plane,
    GLenum pname,
    GLfloat *params,
    angle::ParamCapture *paramCapture)
{
    size_t numParams = 1;
    switch (pname)
    {
        case GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE:
        case GL_PIXEL_LOCAL_CLEAR_VALUE_INT_ANGLE:
        case GL_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT_ANGLE:
            numParams = 4;
            break;
    }
    paramCapture->readBufferSizeBytes = sizeof(GLfloat) * numParams;
}

void CaptureGetFramebufferPixelLocalStorageParameterivANGLE_params(
    const State &glState,
    GLint plane,
    GLenum pname,
    GLint *params,
    angle::ParamCapture *paramCapture)
{
    size_t numParams = 1;
    switch (pname)
    {
        case GL_PIXEL_LOCAL_CLEAR_VALUE_FLOAT_ANGLE:
        case GL_PIXEL_LOCAL_CLEAR_VALUE_INT_ANGLE:
        case GL_PIXEL_LOCAL_CLEAR_VALUE_UNSIGNED_INT_ANGLE:
            numParams = 4;
            break;
    }
    paramCapture->readBufferSizeBytes = sizeof(GLint) * numParams;
}

// GL_ANGLE_texture_multisample
void CaptureGetMultisamplefvANGLE_val(const State &glState,
                                      GLenum pname,
                                      GLuint index,
                                      GLfloat *val,
                                      ParamCapture *paramCapture)
{
    // GL_SAMPLE_POSITION_ANGLE: 2 floats
    paramCapture->readBufferSizeBytes = sizeof(GLfloat) * 2;
}

// GL_ANGLE_translated_shader_source
void CaptureGetTranslatedShaderSourceANGLE_length(const State &glState,
                                                  ShaderProgramID shader,
                                                  GLsizei bufsize,
                                                  GLsizei *length,
                                                  GLchar *source,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTranslatedShaderSourceANGLE_source(const State &glState,
                                                  ShaderProgramID shader,
                                                  GLsizei bufsize,
                                                  GLsizei *length,
                                                  GLchar *source,
                                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_ANGLE_vulkan_image
void CaptureAcquireTexturesANGLE_texturesPacked(const State &glState,
                                                GLuint numTextures,
                                                const TextureID *textures,
                                                const GLenum *layouts,
                                                angle::ParamCapture *paramCapture)
{
    CaptureArray(textures, numTextures, paramCapture);
}

void CaptureAcquireTexturesANGLE_layouts(const State &glState,
                                         GLuint numTextures,
                                         const TextureID *texturesPacked,
                                         const GLenum *layouts,
                                         angle::ParamCapture *paramCapture)
{
    CaptureArray(layouts, numTextures * sizeof(GLenum), paramCapture);
}

void CaptureReleaseTexturesANGLE_texturesPacked(const State &glState,
                                                GLuint numTextures,
                                                const TextureID *textures,
                                                GLenum *layouts,
                                                angle::ParamCapture *paramCapture)
{
    CaptureArray(textures, numTextures, paramCapture);
}

void CaptureReleaseTexturesANGLE_layouts(const State &glState,
                                         GLuint numTextures,
                                         const TextureID *texturesPacked,
                                         GLenum *layouts,
                                         angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_CHROMIUM_bind_uniform_location
void CaptureBindUniformLocationCHROMIUM_name(const State &glState,
                                             ShaderProgramID program,
                                             UniformLocation location,
                                             const GLchar *name,
                                             ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

// GL_EXT_EGL_image_storage
void CaptureEGLImageTargetTexStorageEXT_attrib_list(const State &glState,
                                                    GLenum target,
                                                    egl::ImageID image,
                                                    const GLint *attrib_list,
                                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_EXT_base_instance
void CaptureDrawElementsInstancedBaseInstanceEXT_indices(const State &glState,
                                                         PrimitiveMode mode,
                                                         GLsizei count,
                                                         DrawElementsType type,
                                                         const void *indices,
                                                         GLsizei instancecount,
                                                         GLuint baseinstance,
                                                         angle::ParamCapture *indicesParam)
{
    CaptureDrawElements_indices(glState, mode, count, type, indices, indicesParam);
}

void CaptureDrawElementsInstancedBaseVertexBaseInstanceEXT_indices(
    const State &glState,
    PrimitiveMode modePacked,
    GLsizei count,
    DrawElementsType typePacked,
    const void *indices,
    GLsizei instancecount,
    GLint basevertex,
    GLuint baseInstance,
    angle::ParamCapture *indicesParam)
{
    CaptureDrawElements_indices(glState, modePacked, count, typePacked, indices, indicesParam);
}

// GL_EXT_blend_func_extended
void CaptureBindFragDataLocationEXT_name(const State &glState,
                                         ShaderProgramID program,
                                         GLuint color,
                                         const GLchar *name,
                                         ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureBindFragDataLocationIndexedEXT_name(const State &glState,
                                                ShaderProgramID program,
                                                GLuint colorNumber,
                                                GLuint index,
                                                const GLchar *name,
                                                ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureGetFragDataIndexEXT_name(const State &glState,
                                     ShaderProgramID program,
                                     const GLchar *name,
                                     ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

void CaptureGetProgramResourceLocationIndexEXT_name(const State &glState,
                                                    ShaderProgramID program,
                                                    GLenum programInterface,
                                                    const GLchar *name,
                                                    ParamCapture *paramCapture)
{
    CaptureString(name, paramCapture);
}

// GL_EXT_buffer_storage
void CaptureBufferStorageEXT_data(const State &glState,
                                  BufferBinding targetPacked,
                                  GLsizeiptr size,
                                  const void *data,
                                  GLbitfield flags,
                                  angle::ParamCapture *paramCapture)
{
    if (data)
    {
        CaptureMemory(data, size, paramCapture);
    }
}

// GL_EXT_clear_texture
void CaptureClearTexImageEXT_data(const State &glState,
                                  TextureID texturePacked,
                                  GLint level,
                                  GLenum format,
                                  GLenum type,
                                  const void *data,
                                  angle::ParamCapture *paramCapture)
{
    if (!data)
    {
        return;
    }

    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(format, type);
    GLuint captureSize                           = internalFormatInfo.computePixelBytes(type);
    CaptureMemory(data, captureSize, paramCapture);
}

void CaptureClearTexSubImageEXT_data(const State &glState,
                                     TextureID texturePacked,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint zoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLenum format,
                                     GLenum type,
                                     const void *data,
                                     angle::ParamCapture *paramCapture)
{
    if (!data)
    {
        return;
    }

    const gl::InternalFormat &internalFormatInfo = gl::GetInternalFormatInfo(format, type);
    GLuint captureSize                           = internalFormatInfo.computePixelBytes(type);
    CaptureMemory(data, captureSize, paramCapture);
}

// GL_EXT_debug_label
void CaptureGetObjectLabelEXT_length(const State &glState,
                                     GLenum type,
                                     GLuint object,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLchar *label,
                                     angle::ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetObjectLabelEXT_label(const State &glState,
                                    GLenum type,
                                    GLuint object,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLchar *label,
                                    angle::ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureLabelObjectEXT_label(const State &glState,
                                 GLenum type,
                                 GLuint object,
                                 GLsizei length,
                                 const GLchar *label,
                                 angle::ParamCapture *paramCapture)
{
    // Skipped
}

// GL_EXT_debug_marker
void CaptureInsertEventMarkerEXT_marker(const State &glState,
                                        GLsizei length,
                                        const GLchar *marker,
                                        ParamCapture *paramCapture)
{
    // Skipped
}

void CapturePushGroupMarkerEXT_marker(const State &glState,
                                      GLsizei length,
                                      const GLchar *marker,
                                      ParamCapture *paramCapture)
{
    // Skipped
}

// GL_EXT_discard_framebuffer
void CaptureDiscardFramebufferEXT_attachments(const State &glState,
                                              GLenum target,
                                              GLsizei numAttachments,
                                              const GLenum *attachments,
                                              ParamCapture *paramCapture)
{
    CaptureArray(attachments, numAttachments, paramCapture);
}

// GL_EXT_disjoint_timer_query
void CaptureDeleteQueriesEXT_idsPacked(const State &glState,
                                       GLsizei n,
                                       const QueryID *ids,
                                       ParamCapture *paramCapture)
{
    CaptureArray(ids, n, paramCapture);
}

void CaptureGenQueriesEXT_idsPacked(const State &glState,
                                    GLsizei n,
                                    QueryID *ids,
                                    ParamCapture *paramCapture)
{
    CaptureGenHandles(n, ids, paramCapture);
}

void CaptureGetInteger64vEXT_data(const State &glState,
                                  GLenum pname,
                                  GLint64 *data,
                                  angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetQueryObjecti64vEXT_params(const State &glState,
                                         QueryID id,
                                         GLenum pname,
                                         GLint64 *params,
                                         ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLint64);
}

void CaptureGetQueryObjectivEXT_params(const State &glState,
                                       QueryID id,
                                       GLenum pname,
                                       GLint *params,
                                       ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLint);
}

void CaptureGetQueryObjectui64vEXT_params(const State &glState,
                                          QueryID id,
                                          GLenum pname,
                                          GLuint64 *params,
                                          ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLuint64);
}

void CaptureGetQueryObjectuivEXT_params(const State &glState,
                                        QueryID id,
                                        GLenum pname,
                                        GLuint *params,
                                        ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLuint);
}

void CaptureGetQueryivEXT_params(const State &glState,
                                 QueryType targetPacked,
                                 GLenum pname,
                                 GLint *params,
                                 ParamCapture *paramCapture)
{
    CaptureMemory(params, sizeof(GLint), paramCapture);
}

// GL_EXT_draw_buffers
void CaptureDrawBuffersEXT_bufs(const State &glState,
                                GLsizei n,
                                const GLenum *bufs,
                                ParamCapture *paramCapture)
{
    CaptureDrawBuffers_bufs(glState, n, bufs, paramCapture);
}

// GL_EXT_draw_elements_base_vertex
void CaptureDrawElementsBaseVertexEXT_indices(const State &glState,
                                              PrimitiveMode modePacked,
                                              GLsizei count,
                                              DrawElementsType typePacked,
                                              const void *indices,
                                              GLint basevertex,
                                              ParamCapture *indicesParam)
{
    UNIMPLEMENTED();
}

void CaptureDrawElementsInstancedBaseVertexEXT_indices(const State &glState,
                                                       PrimitiveMode modePacked,
                                                       GLsizei count,
                                                       DrawElementsType typePacked,
                                                       const void *indices,
                                                       GLsizei instancecount,
                                                       GLint basevertex,
                                                       ParamCapture *indicesParam)
{
    UNIMPLEMENTED();
}

void CaptureDrawRangeElementsBaseVertexEXT_indices(const State &glState,
                                                   PrimitiveMode modePacked,
                                                   GLuint start,
                                                   GLuint end,
                                                   GLsizei count,
                                                   DrawElementsType typePacked,
                                                   const void *indices,
                                                   GLint basevertex,
                                                   ParamCapture *indicesParam)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsBaseVertexEXT_count(const State &glState,
                                                 PrimitiveMode modePacked,
                                                 const GLsizei *count,
                                                 DrawElementsType typePacked,
                                                 const void *const *indices,
                                                 GLsizei drawcount,
                                                 const GLint *basevertex,
                                                 ParamCapture *countParam)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsBaseVertexEXT_indices(const State &glState,
                                                   PrimitiveMode modePacked,
                                                   const GLsizei *count,
                                                   DrawElementsType typePacked,
                                                   const void *const *indices,
                                                   GLsizei drawcount,
                                                   const GLint *basevertex,
                                                   ParamCapture *indicesParam)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsBaseVertexEXT_basevertex(const State &glState,
                                                      PrimitiveMode modePacked,
                                                      const GLsizei *count,
                                                      DrawElementsType typePacked,
                                                      const void *const *indices,
                                                      GLsizei drawcount,
                                                      const GLint *basevertex,
                                                      ParamCapture *basevertexParam)
{
    UNIMPLEMENTED();
}

// GL_EXT_draw_instanced
void CaptureDrawElementsInstancedEXT_indices(const State &glState,
                                             PrimitiveMode modePacked,
                                             GLsizei count,
                                             DrawElementsType typePacked,
                                             const void *indices,
                                             GLsizei primcount,
                                             ParamCapture *paramCapture)
{
    CaptureDrawElements_indices(glState, modePacked, count, typePacked, indices, paramCapture);
}

// GL_EXT_fragment_shading_rate
void CaptureGetFragmentShadingRatesEXT_count(const State &glState,
                                             GLsizei samples,
                                             GLsizei maxCount,
                                             GLsizei *count,
                                             GLenum *shadingRates,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}
void CaptureGetFragmentShadingRatesEXT_shadingRates(const State &glState,
                                                    GLsizei samples,
                                                    GLsizei maxCount,
                                                    GLsizei *count,
                                                    GLenum *shadingRates,
                                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_EXT_memory_object
void CaptureCreateMemoryObjectsEXT_memoryObjectsPacked(const State &glState,
                                                       GLsizei n,
                                                       MemoryObjectID *memoryObjects,
                                                       ParamCapture *paramCapture)
{
    CaptureGenHandles(n, memoryObjects, paramCapture);
}

void CaptureDeleteMemoryObjectsEXT_memoryObjectsPacked(const State &glState,
                                                       GLsizei n,
                                                       const MemoryObjectID *memoryObjects,
                                                       ParamCapture *paramCapture)
{
    CaptureArray(memoryObjects, n, paramCapture);
}

void CaptureGetMemoryObjectParameterivEXT_params(const State &glState,
                                                 MemoryObjectID memoryObject,
                                                 GLenum pname,
                                                 GLint *params,
                                                 ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLint);
}

void CaptureGetUnsignedBytevEXT_data(const State &glState,
                                     GLenum pname,
                                     GLubyte *data,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetUnsignedBytei_vEXT_data(const State &glState,
                                       GLenum target,
                                       GLuint index,
                                       GLubyte *data,
                                       ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMemoryObjectParameterivEXT_params(const State &glState,
                                              MemoryObjectID memoryObject,
                                              GLenum pname,
                                              const GLint *params,
                                              ParamCapture *paramCapture)
{
    CaptureMemory(params, sizeof(GLint), paramCapture);
}

// GL_EXT_multi_draw_arrays
void CaptureMultiDrawArraysEXT_first(const State &glState,
                                     PrimitiveMode modePacked,
                                     const GLint *first,
                                     const GLsizei *count,
                                     GLsizei primcount,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawArraysEXT_count(const State &glState,
                                     PrimitiveMode modePacked,
                                     const GLint *first,
                                     const GLsizei *count,
                                     GLsizei primcount,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsEXT_count(const State &glState,
                                       PrimitiveMode modePacked,
                                       const GLsizei *count,
                                       DrawElementsType typePacked,
                                       const void *const *indices,
                                       GLsizei primcount,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsEXT_indices(const State &glState,
                                         PrimitiveMode modePacked,
                                         const GLsizei *count,
                                         DrawElementsType typePacked,
                                         const void *const *indices,
                                         GLsizei primcount,
                                         angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_EXT_multi_draw_indirect
void CaptureMultiDrawArraysIndirectEXT_indirect(const State &glState,
                                                PrimitiveMode modePacked,
                                                const void *indirect,
                                                GLsizei drawcount,
                                                GLsizei stride,
                                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureMultiDrawElementsIndirectEXT_indirect(const State &glState,
                                                  PrimitiveMode modePacked,
                                                  DrawElementsType typePacked,
                                                  const void *indirect,
                                                  GLsizei drawcount,
                                                  GLsizei stride,
                                                  angle::ParamCapture *paramCapture)
{
    if (glState.getTargetBuffer(gl::BufferBinding::DrawIndirect) != nullptr)
    {
        paramCapture->value.voidConstPointerVal = indirect;
    }
    else
    {
        if (stride == 0)
        {
            stride = sizeof(DrawElementsIndirectCommand);
        }
        CaptureMemory(indirect, stride * drawcount, paramCapture);
    }
}

// GL_EXT_robustness
void CaptureGetnUniformfvEXT_params(const State &glState,
                                    ShaderProgramID program,
                                    UniformLocation location,
                                    GLsizei bufSize,
                                    GLfloat *params,
                                    ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = bufSize;
}

void CaptureGetnUniformivEXT_params(const State &glState,
                                    ShaderProgramID program,
                                    UniformLocation location,
                                    GLsizei bufSize,
                                    GLint *params,
                                    ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = bufSize;
}

void CaptureReadnPixelsEXT_data(const State &glState,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLenum format,
                                GLenum type,
                                GLsizei bufSize,
                                void *data,
                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_EXT_semaphore
void CaptureDeleteSemaphoresEXT_semaphoresPacked(const State &glState,
                                                 GLsizei n,
                                                 const SemaphoreID *semaphores,
                                                 ParamCapture *paramCapture)
{
    CaptureArray(semaphores, n, paramCapture);
}

void CaptureGenSemaphoresEXT_semaphoresPacked(const State &glState,
                                              GLsizei n,
                                              SemaphoreID *semaphores,
                                              ParamCapture *paramCapture)
{
    CaptureGenHandles(n, semaphores, paramCapture);
}

void CaptureGetSemaphoreParameterui64vEXT_params(const State &glState,
                                                 SemaphoreID semaphore,
                                                 GLenum pname,
                                                 GLuint64 *params,
                                                 ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSemaphoreParameterui64vEXT_params(const State &glState,
                                              SemaphoreID semaphore,
                                              GLenum pname,
                                              const GLuint64 *params,
                                              ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureSignalSemaphoreEXT_buffersPacked(const State &glState,
                                             SemaphoreID semaphore,
                                             GLuint numBufferBarriers,
                                             const BufferID *buffers,
                                             GLuint numTextureBarriers,
                                             const TextureID *textures,
                                             const GLenum *dstLayouts,
                                             ParamCapture *paramCapture)
{
    CaptureArray(buffers, numBufferBarriers, paramCapture);
}

void CaptureSignalSemaphoreEXT_texturesPacked(const State &glState,
                                              SemaphoreID semaphore,
                                              GLuint numBufferBarriers,
                                              const BufferID *buffers,
                                              GLuint numTextureBarriers,
                                              const TextureID *textures,
                                              const GLenum *dstLayouts,
                                              ParamCapture *paramCapture)
{
    CaptureArray(textures, numTextureBarriers, paramCapture);
}

void CaptureSignalSemaphoreEXT_dstLayouts(const State &glState,
                                          SemaphoreID semaphore,
                                          GLuint numBufferBarriers,
                                          const BufferID *buffers,
                                          GLuint numTextureBarriers,
                                          const TextureID *textures,
                                          const GLenum *dstLayouts,
                                          ParamCapture *paramCapture)
{
    CaptureArray(dstLayouts, (numBufferBarriers + numTextureBarriers) * sizeof(GLenum),
                 paramCapture);
}

void CaptureWaitSemaphoreEXT_buffersPacked(const State &glState,
                                           SemaphoreID semaphore,
                                           GLuint numBufferBarriers,
                                           const BufferID *buffers,
                                           GLuint numTextureBarriers,
                                           const TextureID *textures,
                                           const GLenum *srcLayouts,
                                           ParamCapture *paramCapture)
{
    CaptureArray(buffers, numBufferBarriers, paramCapture);
}

void CaptureWaitSemaphoreEXT_texturesPacked(const State &glState,
                                            SemaphoreID semaphore,
                                            GLuint numBufferBarriers,
                                            const BufferID *buffers,
                                            GLuint numTextureBarriers,
                                            const TextureID *textures,
                                            const GLenum *srcLayouts,
                                            ParamCapture *paramCapture)
{
    CaptureArray(textures, numTextureBarriers, paramCapture);
}

void CaptureWaitSemaphoreEXT_srcLayouts(const State &glState,
                                        SemaphoreID semaphore,
                                        GLuint numBufferBarriers,
                                        const BufferID *buffers,
                                        GLuint numTextureBarriers,
                                        const TextureID *textures,
                                        const GLenum *srcLayouts,
                                        ParamCapture *paramCapture)
{
    CaptureArray(srcLayouts, (numBufferBarriers + numTextureBarriers), paramCapture);
}

// GL_EXT_separate_shader_objects
void CaptureCreateShaderProgramvEXT_strings(const State &glState,
                                            ShaderType typePacked,
                                            GLsizei count,
                                            const GLchar *const *strings,
                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDeleteProgramPipelinesEXT_pipelinesPacked(const State &glState,
                                                      GLsizei n,
                                                      const ProgramPipelineID *pipelinesPacked,
                                                      angle::ParamCapture *paramCapture)
{
    CaptureArray(pipelinesPacked, n, paramCapture);
}

void CaptureGenProgramPipelinesEXT_pipelinesPacked(const State &glState,
                                                   GLsizei n,
                                                   ProgramPipelineID *pipelinesPacked,
                                                   angle::ParamCapture *paramCapture)
{
    CaptureGenHandles(n, pipelinesPacked, paramCapture);
}

void CaptureGetProgramPipelineInfoLogEXT_length(const State &glState,
                                                ProgramPipelineID pipelinePacked,
                                                GLsizei bufSize,
                                                GLsizei *length,
                                                GLchar *infoLog,
                                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramPipelineInfoLogEXT_infoLog(const State &glState,
                                                 ProgramPipelineID pipelinePacked,
                                                 GLsizei bufSize,
                                                 GLsizei *length,
                                                 GLchar *infoLog,
                                                 angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetProgramPipelineivEXT_params(const State &glState,
                                           ProgramPipelineID pipelinePacked,
                                           GLenum pname,
                                           GLint *params,
                                           angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform1fvEXT_value(const State &glState,
                                       ShaderProgramID programPacked,
                                       UniformLocation locationPacked,
                                       GLsizei count,
                                       const GLfloat *value,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform1ivEXT_value(const State &glState,
                                       ShaderProgramID programPacked,
                                       UniformLocation locationPacked,
                                       GLsizei count,
                                       const GLint *value,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform1uivEXT_value(const State &glState,
                                        ShaderProgramID programPacked,
                                        UniformLocation locationPacked,
                                        GLsizei count,
                                        const GLuint *value,
                                        angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform2fvEXT_value(const State &glState,
                                       ShaderProgramID programPacked,
                                       UniformLocation locationPacked,
                                       GLsizei count,
                                       const GLfloat *value,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform2ivEXT_value(const State &glState,
                                       ShaderProgramID programPacked,
                                       UniformLocation locationPacked,
                                       GLsizei count,
                                       const GLint *value,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform2uivEXT_value(const State &glState,
                                        ShaderProgramID programPacked,
                                        UniformLocation locationPacked,
                                        GLsizei count,
                                        const GLuint *value,
                                        angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform3fvEXT_value(const State &glState,
                                       ShaderProgramID programPacked,
                                       UniformLocation locationPacked,
                                       GLsizei count,
                                       const GLfloat *value,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform3ivEXT_value(const State &glState,
                                       ShaderProgramID programPacked,
                                       UniformLocation locationPacked,
                                       GLsizei count,
                                       const GLint *value,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform3uivEXT_value(const State &glState,
                                        ShaderProgramID programPacked,
                                        UniformLocation locationPacked,
                                        GLsizei count,
                                        const GLuint *value,
                                        angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform4fvEXT_value(const State &glState,
                                       ShaderProgramID programPacked,
                                       UniformLocation locationPacked,
                                       GLsizei count,
                                       const GLfloat *value,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform4ivEXT_value(const State &glState,
                                       ShaderProgramID programPacked,
                                       UniformLocation locationPacked,
                                       GLsizei count,
                                       const GLint *value,
                                       angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniform4uivEXT_value(const State &glState,
                                        ShaderProgramID programPacked,
                                        UniformLocation locationPacked,
                                        GLsizei count,
                                        const GLuint *value,
                                        angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix2fvEXT_value(const State &glState,
                                             ShaderProgramID programPacked,
                                             UniformLocation locationPacked,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat *value,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix2x3fvEXT_value(const State &glState,
                                               ShaderProgramID programPacked,
                                               UniformLocation locationPacked,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat *value,
                                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix2x4fvEXT_value(const State &glState,
                                               ShaderProgramID programPacked,
                                               UniformLocation locationPacked,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat *value,
                                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix3fvEXT_value(const State &glState,
                                             ShaderProgramID programPacked,
                                             UniformLocation locationPacked,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat *value,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix3x2fvEXT_value(const State &glState,
                                               ShaderProgramID programPacked,
                                               UniformLocation locationPacked,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat *value,
                                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix3x4fvEXT_value(const State &glState,
                                               ShaderProgramID programPacked,
                                               UniformLocation locationPacked,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat *value,
                                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix4fvEXT_value(const State &glState,
                                             ShaderProgramID programPacked,
                                             UniformLocation locationPacked,
                                             GLsizei count,
                                             GLboolean transpose,
                                             const GLfloat *value,
                                             angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix4x2fvEXT_value(const State &glState,
                                               ShaderProgramID programPacked,
                                               UniformLocation locationPacked,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat *value,
                                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureProgramUniformMatrix4x3fvEXT_value(const State &glState,
                                               ShaderProgramID programPacked,
                                               UniformLocation locationPacked,
                                               GLsizei count,
                                               GLboolean transpose,
                                               const GLfloat *value,
                                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_EXT_texture_border_clamp
void CaptureGetSamplerParameterIivEXT_params(const State &glState,
                                             SamplerID samplerPacked,
                                             GLenum pname,
                                             GLint *params,
                                             angle::ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetSamplerParameterIuivEXT_params(const State &glState,
                                              SamplerID samplerPacked,
                                              GLenum pname,
                                              GLuint *params,
                                              angle::ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetTexParameterIivEXT_params(const State &glState,
                                         TextureType targetPacked,
                                         GLenum pname,
                                         GLint *params,
                                         angle::ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetTexParameterIuivEXT_params(const State &glState,
                                          TextureType targetPacked,
                                          GLenum pname,
                                          GLuint *params,
                                          angle::ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureSamplerParameterIivEXT_param(const State &glState,
                                         SamplerID samplerPacked,
                                         GLenum pname,
                                         const GLint *param,
                                         angle::ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureSamplerParameterIuivEXT_param(const State &glState,
                                          SamplerID samplerPacked,
                                          GLenum pname,
                                          const GLuint *param,
                                          angle::ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureTexParameterIivEXT_params(const State &glState,
                                      TextureType targetPacked,
                                      GLenum pname,
                                      const GLint *params,
                                      angle::ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureTexParameterIuivEXT_params(const State &glState,
                                       TextureType targetPacked,
                                       GLenum pname,
                                       const GLuint *params,
                                       angle::ParamCapture *paramCapture)
{
    CaptureTexParameterIuiv_params(glState, targetPacked, pname, params, paramCapture);
}

// GL_EXT_texture_storage_compression
void CaptureTexStorageAttribs2DEXT_attrib_list(const State &glState,
                                               GLenum target,
                                               GLsizei levels,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               const GLint *attrib_list,
                                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexStorageAttribs3DEXT_attrib_list(const State &glState,
                                               GLenum target,
                                               GLsizei levels,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLsizei depth,
                                               const GLint *attrib_list,
                                               angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_KHR_debug
void CaptureDebugMessageCallbackKHR_userParam(const State &glState,
                                              GLDEBUGPROCKHR callback,
                                              const void *userParam,
                                              ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureDebugMessageControlKHR_ids(const State &glState,
                                       GLenum source,
                                       GLenum type,
                                       GLenum severity,
                                       GLsizei count,
                                       const GLuint *ids,
                                       GLboolean enabled,
                                       ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureDebugMessageInsertKHR_buf(const State &glState,
                                      GLenum source,
                                      GLenum type,
                                      GLuint id,
                                      GLenum severity,
                                      GLsizei length,
                                      const GLchar *buf,
                                      ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetDebugMessageLogKHR_sources(const State &glState,
                                          GLuint count,
                                          GLsizei bufSize,
                                          GLenum *sources,
                                          GLenum *types,
                                          GLuint *ids,
                                          GLenum *severities,
                                          GLsizei *lengths,
                                          GLchar *messageLog,
                                          ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetDebugMessageLogKHR_types(const State &glState,
                                        GLuint count,
                                        GLsizei bufSize,
                                        GLenum *sources,
                                        GLenum *types,
                                        GLuint *ids,
                                        GLenum *severities,
                                        GLsizei *lengths,
                                        GLchar *messageLog,
                                        ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetDebugMessageLogKHR_ids(const State &glState,
                                      GLuint count,
                                      GLsizei bufSize,
                                      GLenum *sources,
                                      GLenum *types,
                                      GLuint *ids,
                                      GLenum *severities,
                                      GLsizei *lengths,
                                      GLchar *messageLog,
                                      ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetDebugMessageLogKHR_severities(const State &glState,
                                             GLuint count,
                                             GLsizei bufSize,
                                             GLenum *sources,
                                             GLenum *types,
                                             GLuint *ids,
                                             GLenum *severities,
                                             GLsizei *lengths,
                                             GLchar *messageLog,
                                             ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetDebugMessageLogKHR_lengths(const State &glState,
                                          GLuint count,
                                          GLsizei bufSize,
                                          GLenum *sources,
                                          GLenum *types,
                                          GLuint *ids,
                                          GLenum *severities,
                                          GLsizei *lengths,
                                          GLchar *messageLog,
                                          ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetDebugMessageLogKHR_messageLog(const State &glState,
                                             GLuint count,
                                             GLsizei bufSize,
                                             GLenum *sources,
                                             GLenum *types,
                                             GLuint *ids,
                                             GLenum *severities,
                                             GLsizei *lengths,
                                             GLchar *messageLog,
                                             ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetObjectLabelKHR_length(const State &glState,
                                     GLenum identifier,
                                     GLuint name,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLchar *label,
                                     ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetObjectLabelKHR_label(const State &glState,
                                    GLenum identifier,
                                    GLuint name,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLchar *label,
                                    ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetObjectPtrLabelKHR_ptr(const State &glState,
                                     const void *ptr,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLchar *label,
                                     ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetObjectPtrLabelKHR_length(const State &glState,
                                        const void *ptr,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *label,
                                        ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetObjectPtrLabelKHR_label(const State &glState,
                                       const void *ptr,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLchar *label,
                                       ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureGetPointervKHR_params(const State &glState,
                                  GLenum pname,
                                  void **params,
                                  ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureObjectLabelKHR_label(const State &glState,
                                 GLenum identifier,
                                 GLuint name,
                                 GLsizei length,
                                 const GLchar *label,
                                 ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureObjectPtrLabelKHR_ptr(const State &glState,
                                  const void *ptr,
                                  GLsizei length,
                                  const GLchar *label,
                                  ParamCapture *paramCapture)
{
    // Skipped
}

void CaptureObjectPtrLabelKHR_label(const State &glState,
                                    const void *ptr,
                                    GLsizei length,
                                    const GLchar *label,
                                    ParamCapture *paramCapture)
{
    // Skipped
}

void CapturePushDebugGroupKHR_message(const State &glState,
                                      GLenum source,
                                      GLuint id,
                                      GLsizei length,
                                      const GLchar *message,
                                      ParamCapture *paramCapture)
{
    // Skipped
}

// GL_KHR_robustness
void CaptureGetnUniformfvKHR_params(const State &glState,
                                    ShaderProgramID programPacked,
                                    UniformLocation locationPacked,
                                    GLsizei bufSize,
                                    GLfloat *params,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformivKHR_params(const State &glState,
                                    ShaderProgramID programPacked,
                                    UniformLocation locationPacked,
                                    GLsizei bufSize,
                                    GLint *params,
                                    angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetnUniformuivKHR_params(const State &glState,
                                     ShaderProgramID programPacked,
                                     UniformLocation locationPacked,
                                     GLsizei bufSize,
                                     GLuint *params,
                                     angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureReadnPixelsKHR_data(const State &glState,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLenum format,
                                GLenum type,
                                GLsizei bufSize,
                                void *data,
                                angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_MESA_framebuffer_flip_y
void CaptureGetFramebufferParameterivMESA_params(const State &glState,
                                                 GLenum target,
                                                 GLenum pname,
                                                 GLint *params,
                                                 angle::ParamCapture *paramCapture)
{
    // Skipped
}

// GL_NV_fence
void CaptureDeleteFencesNV_fencesPacked(const State &glState,
                                        GLsizei n,
                                        const FenceNVID *fences,
                                        ParamCapture *paramCapture)
{
    CaptureMemory(fences, n * sizeof(FenceNVID), paramCapture);
}

void CaptureGenFencesNV_fencesPacked(const State &glState,
                                     GLsizei n,
                                     FenceNVID *fences,
                                     ParamCapture *paramCapture)
{
    CaptureGenHandles(n, fences, paramCapture);
}

void CaptureGetFenceivNV_params(const State &glState,
                                FenceNVID fence,
                                GLenum pname,
                                GLint *params,
                                ParamCapture *paramCapture)
{
    CaptureMemory(params, sizeof(GLint), paramCapture);
}

// GL_OES_draw_elements_base_vertex
void CaptureDrawElementsBaseVertexOES_indices(const State &glState,
                                              PrimitiveMode modePacked,
                                              GLsizei count,
                                              DrawElementsType typePacked,
                                              const void *indices,
                                              GLint basevertex,
                                              ParamCapture *indicesParam)
{
    CaptureDrawElements_indices(glState, modePacked, count, typePacked, indices, indicesParam);
}

void CaptureDrawElementsInstancedBaseVertexOES_indices(const State &glState,
                                                       PrimitiveMode modePacked,
                                                       GLsizei count,
                                                       DrawElementsType typePacked,
                                                       const void *indices,
                                                       GLsizei instancecount,
                                                       GLint basevertex,
                                                       ParamCapture *indicesParam)
{
    UNIMPLEMENTED();
}

void CaptureDrawRangeElementsBaseVertexOES_indices(const State &glState,
                                                   PrimitiveMode modePacked,
                                                   GLuint start,
                                                   GLuint end,
                                                   GLsizei count,
                                                   DrawElementsType typePacked,
                                                   const void *indices,
                                                   GLint basevertex,
                                                   ParamCapture *indicesParam)
{
    UNIMPLEMENTED();
}

// GL_OES_draw_texture
void CaptureDrawTexfvOES_coords(const State &glState,
                                const GLfloat *coords,
                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDrawTexivOES_coords(const State &glState,
                                const GLint *coords,
                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDrawTexsvOES_coords(const State &glState,
                                const GLshort *coords,
                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureDrawTexxvOES_coords(const State &glState,
                                const GLfixed *coords,
                                ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_OES_framebuffer_object
void CaptureDeleteFramebuffersOES_framebuffersPacked(const State &glState,
                                                     GLsizei n,
                                                     const FramebufferID *framebuffers,
                                                     ParamCapture *paramCapture)
{
    CaptureArray(framebuffers, n, paramCapture);
}

void CaptureDeleteRenderbuffersOES_renderbuffersPacked(const State &glState,
                                                       GLsizei n,
                                                       const RenderbufferID *renderbuffers,
                                                       ParamCapture *paramCapture)
{
    CaptureArray(renderbuffers, n, paramCapture);
}

void CaptureGenFramebuffersOES_framebuffersPacked(const State &glState,
                                                  GLsizei n,
                                                  FramebufferID *framebuffers,
                                                  ParamCapture *paramCapture)
{
    CaptureGenHandles(n, framebuffers, paramCapture);
}

void CaptureGenRenderbuffersOES_renderbuffersPacked(const State &glState,
                                                    GLsizei n,
                                                    RenderbufferID *renderbuffers,
                                                    ParamCapture *paramCapture)
{
    CaptureGenHandles(n, renderbuffers, paramCapture);
}

void CaptureGetFramebufferAttachmentParameterivOES_params(const State &glState,
                                                          GLenum target,
                                                          GLenum attachment,
                                                          GLenum pname,
                                                          GLint *params,
                                                          ParamCapture *paramCapture)
{
    CaptureMemory(params, sizeof(GLint), paramCapture);
}

void CaptureGetRenderbufferParameterivOES_params(const State &glState,
                                                 GLenum target,
                                                 GLenum pname,
                                                 GLint *params,
                                                 ParamCapture *paramCapture)
{
    CaptureMemory(params, sizeof(GLint), paramCapture);
}

// GL_OES_get_program_binary
void CaptureGetProgramBinaryOES_length(const State &glState,
                                       ShaderProgramID program,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLenum *binaryFormat,
                                       void *binary,
                                       ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLsizei);
}

void CaptureGetProgramBinaryOES_binaryFormat(const State &glState,
                                             ShaderProgramID program,
                                             GLsizei bufSize,
                                             GLsizei *length,
                                             GLenum *binaryFormat,
                                             void *binary,
                                             ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = sizeof(GLenum);
}

void CaptureGetProgramBinaryOES_binary(const State &glState,
                                       ShaderProgramID program,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLenum *binaryFormat,
                                       void *binary,
                                       ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = bufSize;
}

void CaptureProgramBinaryOES_binary(const State &glState,
                                    ShaderProgramID program,
                                    GLenum binaryFormat,
                                    const void *binary,
                                    GLint length,
                                    ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_OES_mapbuffer
void CaptureGetBufferPointervOES_params(const State &glState,
                                        BufferBinding targetPacked,
                                        GLenum pname,
                                        void **params,
                                        ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_OES_matrix_palette
void CaptureMatrixIndexPointerOES_pointer(const State &glState,
                                          GLint size,
                                          GLenum type,
                                          GLsizei stride,
                                          const void *pointer,
                                          ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureWeightPointerOES_pointer(const State &glState,
                                     GLint size,
                                     GLenum type,
                                     GLsizei stride,
                                     const void *pointer,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_OES_point_size_array
void CapturePointSizePointerOES_pointer(const State &glState,
                                        VertexAttribType typePacked,
                                        GLsizei stride,
                                        const void *pointer,
                                        ParamCapture *paramCapture)
{
    CaptureVertexPointerGLES1(glState, ClientVertexArrayType::PointSize, pointer, paramCapture);
}

// GL_OES_query_matrix
void CaptureQueryMatrixxOES_mantissa(const State &glState,
                                     GLfixed *mantissa,
                                     GLint *exponent,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureQueryMatrixxOES_exponent(const State &glState,
                                     GLfixed *mantissa,
                                     GLint *exponent,
                                     ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_OES_texture_3D
void CaptureCompressedTexImage3DOES_data(const State &glState,
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
    CaptureCompressedTexImage3D_data(glState, targetPacked, level, internalformat, width, height,
                                     depth, border, imageSize, data, paramCapture);
}

void CaptureCompressedTexSubImage3DOES_data(const State &glState,
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
    UNIMPLEMENTED();
}

void CaptureTexImage3DOES_pixels(const State &glState,
                                 TextureTarget targetPacked,
                                 GLint level,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLint border,
                                 GLenum format,
                                 GLenum type,
                                 const void *pixels,
                                 ParamCapture *paramCapture)
{
    CaptureTexImage3D_pixels(glState, targetPacked, level, internalformat, width, height, depth,
                             border, format, type, pixels, paramCapture);
}

void CaptureTexSubImage3DOES_pixels(const State &glState,
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
    CaptureTexSubImage3D_pixels(glState, targetPacked, level, xoffset, yoffset, zoffset, width,
                                height, depth, format, type, pixels, paramCapture);
}

// GL_OES_texture_border_clamp
void CaptureGetSamplerParameterIivOES_params(const State &glState,
                                             SamplerID sampler,
                                             GLenum pname,
                                             GLint *params,
                                             ParamCapture *paramCapture)
{
    CaptureGetSamplerParameterIiv_params(glState, sampler, pname, params, paramCapture);
}

void CaptureGetSamplerParameterIuivOES_params(const State &glState,
                                              SamplerID sampler,
                                              GLenum pname,
                                              GLuint *params,
                                              ParamCapture *paramCapture)
{
    CaptureGetSamplerParameterIuiv_params(glState, sampler, pname, params, paramCapture);
}

void CaptureGetTexParameterIivOES_params(const State &glState,
                                         TextureType targetPacked,
                                         GLenum pname,
                                         GLint *params,
                                         ParamCapture *paramCapture)
{
    CaptureGetTexParameterIiv_params(glState, targetPacked, pname, params, paramCapture);
}

void CaptureGetTexParameterIuivOES_params(const State &glState,
                                          TextureType targetPacked,
                                          GLenum pname,
                                          GLuint *params,
                                          ParamCapture *paramCapture)
{
    CaptureGetTexParameterIuiv_params(glState, targetPacked, pname, params, paramCapture);
}

void CaptureSamplerParameterIivOES_param(const State &glState,
                                         SamplerID sampler,
                                         GLenum pname,
                                         const GLint *param,
                                         ParamCapture *paramCapture)
{
    CaptureSamplerParameterIiv_param(glState, sampler, pname, param, paramCapture);
}

void CaptureSamplerParameterIuivOES_param(const State &glState,
                                          SamplerID sampler,
                                          GLenum pname,
                                          const GLuint *param,
                                          ParamCapture *paramCapture)
{
    CaptureSamplerParameterIuiv_param(glState, sampler, pname, param, paramCapture);
}

void CaptureTexParameterIivOES_params(const State &glState,
                                      TextureType targetPacked,
                                      GLenum pname,
                                      const GLint *params,
                                      ParamCapture *paramCapture)
{
    CaptureTexParameterIiv_params(glState, targetPacked, pname, params, paramCapture);
}

void CaptureTexParameterIuivOES_params(const State &glState,
                                       TextureType targetPacked,
                                       GLenum pname,
                                       const GLuint *params,
                                       ParamCapture *paramCapture)
{
    CaptureTexParameterIuiv_params(glState, targetPacked, pname, params, paramCapture);
}

// GL_OES_texture_cube_map
void CaptureGetTexGenfvOES_params(const State &glState,
                                  GLenum coord,
                                  GLenum pname,
                                  GLfloat *params,
                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexGenivOES_params(const State &glState,
                                  GLenum coord,
                                  GLenum pname,
                                  GLint *params,
                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureGetTexGenxvOES_params(const State &glState,
                                  GLenum coord,
                                  GLenum pname,
                                  GLfixed *params,
                                  ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexGenfvOES_params(const State &glState,
                               GLenum coord,
                               GLenum pname,
                               const GLfloat *params,
                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexGenivOES_params(const State &glState,
                               GLenum coord,
                               GLenum pname,
                               const GLint *params,
                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

void CaptureTexGenxvOES_params(const State &glState,
                               GLenum coord,
                               GLenum pname,
                               const GLfixed *params,
                               ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

// GL_OES_vertex_array_object
void CaptureDeleteVertexArraysOES_arraysPacked(const State &glState,
                                               GLsizei n,
                                               const VertexArrayID *arrays,
                                               ParamCapture *paramCapture)
{
    CaptureDeleteVertexArrays_arraysPacked(glState, n, arrays, paramCapture);
}

void CaptureGenVertexArraysOES_arraysPacked(const State &glState,
                                            GLsizei n,
                                            VertexArrayID *arrays,
                                            ParamCapture *paramCapture)
{
    CaptureGenVertexArrays_arraysPacked(glState, n, arrays, paramCapture);
}

// GL_QCOM_framebuffer_foveated
void CaptureFramebufferFoveationConfigQCOM_providedFeatures(const State &glState,
                                                            FramebufferID framebufferPacked,
                                                            GLuint numLayers,
                                                            GLuint focalPointsPerLayer,
                                                            GLuint requestedFeatures,
                                                            GLuint *providedFeatures,
                                                            angle::ParamCapture *paramCapture)
{
    UNIMPLEMENTED();
}

}  // namespace gl
