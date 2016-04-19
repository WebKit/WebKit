//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.h: Defines the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#ifndef LIBANGLE_CONTEXT_H_
#define LIBANGLE_CONTEXT_H_

#include <set>
#include <string>

#include "angle_gl.h"
#include "common/angleutils.h"
#include "libANGLE/RefCountObject.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Data.h"
#include "libANGLE/Error.h"
#include "libANGLE/HandleAllocator.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/angletypes.h"

namespace rx
{
class Renderer;
}

namespace egl
{
class AttributeMap;
class Surface;
struct Config;
}

namespace gl
{
class Compiler;
class Shader;
class Program;
class Texture;
class Framebuffer;
class Renderbuffer;
class FenceNV;
class FenceSync;
class Query;
class ResourceManager;
class Buffer;
struct VertexAttribute;
class VertexArray;
class Sampler;
class TransformFeedback;

class Context final : public ValidationContext
{
  public:
    Context(const egl::Config *config,
            const Context *shareContext,
            rx::Renderer *renderer,
            const egl::AttributeMap &attribs);

    virtual ~Context();

    void makeCurrent(egl::Surface *surface);
    void releaseSurface();

    virtual void markContextLost();
    bool isContextLost();

    // These create  and destroy methods are merely pass-throughs to
    // ResourceManager, which owns these object types
    GLuint createBuffer();
    GLuint createShader(GLenum type);
    GLuint createProgram();
    GLuint createTexture();
    GLuint createRenderbuffer();
    GLuint createSampler();
    GLuint createTransformFeedback();
    GLsync createFenceSync();

    void deleteBuffer(GLuint buffer);
    void deleteShader(GLuint shader);
    void deleteProgram(GLuint program);
    void deleteTexture(GLuint texture);
    void deleteRenderbuffer(GLuint renderbuffer);
    void deleteSampler(GLuint sampler);
    void deleteTransformFeedback(GLuint transformFeedback);
    void deleteFenceSync(GLsync fenceSync);

    // Framebuffers are owned by the Context, so these methods do not pass through
    GLuint createFramebuffer();
    void deleteFramebuffer(GLuint framebuffer);

    // NV Fences are owned by the Context.
    GLuint createFenceNV();
    void deleteFenceNV(GLuint fence);

    // Queries are owned by the Context;
    GLuint createQuery();
    void deleteQuery(GLuint query);

    // Vertex arrays are owned by the Context
    GLuint createVertexArray();
    void deleteVertexArray(GLuint vertexArray);

    void bindArrayBuffer(GLuint bufferHandle);
    void bindElementArrayBuffer(GLuint bufferHandle);
    void bindTexture(GLenum target, GLuint handle);
    void bindReadFramebuffer(GLuint framebufferHandle);
    void bindDrawFramebuffer(GLuint framebufferHandle);
    void bindRenderbuffer(GLuint renderbufferHandle);
    void bindVertexArray(GLuint vertexArrayHandle);
    void bindSampler(GLuint textureUnit, GLuint samplerHandle);
    void bindGenericUniformBuffer(GLuint bufferHandle);
    void bindIndexedUniformBuffer(GLuint bufferHandle,
                                  GLuint index,
                                  GLintptr offset,
                                  GLsizeiptr size);
    void bindGenericTransformFeedbackBuffer(GLuint bufferHandle);
    void bindIndexedTransformFeedbackBuffer(GLuint bufferHandle,
                                            GLuint index,
                                            GLintptr offset,
                                            GLsizeiptr size);
    void bindCopyReadBuffer(GLuint bufferHandle);
    void bindCopyWriteBuffer(GLuint bufferHandle);
    void bindPixelPackBuffer(GLuint bufferHandle);
    void bindPixelUnpackBuffer(GLuint bufferHandle);
    void useProgram(GLuint program);
    void bindTransformFeedback(GLuint transformFeedbackHandle);

    Error beginQuery(GLenum target, GLuint query);
    Error endQuery(GLenum target);
    Error queryCounter(GLuint id, GLenum target);
    void getQueryiv(GLenum target, GLenum pname, GLint *params);
    Error getQueryObjectiv(GLuint id, GLenum pname, GLint *params);
    Error getQueryObjectuiv(GLuint id, GLenum pname, GLuint *params);
    Error getQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params);
    Error getQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params);

    void setVertexAttribDivisor(GLuint index, GLuint divisor);

    void samplerParameteri(GLuint sampler, GLenum pname, GLint param);
    void samplerParameterf(GLuint sampler, GLenum pname, GLfloat param);
    GLint getSamplerParameteri(GLuint sampler, GLenum pname);
    GLfloat getSamplerParameterf(GLuint sampler, GLenum pname);

    void programParameteri(GLuint program, GLenum pname, GLint value);

    Buffer *getBuffer(GLuint handle) const;
    FenceNV *getFenceNV(GLuint handle);
    FenceSync *getFenceSync(GLsync handle) const;
    Shader *getShader(GLuint handle) const;
    Program *getProgram(GLuint handle) const;
    Texture *getTexture(GLuint handle) const;
    Framebuffer *getFramebuffer(GLuint handle) const;
    Renderbuffer *getRenderbuffer(GLuint handle) const;
    VertexArray *getVertexArray(GLuint handle) const;
    Sampler *getSampler(GLuint handle) const;
    Query *getQuery(GLuint handle, bool create, GLenum type);
    Query *getQuery(GLuint handle) const;
    TransformFeedback *getTransformFeedback(GLuint handle) const;
    LabeledObject *getLabeledObject(GLenum identifier, GLuint name) const;
    LabeledObject *getLabeledObjectFromPtr(const void *ptr) const;

    Texture *getTargetTexture(GLenum target) const;
    Texture *getSamplerTexture(unsigned int sampler, GLenum type) const;

    Compiler *getCompiler() const;

    bool isSampler(GLuint samplerName) const;

    bool isVertexArrayGenerated(GLuint vertexArray);
    bool isTransformFeedbackGenerated(GLuint vertexArray);

    void getBooleanv(GLenum pname, GLboolean *params);
    void getFloatv(GLenum pname, GLfloat *params);
    void getIntegerv(GLenum pname, GLint *params);
    void getInteger64v(GLenum pname, GLint64 *params);
    void getPointerv(GLenum pname, void **params) const;

    bool getIndexedIntegerv(GLenum target, GLuint index, GLint *data);
    bool getIndexedInteger64v(GLenum target, GLuint index, GLint64 *data);

    bool getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams);
    bool getIndexedQueryParameterInfo(GLenum target, GLenum *type, unsigned int *numParams);

    void clear(GLbitfield mask);
    void clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *values);
    void clearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *values);
    void clearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *values);
    void clearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);

    Error drawArrays(GLenum mode, GLint first, GLsizei count);
    Error drawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instanceCount);

    Error drawElements(GLenum mode,
                       GLsizei count,
                       GLenum type,
                       const GLvoid *indices,
                       const IndexRange &indexRange);
    Error drawElementsInstanced(GLenum mode,
                                GLsizei count,
                                GLenum type,
                                const GLvoid *indices,
                                GLsizei instances,
                                const IndexRange &indexRange);
    Error drawRangeElements(GLenum mode,
                            GLuint start,
                            GLuint end,
                            GLsizei count,
                            GLenum type,
                            const GLvoid *indices,
                            const IndexRange &indexRange);

    void blitFramebuffer(GLint srcX0,
                         GLint srcY0,
                         GLint srcX1,
                         GLint srcY1,
                         GLint dstX0,
                         GLint dstY0,
                         GLint dstX1,
                         GLint dstY1,
                         GLbitfield mask,
                         GLenum filter);

    void readPixels(GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height,
                    GLenum format,
                    GLenum type,
                    GLvoid *pixels);

    void copyTexImage2D(GLenum target,
                        GLint level,
                        GLenum internalformat,
                        GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLint border);

    void copyTexSubImage2D(GLenum target,
                           GLint level,
                           GLint xoffset,
                           GLint yoffset,
                           GLint x,
                           GLint y,
                           GLsizei width,
                           GLsizei height);

    void copyTexSubImage3D(GLenum target,
                           GLint level,
                           GLint xoffset,
                           GLint yoffset,
                           GLint zoffset,
                           GLint x,
                           GLint y,
                           GLsizei width,
                           GLsizei height);

    void framebufferTexture2D(GLenum target,
                              GLenum attachment,
                              GLenum textarget,
                              GLuint texture,
                              GLint level);

    void framebufferRenderbuffer(GLenum target,
                                 GLenum attachment,
                                 GLenum renderbuffertarget,
                                 GLuint renderbuffer);

    void framebufferTextureLayer(GLenum target,
                                 GLenum attachment,
                                 GLuint texture,
                                 GLint level,
                                 GLint layer);

    void drawBuffers(GLsizei n, const GLenum *bufs);
    void readBuffer(GLenum mode);

    void discardFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments);
    void invalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments);
    void invalidateSubFramebuffer(GLenum target,
                                  GLsizei numAttachments,
                                  const GLenum *attachments,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height);

    void texImage2D(GLenum target,
                    GLint level,
                    GLint internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const GLvoid *pixels);
    void texImage3D(GLenum target,
                    GLint level,
                    GLint internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLsizei depth,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const GLvoid *pixels);
    void texSubImage2D(GLenum target,
                       GLint level,
                       GLint xoffset,
                       GLint yoffset,
                       GLsizei width,
                       GLsizei height,
                       GLenum format,
                       GLenum type,
                       const GLvoid *pixels);
    void texSubImage3D(GLenum target,
                       GLint level,
                       GLint xoffset,
                       GLint yoffset,
                       GLint zoffset,
                       GLsizei width,
                       GLsizei height,
                       GLsizei depth,
                       GLenum format,
                       GLenum type,
                       const GLvoid *pixels);
    void compressedTexImage2D(GLenum target,
                              GLint level,
                              GLenum internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLint border,
                              GLsizei imageSize,
                              const GLvoid *data);
    void compressedTexImage3D(GLenum target,
                              GLint level,
                              GLenum internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLsizei depth,
                              GLint border,
                              GLsizei imageSize,
                              const GLvoid *data);
    void compressedTexSubImage2D(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLenum format,
                                 GLsizei imageSize,
                                 const GLvoid *data);
    void compressedTexSubImage3D(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint zoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLsizei imageSize,
                                 const GLvoid *data);

    Error flush();
    Error finish();

    void getBufferPointerv(GLenum target, GLenum pname, void **params);
    GLvoid *mapBuffer(GLenum target, GLenum access);
    GLboolean unmapBuffer(GLenum target);
    GLvoid *mapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
    void flushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length);

    void beginTransformFeedback(GLenum primitiveMode);

    bool hasActiveTransformFeedback(GLuint program) const;

    void insertEventMarker(GLsizei length, const char *marker);
    void pushGroupMarker(GLsizei length, const char *marker);
    void popGroupMarker();

    void recordError(const Error &error) override;

    GLenum getError();
    GLenum getResetStatus();
    virtual bool isResetNotificationEnabled();

    const egl::Config *getConfig() const;
    EGLenum getClientType() const;
    EGLenum getRenderBuffer() const;

    const std::string &getRendererString() const;

    const std::string &getExtensionString() const;
    const std::string &getExtensionString(size_t idx) const;
    size_t getExtensionStringCount() const;

    rx::Renderer *getRenderer() { return mRenderer; }

    State &getState() { return mState; }

  private:
    void syncRendererState();
    void syncRendererState(const State::DirtyBits &bitMask, const State::DirtyObjects &objectMask);
    void syncStateForReadPixels();
    void syncStateForTexImage();
    void syncStateForClear();
    void syncStateForBlit();
    VertexArray *checkVertexArrayAllocation(GLuint vertexArrayHandle);
    TransformFeedback *checkTransformFeedbackAllocation(GLuint transformFeedback);
    Framebuffer *checkFramebufferAllocation(GLuint framebufferHandle);

    void detachBuffer(GLuint buffer);
    void detachTexture(GLuint texture);
    void detachFramebuffer(GLuint framebuffer);
    void detachRenderbuffer(GLuint renderbuffer);
    void detachVertexArray(GLuint vertexArray);
    void detachTransformFeedback(GLuint transformFeedback);
    void detachSampler(GLuint sampler);

    void initRendererString();
    void initExtensionStrings();

    void initCaps(GLuint clientVersion);

    // Caps to use for validation
    Caps mCaps;
    TextureCapsMap mTextureCaps;
    Extensions mExtensions;
    Limitations mLimitations;

    // Shader compiler
    Compiler *mCompiler;

    rx::Renderer *const mRenderer;
    State mState;

    int mClientVersion;

    const egl::Config *mConfig;
    EGLenum mClientType;

    TextureMap mZeroTextures;

    ResourceMap<Framebuffer> mFramebufferMap;
    HandleAllocator mFramebufferHandleAllocator;

    ResourceMap<FenceNV> mFenceNVMap;
    HandleAllocator mFenceNVHandleAllocator;

    ResourceMap<Query> mQueryMap;
    HandleAllocator mQueryHandleAllocator;

    ResourceMap<VertexArray> mVertexArrayMap;
    HandleAllocator mVertexArrayHandleAllocator;

    ResourceMap<TransformFeedback> mTransformFeedbackMap;
    HandleAllocator mTransformFeedbackAllocator;

    std::string mRendererString;
    std::string mExtensionString;
    std::vector<std::string> mExtensionStrings;

    // Recorded errors
    typedef std::set<GLenum> ErrorSet;
    ErrorSet mErrors;

    // Current/lost context flags
    bool mHasBeenCurrent;
    bool mContextLost;
    GLenum mResetStatus;
    GLenum mResetStrategy;
    bool mRobustAccess;
    egl::Surface *mCurrentSurface;

    ResourceManager *mResourceManager;

    State::DirtyBits mTexImageDirtyBits;
    State::DirtyObjects mTexImageDirtyObjects;
    State::DirtyBits mReadPixelsDirtyBits;
    State::DirtyObjects mReadPixelsDirtyObjects;
    State::DirtyBits mClearDirtyBits;
    State::DirtyObjects mClearDirtyObjects;
    State::DirtyBits mBlitDirtyBits;
    State::DirtyObjects mBlitDirtyObjects;
};

}  // namespace gl

#endif   // LIBANGLE_CONTEXT_H_
