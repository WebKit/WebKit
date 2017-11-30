//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextImpl:
//   Implementation-specific functionality associated with a GL Context.
//

#ifndef LIBANGLE_RENDERER_CONTEXTIMPL_H_
#define LIBANGLE_RENDERER_CONTEXTIMPL_H_

#include <vector>

#include "common/angleutils.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/renderer/GLImplFactory.h"

namespace gl
{
class MemoryProgramCache;
class Path;
struct Workarounds;
}

namespace rx
{
class ContextImpl : public GLImplFactory
{
  public:
    ContextImpl(const gl::ContextState &state);
    ~ContextImpl() override;

    virtual void onDestroy(const gl::Context *context) {}

    virtual gl::Error initialize() = 0;

    // Flush and finish.
    virtual gl::Error flush(const gl::Context *context)  = 0;
    virtual gl::Error finish(const gl::Context *context) = 0;

    // Drawing methods.
    virtual gl::Error drawArrays(const gl::Context *context,
                                 GLenum mode,
                                 GLint first,
                                 GLsizei count) = 0;
    virtual gl::Error drawArraysInstanced(const gl::Context *context,
                                          GLenum mode,
                                          GLint first,
                                          GLsizei count,
                                          GLsizei instanceCount) = 0;

    virtual gl::Error drawElements(const gl::Context *context,
                                   GLenum mode,
                                   GLsizei count,
                                   GLenum type,
                                   const void *indices) = 0;
    virtual gl::Error drawElementsInstanced(const gl::Context *context,
                                            GLenum mode,
                                            GLsizei count,
                                            GLenum type,
                                            const void *indices,
                                            GLsizei instances) = 0;
    virtual gl::Error drawRangeElements(const gl::Context *context,
                                        GLenum mode,
                                        GLuint start,
                                        GLuint end,
                                        GLsizei count,
                                        GLenum type,
                                        const void *indices) = 0;

    virtual gl::Error drawArraysIndirect(const gl::Context *context,
                                         GLenum mode,
                                         const void *indirect) = 0;
    virtual gl::Error drawElementsIndirect(const gl::Context *context,
                                           GLenum mode,
                                           GLenum type,
                                           const void *indirect) = 0;

    // CHROMIUM_path_rendering path drawing methods.
    virtual void stencilFillPath(const gl::Path *path, GLenum fillMode, GLuint mask);
    virtual void stencilStrokePath(const gl::Path *path, GLint reference, GLuint mask);
    virtual void coverFillPath(const gl::Path *path, GLenum coverMode);
    virtual void coverStrokePath(const gl::Path *path, GLenum coverMode);
    virtual void stencilThenCoverFillPath(const gl::Path *path,
                                          GLenum fillMode,
                                          GLuint mask,
                                          GLenum coverMode);

    virtual void stencilThenCoverStrokePath(const gl::Path *path,
                                            GLint reference,
                                            GLuint mask,
                                            GLenum coverMode);

    virtual void coverFillPathInstanced(const std::vector<gl::Path *> &paths,
                                        GLenum coverMode,
                                        GLenum transformType,
                                        const GLfloat *transformValues);
    virtual void coverStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                          GLenum coverMode,
                                          GLenum transformType,
                                          const GLfloat *transformValues);
    virtual void stencilFillPathInstanced(const std::vector<gl::Path *> &paths,
                                          GLenum fillMode,
                                          GLuint mask,
                                          GLenum transformType,
                                          const GLfloat *transformValues);
    virtual void stencilStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                            GLint reference,
                                            GLuint mask,
                                            GLenum transformType,
                                            const GLfloat *transformValues);
    virtual void stencilThenCoverFillPathInstanced(const std::vector<gl::Path *> &paths,
                                                   GLenum coverMode,
                                                   GLenum fillMode,
                                                   GLuint mask,
                                                   GLenum transformType,
                                                   const GLfloat *transformValues);
    virtual void stencilThenCoverStrokePathInstanced(const std::vector<gl::Path *> &paths,
                                                     GLenum coverMode,
                                                     GLint reference,
                                                     GLuint mask,
                                                     GLenum transformType,
                                                     const GLfloat *transformValues);

    // Device loss
    virtual GLenum getResetStatus() = 0;

    // Vendor and description strings.
    virtual std::string getVendorString() const        = 0;
    virtual std::string getRendererDescription() const = 0;

    // EXT_debug_marker
    virtual void insertEventMarker(GLsizei length, const char *marker) = 0;
    virtual void pushGroupMarker(GLsizei length, const char *marker)   = 0;
    virtual void popGroupMarker() = 0;

    // KHR_debug
    virtual void pushDebugGroup(GLenum source, GLuint id, GLsizei length, const char *message) = 0;
    virtual void popDebugGroup()                                                               = 0;

    // State sync with dirty bits.
    virtual void syncState(const gl::Context *context, const gl::State::DirtyBits &dirtyBits) = 0;

    // Disjoint timer queries
    virtual GLint getGPUDisjoint() = 0;
    virtual GLint64 getTimestamp() = 0;

    // Context switching
    virtual void onMakeCurrent(const gl::Context *context) = 0;

    // Native capabilities, unmodified by gl::Context.
    virtual const gl::Caps &getNativeCaps() const                  = 0;
    virtual const gl::TextureCapsMap &getNativeTextureCaps() const = 0;
    virtual const gl::Extensions &getNativeExtensions() const      = 0;
    virtual const gl::Limitations &getNativeLimitations() const    = 0;

    virtual void applyNativeWorkarounds(gl::Workarounds *workarounds) const {}

    virtual gl::Error dispatchCompute(const gl::Context *context,
                                      GLuint numGroupsX,
                                      GLuint numGroupsY,
                                      GLuint numGroupsZ) = 0;

    const gl::ContextState &getContextState() { return mState; }
    int getClientMajorVersion() const { return mState.getClientMajorVersion(); }
    int getClientMinorVersion() const { return mState.getClientMinorVersion(); }
    const gl::State &getGLState() const { return mState.getState(); }
    const gl::Caps &getCaps() const { return mState.getCaps(); }
    const gl::TextureCapsMap &getTextureCaps() const { return mState.getTextureCaps(); }
    const gl::Extensions &getExtensions() const { return mState.getExtensions(); }
    const gl::Limitations &getLimitations() const { return mState.getLimitations(); }

    // A common GL driver behaviour is to trigger dynamic shader recompilation on a draw call,
    // based on the current render states. We store a mutable pointer to the program cache so
    // on draw calls we can store the refreshed shaders in the cache.
    void setMemoryProgramCache(gl::MemoryProgramCache *memoryProgramCache);

  protected:
    const gl::ContextState &mState;
    gl::MemoryProgramCache *mMemoryProgramCache;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_CONTEXTIMPL_H_
