//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TracePerfTest.cpp:
//   Performance test for ANGLE replaying traces.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "tests/perf_tests/TracePerfTest.h"
#include <gtest/gtest.h>
#include "common/PackedEnums.h"
#include "common/string_utils.h"
#include "common/system_utils.h"
#include "tests/perf_tests/ANGLEPerfTest.h"
#include "tests/perf_tests/ANGLEPerfTestArgs.h"
#include "tests/perf_tests/DrawCallPerfParams.h"
#include "util/capture/frame_capture_test_utils.h"
#include "util/capture/traces_export.h"
#include "util/egl_loader_autogen.h"
#include "util/png_utils.h"
#include "util/test_utils.h"

#if defined(ANGLE_PLATFORM_ANDROID)
#    include "util/android/AndroidWindow.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include <cassert>
#include <fstream>
#include <functional>
#include <sstream>

// When --minimize-gpu-work is specified, we want to reduce GPU work to minimum and lift up the CPU
// overhead to surface so that we can see how much CPU overhead each driver has for each app trace.
// On some driver(s) the bufferSubData/texSubImage calls end up dominating the frame time when the
// actual GPU work is minimized. Even reducing the texSubImage calls to only update 1x1 area is not
// enough. The driver may be implementing copy on write by cloning the entire texture to another
// memory storage for texSubImage call. While this information is also important for performance,
// they should be evaluated separately in real app usage scenario, or write stand alone tests for
// these. For the purpose of CPU overhead and avoid data copy to dominate the trace, I am using this
// flag to noop the texSubImage and bufferSubData call when --minimize-gpu-work is specified. Feel
// free to disable this when you have other needs. Or it can be turned to another run time option
// when desired.
#define NOOP_SUBDATA_SUBIMAGE_FOR_MINIMIZE_GPU_WORK

using namespace angle;
using namespace egl_platform;

namespace
{

class TracePerfTest : public ANGLERenderTest
{
  public:
    TracePerfTest(std::unique_ptr<const TracePerfParams> params);

    void startTest() override;
    void initializeBenchmark() override;
    void destroyBenchmark() override;
    void drawBenchmark() override;

    // TODO(http://anglebug.com/42264418): Add support for creating EGLSurface:
    // - eglCreatePbufferSurface()
    // - eglCreateWindowSurface()
    EGLContext onEglCreateContext(EGLDisplay display,
                                  EGLConfig config,
                                  EGLContext share_context,
                                  EGLint const *attrib_list);
    void onEglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context);
    EGLContext onEglGetCurrentContext();
    EGLImage onEglCreateImage(EGLDisplay display,
                              EGLContext context,
                              EGLenum target,
                              EGLClientBuffer buffer,
                              const EGLAttrib *attrib_list);
    EGLImageKHR onEglCreateImageKHR(EGLDisplay display,
                                    EGLContext context,
                                    EGLenum target,
                                    EGLClientBuffer buffer,
                                    const EGLint *attrib_list);
    EGLBoolean onEglDestroyImage(EGLDisplay display, EGLImage image);
    EGLBoolean onEglDestroyImageKHR(EGLDisplay display, EGLImage image);
    EGLSync onEglCreateSync(EGLDisplay dpy, EGLenum type, const EGLAttrib *attrib_list);
    EGLSync onEglCreateSyncKHR(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list);
    EGLBoolean onEglDestroySync(EGLDisplay dpy, EGLSync sync);
    EGLBoolean onEglDestroySyncKHR(EGLDisplay dpy, EGLSync sync);
    EGLint onEglClientWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTimeKHR timeout);
    EGLint onEglClientWaitSyncKHR(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTimeKHR timeout);
    EGLint onEglGetError();
    EGLDisplay onEglGetCurrentDisplay();

    void onReplayFramebufferChange(GLenum target, GLuint framebuffer);
    void onReplayInvalidateFramebuffer(GLenum target,
                                       GLsizei numAttachments,
                                       const GLenum *attachments);
    void onReplayInvalidateSubFramebuffer(GLenum target,
                                          GLsizei numAttachments,
                                          const GLenum *attachments,
                                          GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height);
    void onReplayDrawBuffers(GLsizei n, const GLenum *bufs);
    void onReplayReadBuffer(GLenum src);
    void onReplayDiscardFramebufferEXT(GLenum target,
                                       GLsizei numAttachments,
                                       const GLenum *attachments);

    void validateSerializedState(const char *serializedState, const char *fileName, uint32_t line);

    bool isDefaultFramebuffer(GLenum target) const;

    double getHostTimeFromGLTime(GLint64 glTime);

    uint32_t frameCount() const
    {
        const TraceInfo &traceInfo = mParams->traceInfo;
        return traceInfo.frameEnd - traceInfo.frameStart + 1;
    }

    int getStepAlignment() const override
    {
        // Align step counts to the number of frames in a trace.
        return static_cast<int>(frameCount());
    }

    void TestBody() override { run(); }

    bool traceNameIs(const char *name) const
    {
        return strncmp(name, mParams->traceInfo.name, kTraceInfoMaxNameLen) == 0;
    }

    bool loadTestExpectationsFromFileWithConfig(const GPUTestConfig &config,
                                                const std::string &fileName);
    void initializeConfigParams(GPUTestConfig::API api);

  private:
    struct QueryInfo
    {
        GLuint beginTimestampQuery;
        GLuint endTimestampQuery;
        GLuint framebuffer;
    };

    struct TimeSample
    {
        GLint64 glTime;
        double hostTime;
    };

    void sampleTime();

    enum class ScreenshotType
    {
        kFrame,
        kGrid,  // Grid of frames (framebuffer 0) in offscreen mode
    };
    void saveScreenshotIfEnabled(ScreenshotType screenshotType);
    void saveScreenshot(const std::string &screenshotName);

    std::unique_ptr<const TracePerfParams> mParams;

    uint32_t mStartFrame;
    uint32_t mEndFrame;

    // For tracking RenderPass/FBO change timing.
    QueryInfo mCurrentQuery = {};
    std::vector<QueryInfo> mRunningQueries;
    std::vector<TimeSample> mTimeline;

    bool mUseTimestampQueries                                           = false;
    // Note: more than 2 offscreen buffers can cause races, surface is double buffered so real-world
    // apps can rely on (now broken) assumptions about GPU completion of a previous frame
    static constexpr int mMaxOffscreenBufferCount                       = 2;
    std::array<GLuint, mMaxOffscreenBufferCount> mOffscreenFramebuffers = {0, 0};
    std::array<GLuint, mMaxOffscreenBufferCount> mOffscreenTextures     = {0, 0};
    std::array<GLsync, mMaxOffscreenBufferCount> mOffscreenSyncs        = {0, 0};
    GLuint mOffscreenDepthStencil                                       = 0;
    int mWindowWidth                                                    = 0;
    int mWindowHeight                                                   = 0;
    GLuint mDrawFramebufferBinding                                      = 0;
    GLuint mReadFramebufferBinding                                      = 0;
    EGLContext mEglContext                                              = 0;
    uint32_t mCurrentFrame                                              = 0;
    uint32_t mCurrentIteration                                          = 0;
    uint32_t mCurrentOffscreenGridIteration                             = 0;
    uint32_t mOffscreenFrameCount                                       = 0;
    uint32_t mTotalFrameCount                                           = 0;
    bool mScreenshotSaved                                               = false;
    int32_t mScreenshotFrame                                            = gScreenshotFrame;
    std::unique_ptr<TraceLibrary> mTraceReplay;
    GPUTestExpectationsParser mTestExpectationsParser;
};

TracePerfTest *gCurrentTracePerfTest = nullptr;

// Don't forget to include KHRONOS_APIENTRY in override methods. Necessary on Win/x86.
EGLContext KHRONOS_APIENTRY EglCreateContext(EGLDisplay display,
                                             EGLConfig config,
                                             EGLContext share_context,
                                             EGLint const *attrib_list)
{
    return gCurrentTracePerfTest->onEglCreateContext(display, config, share_context, attrib_list);
}

void KHRONOS_APIENTRY EglMakeCurrent(EGLDisplay display,
                                     EGLSurface draw,
                                     EGLSurface read,
                                     EGLContext context)
{
    gCurrentTracePerfTest->onEglMakeCurrent(display, draw, read, context);
}

EGLContext KHRONOS_APIENTRY EglGetCurrentContext()
{
    return gCurrentTracePerfTest->onEglGetCurrentContext();
}

EGLImage KHRONOS_APIENTRY EglCreateImage(EGLDisplay display,
                                         EGLContext context,
                                         EGLenum target,
                                         EGLClientBuffer buffer,
                                         const EGLAttrib *attrib_list)
{
    return gCurrentTracePerfTest->onEglCreateImage(display, context, target, buffer, attrib_list);
}

EGLImageKHR KHRONOS_APIENTRY EglCreateImageKHR(EGLDisplay display,
                                               EGLContext context,
                                               EGLenum target,
                                               EGLClientBuffer buffer,
                                               const EGLint *attrib_list)
{
    return gCurrentTracePerfTest->onEglCreateImageKHR(display, context, target, buffer,
                                                      attrib_list);
}

EGLBoolean KHRONOS_APIENTRY EglDestroyImage(EGLDisplay display, EGLImage image)
{
    return gCurrentTracePerfTest->onEglDestroyImage(display, image);
}

EGLBoolean KHRONOS_APIENTRY EglDestroyImageKHR(EGLDisplay display, EGLImage image)
{
    return gCurrentTracePerfTest->onEglDestroyImageKHR(display, image);
}

EGLSync KHRONOS_APIENTRY EglCreateSync(EGLDisplay dpy, EGLenum type, const EGLAttrib *attrib_list)
{
    return gCurrentTracePerfTest->onEglCreateSync(dpy, type, attrib_list);
}

EGLSync KHRONOS_APIENTRY EglCreateSyncKHR(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list)
{
    return gCurrentTracePerfTest->onEglCreateSyncKHR(dpy, type, attrib_list);
}

EGLBoolean KHRONOS_APIENTRY EglDestroySync(EGLDisplay dpy, EGLSync sync)
{
    return gCurrentTracePerfTest->onEglDestroySync(dpy, sync);
}

EGLBoolean KHRONOS_APIENTRY EglDestroySyncKHR(EGLDisplay dpy, EGLSync sync)
{
    return gCurrentTracePerfTest->onEglDestroySyncKHR(dpy, sync);
}

EGLint KHRONOS_APIENTRY EglClientWaitSync(EGLDisplay dpy,
                                          EGLSync sync,
                                          EGLint flags,
                                          EGLTimeKHR timeout)
{
    return gCurrentTracePerfTest->onEglClientWaitSync(dpy, sync, flags, timeout);
}
EGLint KHRONOS_APIENTRY EglClientWaitSyncKHR(EGLDisplay dpy,
                                             EGLSync sync,
                                             EGLint flags,
                                             EGLTimeKHR timeout)
{
    return gCurrentTracePerfTest->onEglClientWaitSyncKHR(dpy, sync, flags, timeout);
}

EGLint KHRONOS_APIENTRY EglGetError()
{
    return gCurrentTracePerfTest->onEglGetError();
}

EGLDisplay KHRONOS_APIENTRY EglGetCurrentDisplay()
{
    return gCurrentTracePerfTest->onEglGetCurrentDisplay();
}

void KHRONOS_APIENTRY BindFramebufferProc(GLenum target, GLuint framebuffer)
{
    gCurrentTracePerfTest->onReplayFramebufferChange(target, framebuffer);
}

void KHRONOS_APIENTRY InvalidateFramebufferProc(GLenum target,
                                                GLsizei numAttachments,
                                                const GLenum *attachments)
{
    gCurrentTracePerfTest->onReplayInvalidateFramebuffer(target, numAttachments, attachments);
}

void KHRONOS_APIENTRY InvalidateSubFramebufferProc(GLenum target,
                                                   GLsizei numAttachments,
                                                   const GLenum *attachments,
                                                   GLint x,
                                                   GLint y,
                                                   GLsizei width,
                                                   GLsizei height)
{
    gCurrentTracePerfTest->onReplayInvalidateSubFramebuffer(target, numAttachments, attachments, x,
                                                            y, width, height);
}

void KHRONOS_APIENTRY DrawBuffersProc(GLsizei n, const GLenum *bufs)
{
    gCurrentTracePerfTest->onReplayDrawBuffers(n, bufs);
}

void KHRONOS_APIENTRY ReadBufferProc(GLenum src)
{
    gCurrentTracePerfTest->onReplayReadBuffer(src);
}

void KHRONOS_APIENTRY DiscardFramebufferEXTProc(GLenum target,
                                                GLsizei numAttachments,
                                                const GLenum *attachments)
{
    gCurrentTracePerfTest->onReplayDiscardFramebufferEXT(target, numAttachments, attachments);
}

void KHRONOS_APIENTRY ViewportMinimizedProc(GLint x, GLint y, GLsizei width, GLsizei height)
{
    glViewport(x, y, 1, 1);
}

void KHRONOS_APIENTRY ScissorMinimizedProc(GLint x, GLint y, GLsizei width, GLsizei height)
{
    glScissor(x, y, 1, 1);
}

// Interpose the calls that generate actual GPU work
void KHRONOS_APIENTRY DrawElementsMinimizedProc(GLenum mode,
                                                GLsizei count,
                                                GLenum type,
                                                const void *indices)
{
    glDrawElements(GL_POINTS, 1, type, indices);
}

void KHRONOS_APIENTRY DrawElementsIndirectMinimizedProc(GLenum mode,
                                                        GLenum type,
                                                        const void *indirect)
{
    glDrawElementsInstancedBaseVertex(GL_POINTS, 1, type, 0, 1, 0);
}

void KHRONOS_APIENTRY DrawElementsInstancedMinimizedProc(GLenum mode,
                                                         GLsizei count,
                                                         GLenum type,
                                                         const void *indices,
                                                         GLsizei instancecount)
{
    glDrawElementsInstanced(GL_POINTS, 1, type, indices, 1);
}

void KHRONOS_APIENTRY DrawElementsInstancedEXTMinimizedProc(GLenum mode,
                                                            GLsizei count,
                                                            GLenum type,
                                                            const void *indices,
                                                            GLsizei instancecount)
{
    glDrawElementsInstancedEXT(GL_POINTS, 1, type, indices, 1);
}

void KHRONOS_APIENTRY DrawElementsBaseVertexMinimizedProc(GLenum mode,
                                                          GLsizei count,
                                                          GLenum type,
                                                          const void *indices,
                                                          GLint basevertex)
{
    glDrawElementsBaseVertex(GL_POINTS, 1, type, indices, basevertex);
}

void KHRONOS_APIENTRY DrawElementsBaseVertexEXTMinimizedProc(GLenum mode,
                                                             GLsizei count,
                                                             GLenum type,
                                                             const void *indices,
                                                             GLint basevertex)
{
    glDrawElementsBaseVertexEXT(GL_POINTS, 1, type, indices, basevertex);
}

void KHRONOS_APIENTRY DrawElementsBaseVertexOESMinimizedProc(GLenum mode,
                                                             GLsizei count,
                                                             GLenum type,
                                                             const void *indices,
                                                             GLint basevertex)
{
    glDrawElementsBaseVertexOES(GL_POINTS, 1, type, indices, basevertex);
}

void KHRONOS_APIENTRY DrawElementsInstancedBaseVertexMinimizedProc(GLenum mode,
                                                                   GLsizei count,
                                                                   GLenum type,
                                                                   const void *indices,
                                                                   GLsizei instancecount,
                                                                   GLint basevertex)
{
    glDrawElementsInstancedBaseVertex(GL_POINTS, 1, type, indices, 1, basevertex);
}

void KHRONOS_APIENTRY DrawElementsInstancedBaseVertexEXTMinimizedProc(GLenum mode,
                                                                      GLsizei count,
                                                                      GLenum type,
                                                                      const void *indices,
                                                                      GLsizei instancecount,
                                                                      GLint basevertex)
{
    glDrawElementsInstancedBaseVertexEXT(GL_POINTS, 1, type, indices, 1, basevertex);
}

void KHRONOS_APIENTRY DrawElementsInstancedBaseVertexOESMinimizedProc(GLenum mode,
                                                                      GLsizei count,
                                                                      GLenum type,
                                                                      const void *indices,
                                                                      GLsizei instancecount,
                                                                      GLint basevertex)
{
    glDrawElementsInstancedBaseVertexOES(GL_POINTS, 1, type, indices, 1, basevertex);
}

void KHRONOS_APIENTRY DrawRangeElementsMinimizedProc(GLenum mode,
                                                     GLuint start,
                                                     GLuint end,
                                                     GLsizei count,
                                                     GLenum type,
                                                     const void *indices)
{
    glDrawRangeElements(GL_POINTS, start, end, 1, type, indices);
}

void KHRONOS_APIENTRY DrawArraysMinimizedProc(GLenum mode, GLint first, GLsizei count)
{
    glDrawArrays(GL_POINTS, first, 1);
}

void KHRONOS_APIENTRY DrawArraysInstancedMinimizedProc(GLenum mode,
                                                       GLint first,
                                                       GLsizei count,
                                                       GLsizei instancecount)
{
    glDrawArraysInstanced(GL_POINTS, first, 1, 1);
}

void KHRONOS_APIENTRY DrawArraysInstancedEXTMinimizedProc(GLenum mode,
                                                          GLint first,
                                                          GLsizei count,
                                                          GLsizei instancecount)
{
    glDrawArraysInstancedEXT(GL_POINTS, first, 1, 1);
}

void KHRONOS_APIENTRY DrawArraysIndirectMinimizedProc(GLenum mode, const void *indirect)
{
    glDrawArraysInstanced(GL_POINTS, 0, 1, 1);
}

void KHRONOS_APIENTRY DispatchComputeMinimizedProc(GLuint num_groups_x,
                                                   GLuint num_groups_y,
                                                   GLuint num_groups_z)
{
    glDispatchCompute(1, 1, 1);
}

void KHRONOS_APIENTRY DispatchComputeIndirectMinimizedProc(GLintptr indirect)
{
    glDispatchCompute(1, 1, 1);
}

// Interpose the calls that generate data copying work
void KHRONOS_APIENTRY BufferDataMinimizedProc(GLenum target,
                                              GLsizeiptr size,
                                              const void *data,
                                              GLenum usage)
{
    glBufferData(target, size, nullptr, usage);
}

void KHRONOS_APIENTRY BufferSubDataMinimizedProc(GLenum target,
                                                 GLintptr offset,
                                                 GLsizeiptr size,
                                                 const void *data)
{
#if !defined(NOOP_SUBDATA_SUBIMAGE_FOR_MINIMIZE_GPU_WORK)
    glBufferSubData(target, offset, 1, data);
#endif
}

void *KHRONOS_APIENTRY MapBufferRangeMinimizedProc(GLenum target,
                                                   GLintptr offset,
                                                   GLsizeiptr length,
                                                   GLbitfield access)
{
    access |= GL_MAP_UNSYNCHRONIZED_BIT;
    return glMapBufferRange(target, offset, length, access);
}

void *KHRONOS_APIENTRY MapBufferRangeEXTMinimizedProc(GLenum target,
                                                      GLintptr offset,
                                                      GLsizeiptr length,
                                                      GLbitfield access)
{
    access |= GL_MAP_UNSYNCHRONIZED_BIT;
    return glMapBufferRangeEXT(target, offset, length, access);
}

void KHRONOS_APIENTRY TexImage2DMinimizedProc(GLenum target,
                                              GLint level,
                                              GLint internalformat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLint border,
                                              GLenum format,
                                              GLenum type,
                                              const void *pixels)
{
    GLint unpackBuffer = 0;
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
    if (unpackBuffer)
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    glTexImage2D(target, level, internalformat, width, height, border, format, type, nullptr);
    if (unpackBuffer)
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
    }
}

void KHRONOS_APIENTRY TexSubImage2DMinimizedProc(GLenum target,
                                                 GLint level,
                                                 GLint xoffset,
                                                 GLint yoffset,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLenum format,
                                                 GLenum type,
                                                 const void *pixels)
{
#if !defined(NOOP_SUBDATA_SUBIMAGE_FOR_MINIMIZE_GPU_WORK)
    glTexSubImage2D(target, level, xoffset, yoffset, 1, 1, format, type, pixels);
#endif
}

void KHRONOS_APIENTRY TexImage3DMinimizedProc(GLenum target,
                                              GLint level,
                                              GLint internalformat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLsizei depth,
                                              GLint border,
                                              GLenum format,
                                              GLenum type,
                                              const void *pixels)
{
    GLint unpackBuffer = 0;
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
    if (unpackBuffer)
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    glTexImage3D(target, level, internalformat, width, height, depth, border, format, type,
                 nullptr);
    if (unpackBuffer)
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
    }
}

void KHRONOS_APIENTRY TexSubImage3DMinimizedProc(GLenum target,
                                                 GLint level,
                                                 GLint xoffset,
                                                 GLint yoffset,
                                                 GLint zoffset,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLsizei depth,
                                                 GLenum format,
                                                 GLenum type,
                                                 const void *pixels)
{
#if !defined(NOOP_SUBDATA_SUBIMAGE_FOR_MINIMIZE_GPU_WORK)
    glTexSubImage3D(target, level, xoffset, yoffset, zoffset, 1, 1, 1, format, type, pixels);
#endif
}

void KHRONOS_APIENTRY GenerateMipmapMinimizedProc(GLenum target)
{
    // Noop it for now. There is a risk that this will leave an incomplete mipmap chain and cause
    // other issues. If this turns out to be a real issue with app traces, we can turn this into a
    // glTexImage2D call for each generated level.
}

void KHRONOS_APIENTRY GenerateMipmapOESMinimizedProc(GLenum target) {}

void KHRONOS_APIENTRY BlitFramebufferMinimizedProc(GLint srcX0,
                                                   GLint srcY0,
                                                   GLint srcX1,
                                                   GLint srcY1,
                                                   GLint dstX0,
                                                   GLint dstY0,
                                                   GLint dstX1,
                                                   GLint dstY1,
                                                   GLbitfield mask,
                                                   GLenum filter)
{
    glBlitFramebuffer(srcX0, srcY0, srcX0 + 1, srcY0 + 1, dstX0, dstY0, dstX0 + 1, dstY0 + 1, mask,
                      filter);
}

void KHRONOS_APIENTRY ReadPixelsMinimizedProc(GLint x,
                                              GLint y,
                                              GLsizei width,
                                              GLsizei height,
                                              GLenum format,
                                              GLenum type,
                                              void *pixels)
{
    glReadPixels(x, y, 1, 1, format, type, pixels);
}

void KHRONOS_APIENTRY BeginTransformFeedbackMinimizedProc(GLenum primitiveMode)
{
    glBeginTransformFeedback(GL_POINTS);
}

angle::GenericProc KHRONOS_APIENTRY TraceLoadProc(const char *procName)
{
    // EGL
    if (strcmp(procName, "eglCreateContext") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglCreateContext);
    }
    if (strcmp(procName, "eglMakeCurrent") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglMakeCurrent);
    }
    if (strcmp(procName, "eglGetCurrentContext") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglGetCurrentContext);
    }
    if (strcmp(procName, "eglCreateImage") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglCreateImage);
    }
    if (strcmp(procName, "eglCreateImageKHR") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglCreateImageKHR);
    }
    if (strcmp(procName, "eglDestroyImage") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglDestroyImage);
    }
    if (strcmp(procName, "eglDestroyImageKHR") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglDestroyImageKHR);
    }
    if (strcmp(procName, "eglCreateSync") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglCreateSync);
    }
    if (strcmp(procName, "eglCreateSyncKHR") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglCreateSyncKHR);
    }
    if (strcmp(procName, "eglDestroySync") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglDestroySync);
    }
    if (strcmp(procName, "eglDestroySyncKHR") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglDestroySyncKHR);
    }
    if (strcmp(procName, "eglClientWaitSync") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglClientWaitSync);
    }
    if (strcmp(procName, "eglClientWaitSyncKHR") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglClientWaitSyncKHR);
    }
    if (strcmp(procName, "eglGetError") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglGetError);
    }
    if (strcmp(procName, "eglGetCurrentDisplay") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(EglGetCurrentDisplay);
    }

    // GLES
    if (strcmp(procName, "glBindFramebuffer") == 0 || strcmp(procName, "glBindFramebufferOES") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(BindFramebufferProc);
    }
    if (strcmp(procName, "glInvalidateFramebuffer") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(InvalidateFramebufferProc);
    }
    if (strcmp(procName, "glInvalidateSubFramebuffer") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(InvalidateSubFramebufferProc);
    }
    if (strcmp(procName, "glDrawBuffers") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(DrawBuffersProc);
    }
    if (strcmp(procName, "glReadBuffer") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(ReadBufferProc);
    }
    if (strcmp(procName, "glDiscardFramebufferEXT") == 0)
    {
        return reinterpret_cast<angle::GenericProc>(DiscardFramebufferEXTProc);
    }

    if (gMinimizeGPUWork)
    {
#define MINIMIZED(EntryPoint)                                                   \
    if (strcmp(procName, "gl" #EntryPoint) == 0)                                \
    {                                                                           \
        return reinterpret_cast<angle::GenericProc>(EntryPoint##MinimizedProc); \
    }

        MINIMIZED(Viewport)
        MINIMIZED(Scissor)

        // Interpose the calls that generate actual GPU work
        MINIMIZED(DrawElements)
        MINIMIZED(DrawElementsIndirect)
        MINIMIZED(DrawElementsInstanced)
        MINIMIZED(DrawElementsInstancedEXT)
        MINIMIZED(DrawElementsBaseVertex)
        MINIMIZED(DrawElementsBaseVertexEXT)
        MINIMIZED(DrawElementsBaseVertexOES)
        MINIMIZED(DrawElementsInstancedBaseVertex)
        MINIMIZED(DrawElementsInstancedBaseVertexEXT)
        MINIMIZED(DrawElementsInstancedBaseVertexOES)
        MINIMIZED(DrawRangeElements)
        MINIMIZED(DrawArrays)
        MINIMIZED(DrawArraysInstanced)
        MINIMIZED(DrawArraysInstancedEXT)
        MINIMIZED(DrawArraysIndirect)
        MINIMIZED(DispatchCompute)
        MINIMIZED(DispatchComputeIndirect)

        // Interpose the calls that generate data copying work
        MINIMIZED(BufferData)
        MINIMIZED(BufferSubData)
        MINIMIZED(MapBufferRange)
        MINIMIZED(MapBufferRangeEXT)
        MINIMIZED(TexImage2D)
        MINIMIZED(TexImage3D)
        MINIMIZED(TexSubImage2D)
        MINIMIZED(TexSubImage3D)
        MINIMIZED(GenerateMipmap)
        MINIMIZED(GenerateMipmapOES)
        MINIMIZED(BlitFramebuffer)
        MINIMIZED(ReadPixels)
        MINIMIZED(BeginTransformFeedback)
    }

    return gCurrentTracePerfTest->getGLWindow()->getProcAddress(procName);
}

void ValidateSerializedState(const char *serializedState, const char *fileName, uint32_t line)
{
    gCurrentTracePerfTest->validateSerializedState(serializedState, fileName, line);
}

bool FindTraceTestDataPath(const char *traceName, char *testDataDirOut, size_t maxDataDirLen)
{
    char relativeTestDataDir[kMaxPath] = {};
    snprintf(relativeTestDataDir, kMaxPath, "%s%c%s", kTraceTestFolder, GetPathSeparator(),
             traceName);
    return angle::FindTestDataPath(relativeTestDataDir, testDataDirOut, maxDataDirLen);
}

bool FindRootTraceTestDataPath(char *testDataDirOut, size_t maxDataDirLen)
{
    return angle::FindTestDataPath(kTraceTestFolder, testDataDirOut, maxDataDirLen);
}

GPUTestConfig::API getTestConfigAPIFromRenderer(angle::GLESDriverType driverType,
                                                EGLenum renderer,
                                                EGLenum deviceType)
{
    if (driverType == angle::GLESDriverType::SystemEGL ||
        driverType == angle::GLESDriverType::SystemWGL)
    {
        return GPUTestConfig::kAPINative;
    }

    if (driverType != angle::GLESDriverType::AngleEGL &&
        driverType != angle::GLESDriverType::AngleVulkanSecondariesEGL)
    {
        return GPUTestConfig::kAPIUnknown;
    }

    switch (renderer)
    {
        case EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE:
            return GPUTestConfig::kAPID3D11;
        case EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE:
            return GPUTestConfig::kAPID3D9;
        case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
            return GPUTestConfig::kAPIGLDesktop;
        case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
            return GPUTestConfig::kAPIGLES;
        case EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE:
            if (deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE)
            {
                return GPUTestConfig::kAPISwiftShader;
            }
            else
            {
                return GPUTestConfig::kAPIVulkan;
            }
        case EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE:
            return GPUTestConfig::kAPIMetal;
        case EGL_PLATFORM_ANGLE_TYPE_WEBGPU_ANGLE:
            return GPUTestConfig::kAPIWgpu;
        default:
            std::cerr << "Unknown Renderer enum: 0x" << std::hex << renderer << "\n";
            return GPUTestConfig::kAPIUnknown;
    }
}

bool TracePerfTest::loadTestExpectationsFromFileWithConfig(const GPUTestConfig &config,
                                                           const std::string &fileName)
{
    if (!mTestExpectationsParser.loadTestExpectationsFromFile(config, fileName))
    {
        std::stringstream errorMsgStream;
        for (const auto &message : mTestExpectationsParser.getErrorMessages())
        {
            errorMsgStream << std::endl << " " << message;
        }

        std::cerr << "Failed to load test expectations." << errorMsgStream.str() << std::endl;
        return false;
    }
    return true;
}

void TracePerfTest::initializeConfigParams(GPUTestConfig::API api)
{
    // TODO (b/423678565): These config parameters will be overridden by ANGLERenderTest::SetUp().
    ConfigParameters &configParams = getConfigParams();
    configParams.redBits           = mParams->traceInfo.configRedBits;
    configParams.greenBits         = mParams->traceInfo.configGreenBits;
    configParams.blueBits          = mParams->traceInfo.configBlueBits;
    configParams.alphaBits         = mParams->traceInfo.configAlphaBits;
    configParams.depthBits         = mParams->traceInfo.configDepthBits;
    configParams.stencilBits       = mParams->traceInfo.configStencilBits;
    configParams.colorSpace        = mParams->traceInfo.drawSurfaceColorSpace;

    // TODO (b/423680521): App traces shouldn't be relying on these extensions anyway, since they
    // are not available when the real app is running on a real device, so these values should
    // always match the defaults to begin with.
    configParams.webGLCompatibility    = mParams->traceInfo.isWebGLCompatibilityEnabled;
    configParams.robustResourceInit    = mParams->traceInfo.isRobustResourceInitEnabled;
    configParams.bindGeneratesResource = mParams->traceInfo.isBindGeneratesResourcesEnabled;
    configParams.clientArraysEnabled   = mParams->traceInfo.areClientArraysEnabled;
}

TracePerfTest::TracePerfTest(std::unique_ptr<const TracePerfParams> params)
    : ANGLERenderTest("TracePerf", *params.get(), "ms"),
      mParams(std::move(params)),
      mStartFrame(0),
      mEndFrame(0)
{
    constexpr char kTestExpectationsPath[] =
        "src/tests/perf_tests/angle_trace_tests_expectations.txt";
    constexpr size_t kMaxPath = 512;
    std::array<char, kMaxPath> foundDataPath;
    if (!angle::FindTestDataPath(kTestExpectationsPath, foundDataPath.data(), foundDataPath.size()))
    {
        failTest(std::string("Unable to find ANGLE trace tests expectations path: ") +
                 std::string(kTestExpectationsPath));
        return;
    }

    angle::GPUTestConfig::API api = getTestConfigAPIFromRenderer(
        mParams->driver, mParams->eglParameters.renderer, mParams->eglParameters.deviceType);

    GPUTestConfig testConfig = GPUTestConfig(api, 0);
    if (!loadTestExpectationsFromFileWithConfig(testConfig, std::string(foundDataPath.data())))
    {
        failTest(std::string("Unable to load ANGLE trace tests expectations file: ") +
                 std::string(foundDataPath.data()));
        return;
    }
    else
    {
        int32_t testExpectation =
            mTestExpectationsParser.getTestExpectation(mParams->traceInfo.name);

        if (testExpectation == GPUTestExpectationsParser::kGpuTestSkip)
        {
            skipTest("Test skipped on this config");
        }
    }

    if (!mParams->traceInfo.initialized)
    {
        failTest("Failed to load trace json.");
        return;
    }

    initializeConfigParams(api);

    for (std::string extension : mParams->traceInfo.requiredExtensions)
    {
        addExtensionPrerequisite(extension);
    }

    if (!mParams->traceInfo.keyFrames.empty())
    {
        // Only support one keyFrame for now
        if (mParams->traceInfo.keyFrames.size() != 1)
        {
            WARN() << "Multiple keyframes detected, only using the first";
        }

        // Only use keyFrame if the user didn't specify a value.
        if (gScreenshotFrame == kDefaultScreenshotFrame)
        {
            mScreenshotFrame = mParams->traceInfo.keyFrames[0];
            INFO() << "Trace contains keyframe, using frame " << mScreenshotFrame
                   << " for screenshot";
        }
        else
        {
            WARN() << "Ignoring keyframe, user requested frame " << mScreenshotFrame
                   << " for screenshot";
            if (mScreenshotFrame == kAllFrames)
            {
                WARN() << "Capturing screenshots of all frames since requested frame was "
                       << kAllFrames;
            }
        }
    }

    // Configuration-specific test exceptions. Only include exceptions that are outside the scope
    // of the trace tests expectations file, "angle_trace_tests_expectations.txt".

    if (traceNameIs("modern_combat_5"))
    {
        if (IsPixel6() && !IsAndroid14OrNewer())
        {
            skipTest(
                "https://issuetracker.google.com/42267261 Causing thermal failures on Pixel 6 with "
                "Android 13");
        }
    }

    if (traceNameIs("genshin_impact"))
    {
        if (!Is64Bit())
        {
            skipTest("Genshin is too large to handle in 32-bit mode");
        }
    }

    // Legacy trace-specific extension dependencies. For new traces this information will be
    // included in the trace's json file.

    if (traceNameIs("brawl_stars"))
    {
        addExtensionPrerequisite("GL_EXT_shadow_samplers");
    }

    if (traceNameIs("free_fire"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("marvel_contest_of_champions"))
    {
        addExtensionPrerequisite("GL_EXT_color_buffer_half_float");
    }

    if (traceNameIs("world_of_tanks_blitz"))
    {
        addExtensionPrerequisite("GL_EXT_disjoint_timer_query");
    }

    if (traceNameIs("dragon_ball_legends"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("lego_legacy"))
    {
        addExtensionPrerequisite("GL_EXT_shadow_samplers");
    }

    if (traceNameIs("world_war_doh"))
    {
        // Linux+NVIDIA doesn't support GL_KHR_texture_compression_astc_ldr (possibly others also)
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("saint_seiya_awakening"))
    {
        addExtensionPrerequisite("GL_EXT_shadow_samplers");
    }

    if (traceNameIs("magic_tiles_3"))
    {
        // Linux+NVIDIA doesn't support GL_KHR_texture_compression_astc_ldr (possibly others also)
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("real_gangster_crime"))
    {
        // Linux+NVIDIA doesn't support GL_KHR_texture_compression_astc_ldr (possibly others also)
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");

        // Intel doesn't support external images.
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("asphalt_8"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("hearthstone"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("shadow_fight_2"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("rise_of_kingdoms"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("among_us"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("extreme_car_driving_simulator"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("junes_journey"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("ragnarok_m_eternal_love"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("league_of_legends_wild_rift"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("aztec_ruins"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("dragon_raja"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("avakin_life"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("ludo_king"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("pokemon_go"))
    {
        addExtensionPrerequisite("GL_EXT_texture_cube_map_array");
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("cookie_run_kingdom"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("pubg_mobile_skydive") || traceNameIs("pubg_mobile_battle_royale"))
    {
        addExtensionPrerequisite("GL_EXT_texture_buffer");
    }

    if (traceNameIs("scrabble_go"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("world_of_kings"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("nier_reincarnation"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("world_cricket_championship_2"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("township"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("asphalt_9"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("dead_by_daylight"))
    {
        addExtensionPrerequisite("GL_EXT_shader_framebuffer_fetch");
    }

    if (traceNameIs("war_planet_online"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("lords_mobile"))
    {
        // http://anglebug.com/42265475 - glTexStorage2DEXT is not exposed on Pixel 4 native
        addExtensionPrerequisite("GL_EXT_texture_storage");
    }

    if (traceNameIs("real_racing3"))
    {
        addExtensionPrerequisite("GL_EXT_shader_framebuffer_fetch");
    }

    if (traceNameIs("blade_and_soul_revolution"))
    {
        addExtensionPrerequisite("GL_EXT_texture_buffer");
        addExtensionPrerequisite("GL_EXT_shader_framebuffer_fetch");
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("scary_teacher_3d"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("car_chase"))
    {
        addExtensionPrerequisite("GL_EXT_geometry_shader");
        addExtensionPrerequisite("GL_EXT_primitive_bounding_box");
        addExtensionPrerequisite("GL_EXT_tessellation_shader");
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
        addExtensionPrerequisite("GL_EXT_texture_cube_map_array");
    }

    if (traceNameIs("aztec_ruins_high"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("special_forces_group_2"))
    {
        addExtensionPrerequisite("GL_EXT_texture_buffer");
    }

    if (traceNameIs("tessellation"))
    {
        addExtensionPrerequisite("GL_EXT_geometry_shader");
        addExtensionPrerequisite("GL_EXT_primitive_bounding_box");
        addExtensionPrerequisite("GL_EXT_tessellation_shader");
        addExtensionPrerequisite("GL_EXT_texture_cube_map_array");
    }

    if (traceNameIs("basemark_gpu"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("mortal_kombat"))
    {
        addExtensionPrerequisite("GL_EXT_texture_buffer");
    }

    if (traceNameIs("ni_no_kuni"))
    {
        addExtensionPrerequisite("GL_EXT_shader_framebuffer_fetch");
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("octopath_traveler"))
    {
        addExtensionPrerequisite("GL_EXT_shader_framebuffer_fetch");
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("antutu_refinery"))
    {
        addExtensionPrerequisite("GL_ANDROID_extension_pack_es31a");
    }

    if (traceNameIs("botworld_adventure"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("life_is_strange"))
    {
        addExtensionPrerequisite("GL_EXT_texture_buffer");
        addExtensionPrerequisite("GL_EXT_texture_cube_map_array");
    }

    if (traceNameIs("minetest"))
    {
        addExtensionPrerequisite("GL_EXT_texture_format_BGRA8888");
        addIntegerPrerequisite(GL_MAX_TEXTURE_UNITS, 4);
    }

    if (traceNameIs("diablo_immortal"))
    {
        addExtensionPrerequisite("GL_EXT_shader_framebuffer_fetch");
    }

    if (traceNameIs("mu_origin_3"))
    {
        addExtensionPrerequisite("GL_EXT_texture_buffer");
        addExtensionPrerequisite("GL_EXT_shader_framebuffer_fetch");
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("catalyst_black"))
    {
        addExtensionPrerequisite("GL_EXT_shader_framebuffer_fetch");
    }

    if (traceNameIs("limbo"))
    {
        addExtensionPrerequisite("GL_EXT_shader_framebuffer_fetch");

        // For LUMINANCE8_ALPHA8_EXT
        addExtensionPrerequisite("GL_EXT_texture_storage");
    }

    if (traceNameIs("arknights"))
    {
        // Intel doesn't support external images.
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("honkai_star_rail"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("toca_life_world"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("poppy_playtime"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
        addIntegerPrerequisite(GL_MAX_TEXTURE_SIZE, 16383);
    }

    if (traceNameIs("grand_mountain_adventure"))
    {
        addIntegerPrerequisite(GL_MAX_TEXTURE_SIZE, 11016);
    }

    if (traceNameIs("passmark_simple"))
    {
        addExtensionPrerequisite("GL_OES_framebuffer_object");
    }

    if (traceNameIs("minecraft_vibrant_visuals"))
    {
        addIntegerPrerequisite(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, 1024);
    }

    // GL_KHR_debug does not work on Android for GLES1
    if (IsAndroid() && mParams->traceInfo.contextClientMajorVersion == 1)
    {
        mEnableDebugCallback = false;
    }

    // We already swap in TracePerfTest::drawBenchmark, no need to swap again in the harness.
    disableTestHarnessSwap();

    gCurrentTracePerfTest = this;

    if (gTraceTestValidation)
    {
        mStepsToRun = frameCount();
    }

    if (gRunToKeyFrame)
    {
        if (mParams->traceInfo.keyFrames.empty())
        {
            // If we don't have a keyFrame, run one step
            INFO() << "No keyframe available for trace, running to frame 1";
            mStepsToRun = 1;
        }
        else
        {
            int keyFrame = mParams->traceInfo.keyFrames[0];
            INFO() << "Running to keyframe: " << keyFrame;
            mStepsToRun = keyFrame;
        }
    }
}

void TracePerfTest::startTest()
{
    // runTrial() must align to frameCount()
    ASSERT(mCurrentFrame == mStartFrame);

    ANGLERenderTest::startTest();
}

std::string FindTraceGzPath(const std::string &traceName)
{
    std::stringstream pathStream;

    char genDir[kMaxPath] = {};
    if (!angle::FindTestDataPath("gen", genDir, kMaxPath))
    {
        return "";
    }
    pathStream << genDir << angle::GetPathSeparator() << "tracegz_" << traceName << ".gz";

    return pathStream.str();
}

void TracePerfTest::initializeBenchmark()
{
    const TraceInfo &traceInfo = mParams->traceInfo;

    char testDataDir[kMaxPath] = {};
    if (!FindTraceTestDataPath(traceInfo.name, testDataDir, kMaxPath))
    {
        failTest("Could not find test data folder.");
        return;
    }

    std::string baseDir = "";
#if defined(ANGLE_TRACE_EXTERNAL_BINARIES)
    baseDir += AndroidWindow::GetApplicationDirectory() + "/angle_traces/";
#endif

    if (gTraceInterpreter)
    {
        mTraceReplay.reset(new TraceLibrary("angle_trace_interpreter", traceInfo, baseDir));
        if (strcmp(gTraceInterpreter, "gz") == 0)
        {
            std::string traceGzPath = FindTraceGzPath(traceInfo.name);
            if (traceGzPath.empty())
            {
                failTest("Could not find trace gz.");
                return;
            }
            mTraceReplay->setTraceGzPath(traceGzPath);
        }
    }
    else
    {
        std::stringstream traceNameStr;
        traceNameStr << "angle_restricted_traces_" << traceInfo.name;
        std::string traceName = traceNameStr.str();
        mTraceReplay.reset(new TraceLibrary(traceNameStr.str(), traceInfo, baseDir));
    }

    LoadTraceEGL(TraceLoadProc);
    LoadTraceGLES(TraceLoadProc);

    if (!mTraceReplay->valid())
    {
        failTest("Could not load trace.");
        return;
    }

    mStartFrame = traceInfo.frameStart;
    mEndFrame   = traceInfo.frameEnd;
    mTraceReplay->setValidateSerializedStateCallback(ValidateSerializedState);
    mTraceReplay->setBinaryDataDir(testDataDir);
    mTraceReplay->setReplayResourceMode(gIncludeInactiveResources);
    if (gScreenshotDir)
    {
        mTraceReplay->setDebugOutputDir(gScreenshotDir);
    }

    if (gMinimizeGPUWork)
    {
        // Shrink the offscreen window to 1x1.
        mWindowWidth  = 1;
        mWindowHeight = 1;
    }
    else
    {
        mWindowWidth  = mTestParams.windowWidth;
        mWindowHeight = mTestParams.windowHeight;
    }
    mCurrentFrame     = mStartFrame;
    mCurrentIteration = mStartFrame;
    mCurrentOffscreenGridIteration = 0;

    if (IsAndroid())
    {
        // On Android, set the orientation used by the app, based on width/height
        getWindow()->setOrientation(mTestParams.windowWidth, mTestParams.windowHeight);
    }

    // If we're rendering offscreen we set up a default back buffer.
    if (mParams->surfaceType == SurfaceType::Offscreen)
    {
        bool gles1 = mParams->traceInfo.contextClientMajorVersion == 1;
        if (gles1 &&
            !CheckExtensionExists(reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)),
                                  "GL_OES_framebuffer_object"))
        {
            failTest("GLES1 --offscreen requires GL_OES_framebuffer_object");
            return;
        }

        auto genRenderbuffers     = gles1 ? glGenRenderbuffersOES : glGenRenderbuffers;
        auto bindRenderbuffer     = gles1 ? glBindRenderbufferOES : glBindRenderbuffer;
        auto renderbufferStorage  = gles1 ? glRenderbufferStorageOES : glRenderbufferStorage;
        auto genFramebuffers      = gles1 ? glGenFramebuffersOES : glGenFramebuffers;
        auto bindFramebuffer      = gles1 ? glBindFramebufferOES : glBindFramebuffer;
        auto framebufferTexture2D = gles1 ? glFramebufferTexture2DOES : glFramebufferTexture2D;
        auto framebufferRenderbuffer =
            gles1 ? glFramebufferRenderbufferOES : glFramebufferRenderbuffer;

        genRenderbuffers(1, &mOffscreenDepthStencil);
        bindRenderbuffer(GL_RENDERBUFFER, mOffscreenDepthStencil);
        renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mWindowWidth, mWindowHeight);
        bindRenderbuffer(GL_RENDERBUFFER, 0);

        mEglContext = eglGetCurrentContext();

        genFramebuffers(mMaxOffscreenBufferCount, mOffscreenFramebuffers.data());
        glGenTextures(mMaxOffscreenBufferCount, mOffscreenTextures.data());
        for (int i = 0; i < mMaxOffscreenBufferCount; i++)
        {
            bindFramebuffer(GL_FRAMEBUFFER, mOffscreenFramebuffers[i]);

            // Hard-code RGBA8/D24S8. This should be specified in the trace info.
            glBindTexture(GL_TEXTURE_2D, mOffscreenTextures[i]);
            glTexImage2D(GL_TEXTURE_2D, 0,
                         mParams->colorSpace == EGL_GL_COLORSPACE_SRGB ? GL_SRGB8_ALPHA8 : GL_RGBA,
                         mWindowWidth, mWindowHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                 mOffscreenTextures[i], 0);
            framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                    mOffscreenDepthStencil);
            framebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                    mOffscreenDepthStencil);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    // Potentially slow. Can load a lot of resources.
    mTraceReplay->setupReplay();

    glFinish();

    ASSERT_GE(mEndFrame, mStartFrame);

    getWindow()->ignoreSizeEvents();
    getWindow()->setVisible(true);

    // If we're re-tracing, trigger capture start after setup. This ensures the Setup function gets
    // recaptured into another Setup function and not merged with the first frame.
    if (gRetraceMode)
    {
        getGLWindow()->swap();
    }
}

#undef TRACE_TEST_CASE

void TracePerfTest::destroyBenchmark()
{
    if (mParams->surfaceType == SurfaceType::Offscreen)
    {
        glDeleteTextures(mMaxOffscreenBufferCount, mOffscreenTextures.data());
        mOffscreenTextures.fill(0);

        bool gles1               = mParams->traceInfo.contextClientMajorVersion == 1;
        auto deleteRenderbuffers = gles1 ? glDeleteRenderbuffersOES : glDeleteRenderbuffers;
        auto deleteFramebuffers  = gles1 ? glDeleteFramebuffersOES : glDeleteFramebuffers;

        deleteRenderbuffers(1, &mOffscreenDepthStencil);
        mOffscreenDepthStencil = 0;

        deleteFramebuffers(mMaxOffscreenBufferCount, mOffscreenFramebuffers.data());
        mOffscreenFramebuffers.fill(0);
    }

    mTraceReplay->finishReplay();
    mTraceReplay.reset(nullptr);
}

void TracePerfTest::sampleTime()
{
    if (mUseTimestampQueries)
    {
        GLint64 glTime;
        // glGetInteger64vEXT is exported by newer versions of the timer query extensions.
        // Unfortunately only the core EP is exposed by some desktop drivers (e.g. NVIDIA).
        if (glGetInteger64vEXT)
        {
            glGetInteger64vEXT(GL_TIMESTAMP_EXT, &glTime);
        }
        else
        {
            glGetInteger64v(GL_TIMESTAMP_EXT, &glTime);
        }
        mTimeline.push_back({glTime, angle::GetHostTimeSeconds()});
    }
}

void TracePerfTest::drawBenchmark()
{
    constexpr uint32_t kFramesPerX  = 6;
    constexpr uint32_t kFramesPerY  = 4;
    constexpr uint32_t kFramesPerSwap = kFramesPerY * kFramesPerX;

    const uint32_t kOffscreenOffsetX = 0;
    const uint32_t kOffscreenOffsetY = 0;
    const uint32_t kOffscreenWidth   = mTestParams.windowWidth;
    const uint32_t kOffscreenHeight  = mTestParams.windowHeight;

    const uint32_t kOffscreenFrameWidth = static_cast<uint32_t>(
        static_cast<double>(kOffscreenWidth / static_cast<double>(kFramesPerX)));
    const uint32_t kOffscreenFrameHeight = static_cast<uint32_t>(
        static_cast<double>(kOffscreenHeight / static_cast<double>(kFramesPerY)));

    // Add a time sample from GL and the host.
    if (mCurrentFrame == mStartFrame)
    {
        sampleTime();
    }

    bool gles1           = mParams->traceInfo.contextClientMajorVersion == 1;
    auto bindFramebuffer = gles1 ? glBindFramebufferOES : glBindFramebuffer;
    int offscreenBufferIndex = mTotalFrameCount % mMaxOffscreenBufferCount;

    if (mParams->surfaceType == SurfaceType::Offscreen)
    {
        // Some driver (ARM and ANGLE) try to nop or defer the glFlush if it is called within the
        // renderpass to avoid breaking renderpass (performance reason). For app traces that does
        // not use any FBO, when we run in the offscreen mode, there is no frame boundary and
        // glFlush call we issued at end of frame will get skipped. To overcome this (and also
        // matches what onscreen double buffering behavior as well), we use two offscreen FBOs and
        // ping pong between them for each frame.
        GLuint buffer = mOffscreenFramebuffers[offscreenBufferIndex];

        if (gles1 && mOffscreenFrameCount == kFramesPerSwap - 1)
        {
            buffer = 0;  // gles1: a single frame is rendered to buffer 0
        }
        bindFramebuffer(GL_FRAMEBUFFER, buffer);

        GLsync sync = mOffscreenSyncs[offscreenBufferIndex];
        if (sync)
        {
            constexpr GLuint64 kTimeout = 2'000'000'000;  // 2 seconds
            GLenum result = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, kTimeout);
            if (result != GL_CONDITION_SATISFIED && result != GL_ALREADY_SIGNALED)
            {
                failTest(std::string("glClientWaitSync unexpected result: ") +
                         std::to_string(result));
            }
            glDeleteSync(sync);
        }
    }

    char frameName[32];
    snprintf(frameName, sizeof(frameName), "Frame %u", mCurrentFrame);
    beginInternalTraceEvent(frameName);

    startGpuTimer();
    atraceCounter("TraceFrameIndex", mCurrentFrame);
    mTraceReplay->replayFrame(mCurrentFrame);
    stopGpuTimer();

    updatePerfCounters();

    if (mParams->surfaceType == SurfaceType::Offscreen)
    {
        if (gMinimizeGPUWork)
        {
            // To keep GPU work minimum, we skip the blit.
            glFlush();
            mOffscreenFrameCount++;
        }
        else
        {
            GLuint offscreenBuffer = mOffscreenFramebuffers[offscreenBufferIndex];

            EGLContext currentEglContext = eglGetCurrentContext();
            if (currentEglContext != mEglContext)
            {
                eglMakeCurrent(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW),
                               eglGetCurrentSurface(EGL_READ), mEglContext);
            }

            GLint currentDrawFBO, currentReadFBO;
            if (gles1)
            {
                // OES_framebuffer_object doesn't define a separate "read" binding
                glGetIntegerv(GL_FRAMEBUFFER_BINDING_OES, &currentDrawFBO);
                bindFramebuffer(GL_FRAMEBUFFER, offscreenBuffer);
            }
            else
            {
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &currentDrawFBO);
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &currentReadFBO);

                bindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                bindFramebuffer(GL_READ_FRAMEBUFFER, offscreenBuffer);
            }

            uint32_t frameX  = (mOffscreenFrameCount % kFramesPerSwap) % kFramesPerX;
            uint32_t frameY  = (mOffscreenFrameCount % kFramesPerSwap) / kFramesPerX;
            uint32_t windowX = kOffscreenOffsetX + frameX * kOffscreenFrameWidth;
            uint32_t windowY = kOffscreenOffsetY + frameY * kOffscreenFrameHeight;

            GLboolean scissorTest = GL_FALSE;
            glGetBooleanv(GL_SCISSOR_TEST, &scissorTest);

            if (scissorTest)
            {
                glDisable(GL_SCISSOR_TEST);
            }

            if (!gles1)  // gles1: no glBlitFramebuffer, a single frame is rendered to buffer 0
            {
                mOffscreenSyncs[offscreenBufferIndex] =
                    glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

                glBlitFramebuffer(0, 0, mWindowWidth, mWindowHeight, windowX, windowY,
                                  windowX + kOffscreenFrameWidth, windowY + kOffscreenFrameHeight,
                                  GL_COLOR_BUFFER_BIT, GL_NEAREST);
            }

            // GL_READ_FRAMEBUFFER is already set correctly for glReadPixels
            saveScreenshotIfEnabled(ScreenshotType::kFrame);

            if (frameX == kFramesPerX - 1 && frameY == kFramesPerY - 1)
            {
                bindFramebuffer(GL_FRAMEBUFFER, 0);
                if (!gles1)  // gles1: no grid, a single frame is rendered to buffer 0
                {
                    mCurrentOffscreenGridIteration++;
                    saveScreenshotIfEnabled(ScreenshotType::kGrid);
                }
                getGLWindow()->swap();
                glClear(GL_COLOR_BUFFER_BIT);
                mOffscreenFrameCount = 0;
            }
            else
            {
                glFlush();
                mOffscreenFrameCount++;
            }

            if (scissorTest)
            {
                glEnable(GL_SCISSOR_TEST);
            }

            if (gles1)
            {
                bindFramebuffer(GL_FRAMEBUFFER, currentDrawFBO);
            }
            else
            {
                bindFramebuffer(GL_DRAW_FRAMEBUFFER, currentDrawFBO);
                bindFramebuffer(GL_READ_FRAMEBUFFER, currentReadFBO);
            }

            if (currentEglContext != mEglContext)
            {
                eglMakeCurrent(eglGetCurrentDisplay(), eglGetCurrentSurface(EGL_DRAW),
                               eglGetCurrentSurface(EGL_READ), currentEglContext);
            }
        }
    }
    else
    {
        bindFramebuffer(GL_FRAMEBUFFER, 0);
        saveScreenshotIfEnabled(ScreenshotType::kFrame);
        getGLWindow()->swap();
    }

    endInternalTraceEvent(frameName);

    mTotalFrameCount++;

    if (mCurrentFrame == mEndFrame)
    {
        mTraceReplay->resetReplay();
        mCurrentFrame = mStartFrame;
    }
    else
    {
        mCurrentFrame++;
    }

    // Always iterated for saving screenshots after reset
    mCurrentIteration++;

    // Process any running queries once per iteration.
    for (size_t queryIndex = 0; queryIndex < mRunningQueries.size();)
    {
        const QueryInfo &query = mRunningQueries[queryIndex];

        GLuint endResultAvailable = 0;
        glGetQueryObjectuivEXT(query.endTimestampQuery, GL_QUERY_RESULT_AVAILABLE,
                               &endResultAvailable);

        if (endResultAvailable == GL_TRUE)
        {
            char fboName[32];
            snprintf(fboName, sizeof(fboName), "FBO %u", query.framebuffer);

            GLint64 beginTimestamp = 0;
            glGetQueryObjecti64vEXT(query.beginTimestampQuery, GL_QUERY_RESULT, &beginTimestamp);
            glDeleteQueriesEXT(1, &query.beginTimestampQuery);
            double beginHostTime = getHostTimeFromGLTime(beginTimestamp);
            beginGLTraceEvent(fboName, beginHostTime);

            GLint64 endTimestamp = 0;
            glGetQueryObjecti64vEXT(query.endTimestampQuery, GL_QUERY_RESULT, &endTimestamp);
            glDeleteQueriesEXT(1, &query.endTimestampQuery);
            double endHostTime = getHostTimeFromGLTime(endTimestamp);
            endGLTraceEvent(fboName, endHostTime);

            mRunningQueries.erase(mRunningQueries.begin() + queryIndex);
        }
        else
        {
            queryIndex++;
        }
    }
}

// Converts a GL timestamp into a host-side CPU time aligned with "GetHostTimeSeconds".
// This check is necessary to line up sampled trace events in a consistent timeline.
// Uses a linear interpolation from a series of samples. We do a blocking call to sample
// both host and GL time once per swap. We then find the two closest GL timestamps and
// interpolate the host times between them to compute our result. If we are past the last
// GL timestamp we sample a new data point pair.
double TracePerfTest::getHostTimeFromGLTime(GLint64 glTime)
{
    // Find two samples to do a lerp.
    size_t firstSampleIndex = mTimeline.size() - 1;
    while (firstSampleIndex > 0)
    {
        if (mTimeline[firstSampleIndex].glTime < glTime)
        {
            break;
        }
        firstSampleIndex--;
    }

    // Add an extra sample if we're missing an ending sample.
    if (firstSampleIndex == mTimeline.size() - 1)
    {
        sampleTime();
    }

    const TimeSample &start = mTimeline[firstSampleIndex];
    const TimeSample &end   = mTimeline[firstSampleIndex + 1];

    // Note: we have observed in some odd cases later timestamps producing values that are
    // smaller than preceding timestamps. This bears further investigation.

    // Compute the scaling factor for the lerp.
    double glDelta = static_cast<double>(glTime - start.glTime);
    double glRange = static_cast<double>(end.glTime - start.glTime);
    double t       = glDelta / glRange;

    // Lerp(t1, t2, t)
    double hostRange = end.hostTime - start.hostTime;
    return mTimeline[firstSampleIndex].hostTime + hostRange * t;
}

EGLContext TracePerfTest::onEglCreateContext(EGLDisplay display,
                                             EGLConfig config,
                                             EGLContext share_context,
                                             EGLint const *attrib_list)
{
    GLWindowContext newContext =
        getGLWindow()->createContextGeneric(reinterpret_cast<GLWindowContext>(share_context));
    return reinterpret_cast<EGLContext>(newContext);
}

void TracePerfTest::onEglMakeCurrent(EGLDisplay display,
                                     EGLSurface draw,
                                     EGLSurface read,
                                     EGLContext context)
{
    getGLWindow()->makeCurrentGeneric(reinterpret_cast<GLWindowContext>(context));
}

EGLContext TracePerfTest::onEglGetCurrentContext()
{
    return getGLWindow()->getCurrentContextGeneric();
}

EGLImage TracePerfTest::onEglCreateImage(EGLDisplay display,
                                         EGLContext context,
                                         EGLenum target,
                                         EGLClientBuffer buffer,
                                         const EGLAttrib *attrib_list)
{
    GLWindowBase::Image image = getGLWindow()->createImage(
        reinterpret_cast<GLWindowContext>(context), target, buffer, attrib_list);
    return reinterpret_cast<EGLImage>(image);
}

EGLImageKHR TracePerfTest::onEglCreateImageKHR(EGLDisplay display,
                                               EGLContext context,
                                               EGLenum target,
                                               EGLClientBuffer buffer,
                                               const EGLint *attrib_list)
{
    GLWindowBase::Image image = getGLWindow()->createImageKHR(
        reinterpret_cast<GLWindowContext>(context), target, buffer, attrib_list);
    return reinterpret_cast<EGLImage>(image);
}

EGLBoolean TracePerfTest::onEglDestroyImage(EGLDisplay display, EGLImage image)
{
    return getGLWindow()->destroyImage(image);
}

EGLBoolean TracePerfTest::onEglDestroyImageKHR(EGLDisplay display, EGLImage image)
{
    return getGLWindow()->destroyImageKHR(image);
}

EGLSync TracePerfTest::onEglCreateSync(EGLDisplay dpy, EGLenum type, const EGLAttrib *attrib_list)
{
    return getGLWindow()->createSync(dpy, type, attrib_list);
}

EGLSync TracePerfTest::onEglCreateSyncKHR(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list)
{
    return getGLWindow()->createSyncKHR(dpy, type, attrib_list);
}

EGLBoolean TracePerfTest::onEglDestroySync(EGLDisplay dpy, EGLSync sync)
{
    return getGLWindow()->destroySync(dpy, sync);
}

EGLBoolean TracePerfTest::onEglDestroySyncKHR(EGLDisplay dpy, EGLSync sync)
{
    return getGLWindow()->destroySyncKHR(dpy, sync);
}

EGLint TracePerfTest::onEglClientWaitSync(EGLDisplay dpy,
                                          EGLSync sync,
                                          EGLint flags,
                                          EGLTimeKHR timeout)
{
    return getGLWindow()->clientWaitSync(dpy, sync, flags, timeout);
}

EGLint TracePerfTest::onEglClientWaitSyncKHR(EGLDisplay dpy,
                                             EGLSync sync,
                                             EGLint flags,
                                             EGLTimeKHR timeout)
{
    return getGLWindow()->clientWaitSyncKHR(dpy, sync, flags, timeout);
}

EGLint TracePerfTest::onEglGetError()
{
    return getGLWindow()->getEGLError();
}

EGLDisplay TracePerfTest::onEglGetCurrentDisplay()
{
    return getGLWindow()->getCurrentDisplay();
}

// Triggered when the replay calls glBindFramebuffer.
void TracePerfTest::onReplayFramebufferChange(GLenum target, GLuint framebuffer)
{
    bool gles1           = mParams->traceInfo.contextClientMajorVersion == 1;
    auto bindFramebuffer = gles1 ? glBindFramebufferOES : glBindFramebuffer;

    if (framebuffer == 0 && mParams->surfaceType == SurfaceType::Offscreen)
    {
        bindFramebuffer(target,
                        mOffscreenFramebuffers[mTotalFrameCount % mMaxOffscreenBufferCount]);
    }
    else
    {
        bindFramebuffer(target, framebuffer);
    }

    switch (target)
    {
        case GL_FRAMEBUFFER:
            mDrawFramebufferBinding = framebuffer;
            mReadFramebufferBinding = framebuffer;
            break;
        case GL_DRAW_FRAMEBUFFER:
            mDrawFramebufferBinding = framebuffer;
            break;
        case GL_READ_FRAMEBUFFER:
            mReadFramebufferBinding = framebuffer;
            return;

        default:
            UNREACHABLE();
            break;
    }

    if (!mUseTimestampQueries)
        return;

    // We have at most one active timestamp query at a time. This code will end the current
    // query and immediately start a new one.
    if (mCurrentQuery.beginTimestampQuery != 0)
    {
        glGenQueriesEXT(1, &mCurrentQuery.endTimestampQuery);
        glQueryCounterEXT(mCurrentQuery.endTimestampQuery, GL_TIMESTAMP_EXT);
        mRunningQueries.push_back(mCurrentQuery);
        mCurrentQuery = {};
    }

    ASSERT(mCurrentQuery.beginTimestampQuery == 0);

    glGenQueriesEXT(1, &mCurrentQuery.beginTimestampQuery);
    glQueryCounterEXT(mCurrentQuery.beginTimestampQuery, GL_TIMESTAMP_EXT);
    mCurrentQuery.framebuffer = framebuffer;
}

std::string GetDiffPath()
{
#if defined(ANGLE_PLATFORM_WINDOWS)
    std::array<char, MAX_PATH> filenameBuffer = {};
    char *filenamePtr                         = nullptr;
    if (SearchPathA(NULL, "diff", ".exe", MAX_PATH, filenameBuffer.data(), &filenamePtr) == 0)
    {
        return "";
    }
    return std::string(filenameBuffer.data());
#else
    return "/usr/bin/diff";
#endif  // defined(ANGLE_PLATFORM_WINDOWS)
}

void PrintFileDiff(const char *aFilePath, const char *bFilePath)
{
    std::string pathToDiff = GetDiffPath();
    if (pathToDiff.empty())
    {
        printf("Could not find diff in the path.\n");
        return;
    }

    std::vector<const char *> args;
    args.push_back(pathToDiff.c_str());
    args.push_back(aFilePath);
    args.push_back(bFilePath);
    args.push_back("-u3");

    printf("Calling");
    for (const char *arg : args)
    {
        printf(" %s", arg);
    }
    printf("\n");

    ProcessHandle proc(LaunchProcess(args, ProcessOutputCapture::StdoutOnly));
    if (proc && proc->finish())
    {
        printf("\n%s\n", proc->getStdout().c_str());
    }
}

void TracePerfTest::validateSerializedState(const char *expectedCapturedSerializedState,
                                            const char *fileName,
                                            uint32_t line)
{
    if (!gTraceTestValidation)
    {
        return;
    }

    printf("Serialization checkpoint %s:%u...\n", fileName, line);

    const GLubyte *bytes                      = glGetString(GL_SERIALIZED_CONTEXT_STRING_ANGLE);
    const char *actualReplayedSerializedState = reinterpret_cast<const char *>(bytes);
    if (strcmp(expectedCapturedSerializedState, actualReplayedSerializedState) == 0)
    {
        printf("Serialization match.\n");
        return;
    }

    GTEST_NONFATAL_FAILURE_("Serialization mismatch!");

    const Optional<std::string> aFilePath = CreateTemporaryFile();
    const char *aFilePathCStr             = aFilePath.value().c_str();
    if (aFilePath.valid())
    {
        printf("Saving \"expected\" capture serialization to \"%s\".\n", aFilePathCStr);
        FILE *fpA = fopen(aFilePathCStr, "wt");
        ASSERT(fpA);
        fprintf(fpA, "%s", expectedCapturedSerializedState);
        fclose(fpA);
    }

    const Optional<std::string> bFilePath = CreateTemporaryFile();
    const char *bFilePathCStr             = bFilePath.value().c_str();
    if (bFilePath.valid())
    {
        printf("Saving \"actual\" replay serialization to \"%s\".\n", bFilePathCStr);
        FILE *fpB = fopen(bFilePathCStr, "wt");
        ASSERT(fpB);
        fprintf(fpB, "%s", actualReplayedSerializedState);
        fclose(fpB);
    }

    PrintFileDiff(aFilePathCStr, bFilePathCStr);
}

bool TracePerfTest::isDefaultFramebuffer(GLenum target) const
{
    switch (target)
    {
        case GL_FRAMEBUFFER:
        case GL_DRAW_FRAMEBUFFER:
            return (mDrawFramebufferBinding == 0);

        case GL_READ_FRAMEBUFFER:
            return (mReadFramebufferBinding == 0);

        default:
            UNREACHABLE();
            return false;
    }
}

GLenum ConvertDefaultFramebufferEnum(GLenum value)
{
    switch (value)
    {
        case GL_NONE:
            return GL_NONE;
        case GL_BACK:
        case GL_COLOR:
            return GL_COLOR_ATTACHMENT0;
        case GL_DEPTH:
            return GL_DEPTH_ATTACHMENT;
        case GL_STENCIL:
            return GL_STENCIL_ATTACHMENT;
        case GL_DEPTH_STENCIL:
            return GL_DEPTH_STENCIL_ATTACHMENT;
        default:
            UNREACHABLE();
            return GL_NONE;
    }
}

std::vector<GLenum> ConvertDefaultFramebufferEnums(GLsizei numAttachments,
                                                   const GLenum *attachments)
{
    std::vector<GLenum> translatedAttachments;
    for (GLsizei attachmentIndex = 0; attachmentIndex < numAttachments; ++attachmentIndex)
    {
        GLenum converted = ConvertDefaultFramebufferEnum(attachments[attachmentIndex]);
        translatedAttachments.push_back(converted);
    }
    return translatedAttachments;
}

// Needs special handling to treat the 0 framebuffer in offscreen mode.
void TracePerfTest::onReplayInvalidateFramebuffer(GLenum target,
                                                  GLsizei numAttachments,
                                                  const GLenum *attachments)
{
    if (mParams->surfaceType != SurfaceType::Offscreen || !isDefaultFramebuffer(target))
    {
        glInvalidateFramebuffer(target, numAttachments, attachments);
    }
    else
    {
        std::vector<GLenum> translatedAttachments =
            ConvertDefaultFramebufferEnums(numAttachments, attachments);
        glInvalidateFramebuffer(target, numAttachments, translatedAttachments.data());
    }
}

void TracePerfTest::onReplayInvalidateSubFramebuffer(GLenum target,
                                                     GLsizei numAttachments,
                                                     const GLenum *attachments,
                                                     GLint x,
                                                     GLint y,
                                                     GLsizei width,
                                                     GLsizei height)
{
    if (mParams->surfaceType != SurfaceType::Offscreen || !isDefaultFramebuffer(target))
    {
        glInvalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);
    }
    else
    {
        std::vector<GLenum> translatedAttachments =
            ConvertDefaultFramebufferEnums(numAttachments, attachments);
        glInvalidateSubFramebuffer(target, numAttachments, translatedAttachments.data(), x, y,
                                   width, height);
    }
}

void TracePerfTest::onReplayDrawBuffers(GLsizei n, const GLenum *bufs)
{
    if (mParams->surfaceType != SurfaceType::Offscreen ||
        !isDefaultFramebuffer(GL_DRAW_FRAMEBUFFER))
    {
        glDrawBuffers(n, bufs);
    }
    else
    {
        std::vector<GLenum> translatedBufs = ConvertDefaultFramebufferEnums(n, bufs);
        glDrawBuffers(n, translatedBufs.data());
    }
}

void TracePerfTest::onReplayReadBuffer(GLenum src)
{
    if (mParams->surfaceType != SurfaceType::Offscreen ||
        !isDefaultFramebuffer(GL_READ_FRAMEBUFFER))
    {
        glReadBuffer(src);
    }
    else
    {
        GLenum translated = ConvertDefaultFramebufferEnum(src);
        glReadBuffer(translated);
    }
}

void TracePerfTest::onReplayDiscardFramebufferEXT(GLenum target,
                                                  GLsizei numAttachments,
                                                  const GLenum *attachments)
{
    if (mParams->surfaceType != SurfaceType::Offscreen || !isDefaultFramebuffer(target))
    {
        glDiscardFramebufferEXT(target, numAttachments, attachments);
    }
    else
    {
        std::vector<GLenum> translatedAttachments =
            ConvertDefaultFramebufferEnums(numAttachments, attachments);
        glDiscardFramebufferEXT(target, numAttachments, translatedAttachments.data());
    }
}

void TracePerfTest::saveScreenshotIfEnabled(ScreenshotType screenshotType)
{
    if (gScreenshotDir != nullptr && gSaveScreenshots && !mScreenshotSaved &&
        (static_cast<uint32_t>(mScreenshotFrame) == mCurrentIteration ||
         mScreenshotFrame == kAllFrames))
    {
        std::stringstream screenshotNameStr;
        screenshotNameStr << gScreenshotDir << GetPathSeparator() << "angle" << mBackend << "_"
                          << mStory;

        // Add a marker to the name for any screenshot that isn't start frame
        if (mStartFrame != static_cast<uint32_t>(mScreenshotFrame))
        {
            if (screenshotType == ScreenshotType::kFrame)
            {
                screenshotNameStr << "_frame" << mCurrentIteration;
            }
            else
            {
                screenshotNameStr << "_grid" << mCurrentOffscreenGridIteration;
            }
        }

        screenshotNameStr << ".png";

        std::string screenshotName = screenshotNameStr.str();
        saveScreenshot(screenshotName);

        // Only set this value if we're capturing a single frame
        mScreenshotSaved = mScreenshotFrame != kAllFrames;
    }
}

void TracePerfTest::saveScreenshot(const std::string &screenshotName)
{
    // The frame is already rendered and is waiting in the default framebuffer.

    // RGBA 4-byte data.
    uint32_t pixelCount = mTestParams.windowWidth * mTestParams.windowHeight;
    std::vector<uint8_t> pixelData(pixelCount * 4);

    glFinish();

    // Backup the current pixel pack state
    GLint originalPackRowLength;
    GLint originalPackSkipRows;
    GLint originalPackSkipPixels;
    GLint originalPackAlignment;

    glGetIntegerv(GL_PACK_ROW_LENGTH, &originalPackRowLength);
    glGetIntegerv(GL_PACK_SKIP_ROWS, &originalPackSkipRows);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &originalPackSkipPixels);
    glGetIntegerv(GL_PACK_ALIGNMENT, &originalPackAlignment);

    // Set default pixel pack parameters (per ES 3.2 Table 16.1)
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);

    glReadPixels(0, 0, mTestParams.windowWidth, mTestParams.windowHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelData.data());

    // Restore the original pixel pack state
    glPixelStorei(GL_PACK_ROW_LENGTH, originalPackRowLength);
    glPixelStorei(GL_PACK_SKIP_ROWS, originalPackSkipRows);
    glPixelStorei(GL_PACK_SKIP_PIXELS, originalPackSkipPixels);
    glPixelStorei(GL_PACK_ALIGNMENT, originalPackAlignment);

    // Convert to RGB and flip y.
    std::vector<uint8_t> rgbData(pixelCount * 3);
    for (EGLint y = 0; y < mTestParams.windowHeight; ++y)
    {
        for (EGLint x = 0; x < mTestParams.windowWidth; ++x)
        {
            EGLint srcPixel = x + y * mTestParams.windowWidth;
            EGLint dstPixel = x + (mTestParams.windowHeight - y - 1) * mTestParams.windowWidth;
            memcpy(&rgbData[dstPixel * 3], &pixelData[srcPixel * 4], 3);
        }
    }

    if (!angle::SavePNGRGB(screenshotName.c_str(), "ANGLE Screenshot", mTestParams.windowWidth,
                           mTestParams.windowHeight, rgbData))
    {
        failTest(std::string("Error saving screenshot: ") + screenshotName);
        return;
    }
    else
    {
        printf("Saved screenshot: '%s'\n", screenshotName.c_str());
    }
}
}  // anonymous namespace

using namespace params;

void RegisterTraceTests()
{
    GLESDriverType driverType = GetDriverTypeFromString(gUseGL, GLESDriverType::AngleEGL);
    GLenum platformType       = EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE;
    GLenum deviceType         = EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE;
    if (IsANGLE(driverType))
    {
        platformType = GetPlatformANGLETypeFromArg(gUseANGLE, EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE);
        deviceType =
            GetANGLEDeviceTypeFromArg(gUseANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE);
    }

    char rootTracePath[kMaxPath] = {};
    if (!FindRootTraceTestDataPath(rootTracePath, kMaxPath))
    {
        ERR() << "Unable to find trace folder " << rootTracePath;
        return;
    }

    // Load JSON data.
    std::vector<std::string> traces;
    {
        char traceListPath[kMaxPath] = {};
        if (!angle::FindTestDataPath("gen/trace_list.json", traceListPath, kMaxPath))
        {
            ERR() << "Cannot find gen/trace_list.json";
            return;
        }

        if (!LoadTraceNamesFromJSON(traceListPath, &traces))
        {
            ERR() << "Unable to load traces from JSON file: " << traceListPath;
            return;
        }
    }

    std::vector<TraceInfo> traceInfos;
    for (const std::string &trace : traces)
    {
        std::stringstream traceJsonStream;
        traceJsonStream << rootTracePath << GetPathSeparator() << trace << GetPathSeparator()
                        << trace << ".json";
        std::string traceJsonPath = traceJsonStream.str();

        TraceInfo traceInfo = {};
        strncpy(traceInfo.name, trace.c_str(), kTraceInfoMaxNameLen);
        traceInfo.initialized = LoadTraceInfoFromJSON(trace, traceJsonPath, &traceInfo);

        traceInfos.push_back(traceInfo);
    }

    for (const TraceInfo &traceInfo : traceInfos)
    {
        const TracePerfParams params(traceInfo, driverType, platformType, deviceType);

        if (!IsPlatformAvailable(params))
        {
            continue;
        }

        std::function<ANGLEPerfTest *()> factory = [params]() -> ANGLEPerfTest * {
            if (params.isCL)
            {
                return CreateTracePerfTestCL(std::make_unique<TracePerfParams>(params));
            }
            return new TracePerfTest(std::make_unique<TracePerfParams>(params));
        };
        testing::RegisterTest("TraceTest", traceInfo.name, nullptr, nullptr, __FILE__, __LINE__,
                              factory);
    }
}
