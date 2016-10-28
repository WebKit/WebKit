//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererGL.cpp: Implements the class methods for RendererGL.

#include "libANGLE/renderer/gl/RendererGL.h"

#include <EGL/eglext.h>

#include "common/debug.h"
#include "libANGLE/AttributeMap.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/Path.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/gl/BlitGL.h"
#include "libANGLE/renderer/gl/BufferGL.h"
#include "libANGLE/renderer/gl/CompilerGL.h"
#include "libANGLE/renderer/gl/ContextGL.h"
#include "libANGLE/renderer/gl/FenceNVGL.h"
#include "libANGLE/renderer/gl/FenceSyncGL.h"
#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/FunctionsGL.h"
#include "libANGLE/renderer/gl/PathGL.h"
#include "libANGLE/renderer/gl/ProgramGL.h"
#include "libANGLE/renderer/gl/QueryGL.h"
#include "libANGLE/renderer/gl/RenderbufferGL.h"
#include "libANGLE/renderer/gl/SamplerGL.h"
#include "libANGLE/renderer/gl/ShaderGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/SurfaceGL.h"
#include "libANGLE/renderer/gl/TextureGL.h"
#include "libANGLE/renderer/gl/TransformFeedbackGL.h"
#include "libANGLE/renderer/gl/VertexArrayGL.h"
#include "libANGLE/renderer/gl/renderergl_utils.h"

namespace
{

std::vector<GLuint> GatherPaths(const std::vector<gl::Path *> &paths)
{
    std::vector<GLuint> ret;
    ret.reserve(paths.size());

    for (const auto *p : paths)
    {
        const auto *pathObj = rx::GetImplAs<rx::PathGL>(p);
        ret.push_back(pathObj->getPathID());
    }
    return ret;
}

}  // namespace

#ifndef NDEBUG
static void INTERNAL_GL_APIENTRY LogGLDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                                   const GLchar *message, const void *userParam)
{
    std::string sourceText;
    switch (source)
    {
      case GL_DEBUG_SOURCE_API:             sourceText = "OpenGL";          break;
      case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   sourceText = "Windows";         break;
      case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceText = "Shader Compiler"; break;
      case GL_DEBUG_SOURCE_THIRD_PARTY:     sourceText = "Third Party";     break;
      case GL_DEBUG_SOURCE_APPLICATION:     sourceText = "Application";     break;
      case GL_DEBUG_SOURCE_OTHER:           sourceText = "Other";           break;
      default:                              sourceText = "UNKNOWN";         break;
    }

    std::string typeText;
    switch (type)
    {
      case GL_DEBUG_TYPE_ERROR:               typeText = "Error";               break;
      case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeText = "Deprecated behavior"; break;
      case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeText = "Undefined behavior";  break;
      case GL_DEBUG_TYPE_PORTABILITY:         typeText = "Portability";         break;
      case GL_DEBUG_TYPE_PERFORMANCE:         typeText = "Performance";         break;
      case GL_DEBUG_TYPE_OTHER:               typeText = "Other";               break;
      case GL_DEBUG_TYPE_MARKER:              typeText = "Marker";              break;
      default:                                typeText = "UNKNOWN";             break;
    }

    std::string severityText;
    switch (severity)
    {
      case GL_DEBUG_SEVERITY_HIGH:         severityText = "High";         break;
      case GL_DEBUG_SEVERITY_MEDIUM:       severityText = "Medium";       break;
      case GL_DEBUG_SEVERITY_LOW:          severityText = "Low";          break;
      case GL_DEBUG_SEVERITY_NOTIFICATION: severityText = "Notification"; break;
      default:                             severityText = "UNKNOWN";      break;
    }

    ERR("\n\tSource: %s\n\tType: %s\n\tID: %d\n\tSeverity: %s\n\tMessage: %s", sourceText.c_str(), typeText.c_str(), id,
        severityText.c_str(), message);
}
#endif

namespace rx
{

RendererGL::RendererGL(const FunctionsGL *functions, const egl::AttributeMap &attribMap)
    : mMaxSupportedESVersion(0, 0),
      mFunctions(functions),
      mStateManager(nullptr),
      mBlitter(nullptr),
      mHasDebugOutput(false),
      mSkipDrawCalls(false),
      mCapsInitialized(false)
{
    ASSERT(mFunctions);
    mStateManager = new StateManagerGL(mFunctions, getNativeCaps());
    nativegl_gl::GenerateWorkarounds(mFunctions, &mWorkarounds);
    mBlitter = new BlitGL(functions, mWorkarounds, mStateManager);

    mHasDebugOutput = mFunctions->isAtLeastGL(gl::Version(4, 3)) ||
                      mFunctions->hasGLExtension("GL_KHR_debug") ||
                      mFunctions->isAtLeastGLES(gl::Version(3, 2)) ||
                      mFunctions->hasGLESExtension("GL_KHR_debug");
#ifndef NDEBUG
    if (mHasDebugOutput)
    {
        mFunctions->enable(GL_DEBUG_OUTPUT);
        mFunctions->enable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        mFunctions->debugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH, 0, nullptr, GL_TRUE);
        mFunctions->debugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr, GL_TRUE);
        mFunctions->debugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0, nullptr, GL_FALSE);
        mFunctions->debugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
        mFunctions->debugMessageCallback(&LogGLDebugMessage, nullptr);
    }
#endif

    EGLint deviceType =
        static_cast<EGLint>(attribMap.get(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_NONE));
    if (deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE)
    {
        mSkipDrawCalls = true;
    }

    if (mWorkarounds.initializeCurrentVertexAttributes)
    {
        GLint maxVertexAttribs = 0;
        mFunctions->getIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);

        for (GLint i = 0; i < maxVertexAttribs; ++i)
        {
            mFunctions->vertexAttrib4f(i, 0.0f, 0.0f, 0.0f, 1.0f);
        }
    }
}

RendererGL::~RendererGL()
{
    SafeDelete(mBlitter);
    SafeDelete(mStateManager);
}

gl::Error RendererGL::flush()
{
    mFunctions->flush();
    return gl::Error(GL_NO_ERROR);
}

gl::Error RendererGL::finish()
{
#ifdef NDEBUG
    if (mWorkarounds.finishDoesNotCauseQueriesToBeAvailable && mHasDebugOutput)
    {
        mFunctions->enable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    }
#endif

    mFunctions->finish();

#ifdef NDEBUG
    if (mWorkarounds.finishDoesNotCauseQueriesToBeAvailable && mHasDebugOutput)
    {
        mFunctions->disable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    }
#endif

    return gl::Error(GL_NO_ERROR);
}

gl::Error RendererGL::drawArrays(const gl::ContextState &data,
                                 GLenum mode,
                                 GLint first,
                                 GLsizei count)
{
    ANGLE_TRY(mStateManager->setDrawArraysState(data, first, count, 0));

    if (!mSkipDrawCalls)
    {
        mFunctions->drawArrays(mode, first, count);
    }

    return gl::NoError();
}

gl::Error RendererGL::drawArraysInstanced(const gl::ContextState &data,
                                          GLenum mode,
                                          GLint first,
                                          GLsizei count,
                                          GLsizei instanceCount)
{
    ANGLE_TRY(mStateManager->setDrawArraysState(data, first, count, instanceCount));

    if (!mSkipDrawCalls)
    {
        mFunctions->drawArraysInstanced(mode, first, count, instanceCount);
    }

    return gl::NoError();
}

gl::Error RendererGL::drawElements(const gl::ContextState &data,
                                   GLenum mode,
                                   GLsizei count,
                                   GLenum type,
                                   const GLvoid *indices,
                                   const gl::IndexRange &indexRange)
{
    const GLvoid *drawIndexPtr = nullptr;
    ANGLE_TRY(mStateManager->setDrawElementsState(data, count, type, indices, 0, &drawIndexPtr));

    if (!mSkipDrawCalls)
    {
        mFunctions->drawElements(mode, count, type, drawIndexPtr);
    }

    return gl::NoError();
}

gl::Error RendererGL::drawElementsInstanced(const gl::ContextState &data,
                                            GLenum mode,
                                            GLsizei count,
                                            GLenum type,
                                            const GLvoid *indices,
                                            GLsizei instances,
                                            const gl::IndexRange &indexRange)
{
    const GLvoid *drawIndexPointer = nullptr;
    ANGLE_TRY(mStateManager->setDrawElementsState(data, count, type, indices, instances,
                                                  &drawIndexPointer));

    if (!mSkipDrawCalls)
    {
        mFunctions->drawElementsInstanced(mode, count, type, drawIndexPointer, instances);
    }

    return gl::NoError();
}

gl::Error RendererGL::drawRangeElements(const gl::ContextState &data,
                                        GLenum mode,
                                        GLuint start,
                                        GLuint end,
                                        GLsizei count,
                                        GLenum type,
                                        const GLvoid *indices,
                                        const gl::IndexRange &indexRange)
{
    const GLvoid *drawIndexPointer = nullptr;
    ANGLE_TRY(
        mStateManager->setDrawElementsState(data, count, type, indices, 0, &drawIndexPointer));

    if (!mSkipDrawCalls)
    {
        mFunctions->drawRangeElements(mode, start, end, count, type, drawIndexPointer);
    }

    return gl::Error(GL_NO_ERROR);
}

void RendererGL::stencilFillPath(const gl::ContextState &state,
                                 const gl::Path *path,
                                 GLenum fillMode,
                                 GLuint mask)
{
    const auto *pathObj = GetImplAs<PathGL>(path);

    mFunctions->stencilFillPathNV(pathObj->getPathID(), fillMode, mask);

    ASSERT(mFunctions->getError() == GL_NO_ERROR);
}

void RendererGL::stencilStrokePath(const gl::ContextState &state,
                                   const gl::Path *path,
                                   GLint reference,
                                   GLuint mask)
{
    const auto *pathObj = GetImplAs<PathGL>(path);

    mFunctions->stencilStrokePathNV(pathObj->getPathID(), reference, mask);

    ASSERT(mFunctions->getError() == GL_NO_ERROR);
}

void RendererGL::coverFillPath(const gl::ContextState &state,
                               const gl::Path *path,
                               GLenum coverMode)
{

    const auto *pathObj = GetImplAs<PathGL>(path);
    mFunctions->coverFillPathNV(pathObj->getPathID(), coverMode);

    ASSERT(mFunctions->getError() == GL_NO_ERROR);
}

void RendererGL::coverStrokePath(const gl::ContextState &state,
                                 const gl::Path *path,
                                 GLenum coverMode)
{
    const auto *pathObj = GetImplAs<PathGL>(path);
    mFunctions->coverStrokePathNV(pathObj->getPathID(), coverMode);

    ASSERT(mFunctions->getError() == GL_NO_ERROR);
}

void RendererGL::stencilThenCoverFillPath(const gl::ContextState &state,
                                          const gl::Path *path,
                                          GLenum fillMode,
                                          GLuint mask,
                                          GLenum coverMode)
{

    const auto *pathObj = GetImplAs<PathGL>(path);
    mFunctions->stencilThenCoverFillPathNV(pathObj->getPathID(), fillMode, mask, coverMode);

    ASSERT(mFunctions->getError() == GL_NO_ERROR);
}

void RendererGL::stencilThenCoverStrokePath(const gl::ContextState &state,
                                            const gl::Path *path,
                                            GLint reference,
                                            GLuint mask,
                                            GLenum coverMode)
{

    const auto *pathObj = GetImplAs<PathGL>(path);
    mFunctions->stencilThenCoverStrokePathNV(pathObj->getPathID(), reference, mask, coverMode);

    ASSERT(mFunctions->getError() == GL_NO_ERROR);
}

void RendererGL::coverFillPathInstanced(const gl::ContextState &state,
                                        const std::vector<gl::Path *> &paths,
                                        GLenum coverMode,
                                        GLenum transformType,
                                        const GLfloat *transformValues)
{
    const auto &pathObjs = GatherPaths(paths);

    mFunctions->coverFillPathInstancedNV(static_cast<GLsizei>(pathObjs.size()), GL_UNSIGNED_INT,
                                         &pathObjs[0], 0, coverMode, transformType,
                                         transformValues);

    ASSERT(mFunctions->getError() == GL_NO_ERROR);
}
void RendererGL::coverStrokePathInstanced(const gl::ContextState &state,
                                          const std::vector<gl::Path *> &paths,
                                          GLenum coverMode,
                                          GLenum transformType,
                                          const GLfloat *transformValues)
{
    const auto &pathObjs = GatherPaths(paths);

    mFunctions->coverStrokePathInstancedNV(static_cast<GLsizei>(pathObjs.size()), GL_UNSIGNED_INT,
                                           &pathObjs[0], 0, coverMode, transformType,
                                           transformValues);

    ASSERT(mFunctions->getError() == GL_NO_ERROR);
}
void RendererGL::stencilFillPathInstanced(const gl::ContextState &state,
                                          const std::vector<gl::Path *> &paths,
                                          GLenum fillMode,
                                          GLuint mask,
                                          GLenum transformType,
                                          const GLfloat *transformValues)
{
    const auto &pathObjs = GatherPaths(paths);

    mFunctions->stencilFillPathInstancedNV(static_cast<GLsizei>(pathObjs.size()), GL_UNSIGNED_INT,
                                           &pathObjs[0], 0, fillMode, mask, transformType,
                                           transformValues);

    ASSERT(mFunctions->getError() == GL_NO_ERROR);
}
void RendererGL::stencilStrokePathInstanced(const gl::ContextState &state,
                                            const std::vector<gl::Path *> &paths,
                                            GLint reference,
                                            GLuint mask,
                                            GLenum transformType,
                                            const GLfloat *transformValues)
{
    const auto &pathObjs = GatherPaths(paths);

    mFunctions->stencilStrokePathInstancedNV(static_cast<GLsizei>(pathObjs.size()), GL_UNSIGNED_INT,
                                             &pathObjs[0], 0, reference, mask, transformType,
                                             transformValues);

    ASSERT(mFunctions->getError() == GL_NO_ERROR);
}

void RendererGL::stencilThenCoverFillPathInstanced(const gl::ContextState &state,
                                                   const std::vector<gl::Path *> &paths,
                                                   GLenum coverMode,
                                                   GLenum fillMode,
                                                   GLuint mask,
                                                   GLenum transformType,
                                                   const GLfloat *transformValues)
{
    const auto &pathObjs = GatherPaths(paths);

    mFunctions->stencilThenCoverFillPathInstancedNV(
        static_cast<GLsizei>(pathObjs.size()), GL_UNSIGNED_INT, &pathObjs[0], 0, fillMode, mask,
        coverMode, transformType, transformValues);

    ASSERT(mFunctions->getError() == GL_NO_ERROR);
}
void RendererGL::stencilThenCoverStrokePathInstanced(const gl::ContextState &state,
                                                     const std::vector<gl::Path *> &paths,
                                                     GLenum coverMode,
                                                     GLint reference,
                                                     GLuint mask,
                                                     GLenum transformType,
                                                     const GLfloat *transformValues)
{
    const auto &pathObjs = GatherPaths(paths);

    mFunctions->stencilThenCoverStrokePathInstancedNV(
        static_cast<GLsizei>(pathObjs.size()), GL_UNSIGNED_INT, &pathObjs[0], 0, reference, mask,
        coverMode, transformType, transformValues);

    ASSERT(mFunctions->getError() == GL_NO_ERROR);
}

GLenum RendererGL::getResetStatus()
{
    return mFunctions->getGraphicsResetStatus();
}

ContextImpl *RendererGL::createContext(const gl::ContextState &state)
{
    return new ContextGL(state, this);
}

void RendererGL::insertEventMarker(GLsizei length, const char *marker)
{
    mFunctions->debugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
                                   GL_DEBUG_SEVERITY_NOTIFICATION, length, marker);
}

void RendererGL::pushGroupMarker(GLsizei length, const char *marker)
{
    mFunctions->pushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, length, marker);
}

void RendererGL::popGroupMarker()
{
    mFunctions->popDebugGroup();
}

std::string RendererGL::getVendorString() const
{
    return std::string(reinterpret_cast<const char*>(mFunctions->getString(GL_VENDOR)));
}

std::string RendererGL::getRendererDescription() const
{
    std::string nativeVendorString(reinterpret_cast<const char*>(mFunctions->getString(GL_VENDOR)));
    std::string nativeRendererString(reinterpret_cast<const char*>(mFunctions->getString(GL_RENDERER)));

    std::ostringstream rendererString;
    rendererString << nativeVendorString << " " << nativeRendererString << " OpenGL";
    if (mFunctions->standard == STANDARD_GL_ES)
    {
        rendererString << " ES";
    }
    rendererString << " " << mFunctions->version.major << "." << mFunctions->version.minor;
    if (mFunctions->standard == STANDARD_GL_DESKTOP)
    {
        // Some drivers (NVIDIA) use a profile mask of 0 when in compatibility profile.
        if ((mFunctions->profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) != 0 ||
            (mFunctions->isAtLeastGL(gl::Version(3, 2)) && mFunctions->profile == 0))
        {
            rendererString << " compatibility";
        }
        else if ((mFunctions->profile & GL_CONTEXT_CORE_PROFILE_BIT) != 0)
        {
            rendererString << " core";
        }
    }

    return rendererString.str();
}

const gl::Version &RendererGL::getMaxSupportedESVersion() const
{
    // Force generation of caps
    getNativeCaps();

    return mMaxSupportedESVersion;
}

void RendererGL::generateCaps(gl::Caps *outCaps, gl::TextureCapsMap* outTextureCaps,
                              gl::Extensions *outExtensions,
                              gl::Limitations * /* outLimitations */) const
{
    nativegl_gl::GenerateCaps(mFunctions, outCaps, outTextureCaps, outExtensions, &mMaxSupportedESVersion);
}

GLint RendererGL::getGPUDisjoint()
{
    // TODO(ewell): On GLES backends we should find a way to reliably query disjoint events
    return 0;
}

GLint64 RendererGL::getTimestamp()
{
    GLint64 result = 0;
    mFunctions->getInteger64v(GL_TIMESTAMP, &result);
    return result;
}

void RendererGL::ensureCapsInitialized() const
{
    if (!mCapsInitialized)
    {
        generateCaps(&mNativeCaps, &mNativeTextureCaps, &mNativeExtensions, &mNativeLimitations);
        mCapsInitialized = true;
    }
}

const gl::Caps &RendererGL::getNativeCaps() const
{
    ensureCapsInitialized();
    return mNativeCaps;
}

const gl::TextureCapsMap &RendererGL::getNativeTextureCaps() const
{
    ensureCapsInitialized();
    return mNativeTextureCaps;
}

const gl::Extensions &RendererGL::getNativeExtensions() const
{
    ensureCapsInitialized();
    return mNativeExtensions;
}

const gl::Limitations &RendererGL::getNativeLimitations() const
{
    ensureCapsInitialized();
    return mNativeLimitations;
}

}  // namespace rx
