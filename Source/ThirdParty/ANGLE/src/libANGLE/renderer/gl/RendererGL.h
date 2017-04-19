//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererGL.h: Defines the class interface for RendererGL.

#ifndef LIBANGLE_RENDERER_GL_RENDERERGL_H_
#define LIBANGLE_RENDERER_GL_RENDERERGL_H_

#include "libANGLE/Caps.h"
#include "libANGLE/Error.h"
#include "libANGLE/Version.h"
#include "libANGLE/renderer/gl/WorkaroundsGL.h"

namespace gl
{
class ContextState;
struct IndexRange;
class Path;
}

namespace egl
{
class AttributeMap;
}

namespace sh
{
struct BlockMemberInfo;
}

namespace rx
{
class BlitGL;
class ContextImpl;
class FunctionsGL;
class StateManagerGL;

class RendererGL : angle::NonCopyable
{
  public:
    RendererGL(const FunctionsGL *functions, const egl::AttributeMap &attribMap);
    ~RendererGL();

    ContextImpl *createContext(const gl::ContextState &state);

    gl::Error flush();
    gl::Error finish();

    gl::Error drawArrays(const gl::ContextState &data, GLenum mode, GLint first, GLsizei count);
    gl::Error drawArraysInstanced(const gl::ContextState &data,
                                  GLenum mode,
                                  GLint first,
                                  GLsizei count,
                                  GLsizei instanceCount);

    gl::Error drawElements(const gl::ContextState &data,
                           GLenum mode,
                           GLsizei count,
                           GLenum type,
                           const GLvoid *indices,
                           const gl::IndexRange &indexRange);
    gl::Error drawElementsInstanced(const gl::ContextState &data,
                                    GLenum mode,
                                    GLsizei count,
                                    GLenum type,
                                    const GLvoid *indices,
                                    GLsizei instances,
                                    const gl::IndexRange &indexRange);
    gl::Error drawRangeElements(const gl::ContextState &data,
                                GLenum mode,
                                GLuint start,
                                GLuint end,
                                GLsizei count,
                                GLenum type,
                                const GLvoid *indices,
                                const gl::IndexRange &indexRange);
    gl::Error drawArraysIndirect(const gl::ContextState &data, GLenum mode, const GLvoid *indirect);
    gl::Error drawElementsIndirect(const gl::ContextState &data,
                                   GLenum mode,
                                   GLenum type,
                                   const GLvoid *indirect);

    // CHROMIUM_path_rendering implementation
    void stencilFillPath(const gl::ContextState &state,
                         const gl::Path *path,
                         GLenum fillMode,
                         GLuint mask);
    void stencilStrokePath(const gl::ContextState &state,
                           const gl::Path *path,
                           GLint reference,
                           GLuint mask);
    void coverFillPath(const gl::ContextState &state, const gl::Path *path, GLenum coverMode);
    void coverStrokePath(const gl::ContextState &state, const gl::Path *path, GLenum coverMode);
    void stencilThenCoverFillPath(const gl::ContextState &state,
                                  const gl::Path *path,
                                  GLenum fillMode,
                                  GLuint mask,
                                  GLenum coverMode);
    void stencilThenCoverStrokePath(const gl::ContextState &state,
                                    const gl::Path *path,
                                    GLint reference,
                                    GLuint mask,
                                    GLenum coverMode);
    void coverFillPathInstanced(const gl::ContextState &state,
                                const std::vector<gl::Path *> &paths,
                                GLenum coverMode,
                                GLenum transformType,
                                const GLfloat *transformValues);
    void coverStrokePathInstanced(const gl::ContextState &state,
                                  const std::vector<gl::Path *> &paths,
                                  GLenum coverMode,
                                  GLenum transformType,
                                  const GLfloat *transformValues);
    void stencilFillPathInstanced(const gl::ContextState &state,
                                  const std::vector<gl::Path *> &paths,
                                  GLenum fillMode,
                                  GLuint mask,
                                  GLenum transformType,
                                  const GLfloat *transformValues);
    void stencilStrokePathInstanced(const gl::ContextState &state,
                                    const std::vector<gl::Path *> &paths,
                                    GLint reference,
                                    GLuint mask,
                                    GLenum transformType,
                                    const GLfloat *transformValues);

    void stencilThenCoverFillPathInstanced(const gl::ContextState &state,
                                           const std::vector<gl::Path *> &paths,
                                           GLenum coverMode,
                                           GLenum fillMode,
                                           GLuint mask,
                                           GLenum transformType,
                                           const GLfloat *transformValues);
    void stencilThenCoverStrokePathInstanced(const gl::ContextState &state,
                                             const std::vector<gl::Path *> &paths,
                                             GLenum coverMode,
                                             GLint reference,
                                             GLuint mask,
                                             GLenum transformType,
                                             const GLfloat *transformValues);

    GLenum getResetStatus();

    // EXT_debug_marker
    void insertEventMarker(GLsizei length, const char *marker);
    void pushGroupMarker(GLsizei length, const char *marker);
    void popGroupMarker();

    std::string getVendorString() const;
    std::string getRendererDescription() const;

    GLint getGPUDisjoint();
    GLint64 getTimestamp();

    const gl::Version &getMaxSupportedESVersion() const;
    const FunctionsGL *getFunctions() const { return mFunctions; }
    StateManagerGL *getStateManager() const { return mStateManager; }
    const WorkaroundsGL &getWorkarounds() const { return mWorkarounds; }
    BlitGL *getBlitter() const { return mBlitter; }

    const gl::Caps &getNativeCaps() const;
    const gl::TextureCapsMap &getNativeTextureCaps() const;
    const gl::Extensions &getNativeExtensions() const;
    const gl::Limitations &getNativeLimitations() const;

    gl::Error dispatchCompute(const gl::ContextState &data,
                              GLuint numGroupsX,
                              GLuint numGroupsY,
                              GLuint numGroupsZ);

  private:
    void ensureCapsInitialized() const;
    void generateCaps(gl::Caps *outCaps,
                      gl::TextureCapsMap *outTextureCaps,
                      gl::Extensions *outExtensions,
                      gl::Limitations *outLimitations) const;

    mutable gl::Version mMaxSupportedESVersion;

    const FunctionsGL *mFunctions;
    StateManagerGL *mStateManager;

    BlitGL *mBlitter;

    WorkaroundsGL mWorkarounds;

    bool mHasDebugOutput;

    // For performance debugging
    bool mSkipDrawCalls;

    mutable bool mCapsInitialized;
    mutable gl::Caps mNativeCaps;
    mutable gl::TextureCapsMap mNativeTextureCaps;
    mutable gl::Extensions mNativeExtensions;
    mutable gl::Limitations mNativeLimitations;
};

}  // namespace rx

#endif // LIBANGLE_RENDERER_GL_RENDERERGL_H_
