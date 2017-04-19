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
class Path;
}

namespace rx
{
class ContextImpl : public GLImplFactory
{
  public:
    ContextImpl(const gl::ContextState &state);
    virtual ~ContextImpl();

    virtual gl::Error initialize() = 0;

    // Flush and finish.
    virtual gl::Error flush() = 0;
    virtual gl::Error finish() = 0;

    // Drawing methods.
    virtual gl::Error drawArrays(GLenum mode, GLint first, GLsizei count) = 0;
    virtual gl::Error drawArraysInstanced(GLenum mode,
                                          GLint first,
                                          GLsizei count,
                                          GLsizei instanceCount) = 0;

    virtual gl::Error drawElements(GLenum mode,
                                   GLsizei count,
                                   GLenum type,
                                   const GLvoid *indices,
                                   const gl::IndexRange &indexRange) = 0;
    virtual gl::Error drawElementsInstanced(GLenum mode,
                                            GLsizei count,
                                            GLenum type,
                                            const GLvoid *indices,
                                            GLsizei instances,
                                            const gl::IndexRange &indexRange) = 0;
    virtual gl::Error drawRangeElements(GLenum mode,
                                        GLuint start,
                                        GLuint end,
                                        GLsizei count,
                                        GLenum type,
                                        const GLvoid *indices,
                                        const gl::IndexRange &indexRange) = 0;

    virtual gl::Error drawArraysIndirect(GLenum mode, const GLvoid *indirect) = 0;
    virtual gl::Error drawElementsIndirect(GLenum mode, GLenum type, const GLvoid *indirect) = 0;

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
    virtual std::string getVendorString() const = 0;
    virtual std::string getRendererDescription() const = 0;

    // Debug markers.
    virtual void insertEventMarker(GLsizei length, const char *marker) = 0;
    virtual void pushGroupMarker(GLsizei length, const char *marker) = 0;
    virtual void popGroupMarker() = 0;

    // State sync with dirty bits.
    virtual void syncState(const gl::State::DirtyBits &dirtyBits) = 0;

    // Disjoint timer queries
    virtual GLint getGPUDisjoint() = 0;
    virtual GLint64 getTimestamp() = 0;

    // Context switching
    virtual void onMakeCurrent(const gl::ContextState &data) = 0;

    // Native capabilities, unmodified by gl::Context.
    virtual const gl::Caps &getNativeCaps() const = 0;
    virtual const gl::TextureCapsMap &getNativeTextureCaps() const = 0;
    virtual const gl::Extensions &getNativeExtensions() const = 0;
    virtual const gl::Limitations &getNativeLimitations() const = 0;

    virtual gl::Error dispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ) = 0;

    const gl::ContextState &getContextState() { return mState; }
    int getClientMajorVersion() const { return mState.getClientMajorVersion(); }
    int getClientMinorVersion() const { return mState.getClientMinorVersion(); }
    const gl::State &getGLState() const { return mState.getState(); }
    const gl::Caps &getCaps() const { return mState.getCaps(); }
    const gl::TextureCapsMap &getTextureCaps() const { return mState.getTextureCaps(); }
    const gl::Extensions &getExtensions() const { return mState.getExtensions(); }
    const gl::Limitations &getLimitations() const { return mState.getLimitations(); }

  protected:
    const gl::ContextState &mState;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_CONTEXTIMPL_H_
