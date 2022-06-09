//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TracePerf:
//   Performance test for ANGLE replaying traces.
//

#include <gtest/gtest.h>
#include "common/PackedEnums.h"
#include "common/string_utils.h"
#include "common/system_utils.h"
#include "restricted_traces/restricted_traces_export.h"
#include "tests/perf_tests/ANGLEPerfTest.h"
#include "tests/perf_tests/ANGLEPerfTestArgs.h"
#include "tests/perf_tests/DrawCallPerfParams.h"
#include "util/capture/frame_capture_test_utils.h"
#include "util/egl_loader_autogen.h"
#include "util/png_utils.h"
#include "util/test_utils.h"

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
constexpr size_t kMaxPath = 1024;

struct TracePerfParams final : public RenderTestParams
{
    // Common default options
    TracePerfParams()
    {
        // Display the frame after every drawBenchmark invocation
        iterationsPerStep = 1;
    }

    std::string story() const override
    {
        std::stringstream strstr;
        strstr << RenderTestParams::story() << "_" << traceInfo.name;
        return strstr.str();
    }

    TraceInfo traceInfo = {};
};

std::ostream &operator<<(std::ostream &os, const TracePerfParams &params)
{
    os << params.backendAndStory().substr(1);
    return os;
}

class TracePerfTest : public ANGLERenderTest
{
  public:
    TracePerfTest(const TracePerfParams &params);

    void initializeBenchmark() override;
    void destroyBenchmark() override;
    void drawBenchmark() override;

    // TODO(http://www.anglebug.com/5878): Add support for creating EGLSurface:
    // - eglCreatePbufferSurface()
    // - eglCreateWindowSurface()
    EGLContext onEglCreateContext(EGLDisplay display,
                                  EGLConfig config,
                                  EGLContext share_context,
                                  EGLint const *attrib_list);
    void onEglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context);
    EGLContext onEglGetCurrentContext();
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
        const TraceInfo &traceInfo = mParams.traceInfo;
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
        return strncmp(name, mParams.traceInfo.name, kTraceInfoMaxNameLen) == 0;
    }

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
    void saveScreenshot(const std::string &screenshotName) override;
    void swap();

    const TracePerfParams mParams;

    uint32_t mStartFrame;
    uint32_t mEndFrame;

    // For tracking RenderPass/FBO change timing.
    QueryInfo mCurrentQuery = {};
    std::vector<QueryInfo> mRunningQueries;
    std::vector<TimeSample> mTimeline;

    std::string mStartingDirectory;
    bool mUseTimestampQueries                                           = false;
    static constexpr int mMaxOffscreenBufferCount                       = 2;
    std::array<GLuint, mMaxOffscreenBufferCount> mOffscreenFramebuffers = {0, 0};
    std::array<GLuint, mMaxOffscreenBufferCount> mOffscreenTextures     = {0, 0};
    GLuint mOffscreenDepthStencil                                       = 0;
    int mWindowWidth                                                    = 0;
    int mWindowHeight                                                   = 0;
    GLuint mDrawFramebufferBinding                                      = 0;
    GLuint mReadFramebufferBinding                                      = 0;
    uint32_t mCurrentFrame                                              = 0;
    uint32_t mOffscreenFrameCount                                       = 0;
    uint32_t mTotalFrameCount                                           = 0;
    bool mScreenshotSaved                                               = false;
    std::unique_ptr<TraceLibrary> mTraceLibrary;
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

void KHRONOS_APIENTRY DrawElementsBaseVertexMinimizedProc(GLenum mode,
                                                          GLsizei count,
                                                          GLenum type,
                                                          const void *indices,
                                                          GLint basevertex)
{
    glDrawElementsBaseVertex(GL_POINTS, 1, type, indices, basevertex);
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

    // GLES
    if (strcmp(procName, "glBindFramebuffer") == 0)
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
        if (strcmp(procName, "glViewport") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(ViewportMinimizedProc);
        }

        if (strcmp(procName, "glScissor") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(ScissorMinimizedProc);
        }

        // Interpose the calls that generate actual GPU work
        if (strcmp(procName, "glDrawElements") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(DrawElementsMinimizedProc);
        }
        if (strcmp(procName, "glDrawElementsIndirect") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(DrawElementsIndirectMinimizedProc);
        }
        if (strcmp(procName, "glDrawElementsInstanced") == 0 ||
            strcmp(procName, "glDrawElementsInstancedEXT") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(DrawElementsInstancedMinimizedProc);
        }
        if (strcmp(procName, "glDrawElementsBaseVertex") == 0 ||
            strcmp(procName, "glDrawElementsBaseVertexEXT") == 0 ||
            strcmp(procName, "glDrawElementsBaseVertexOES") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(DrawElementsBaseVertexMinimizedProc);
        }
        if (strcmp(procName, "glDrawElementsInstancedBaseVertex") == 0 ||
            strcmp(procName, "glDrawElementsInstancedBaseVertexEXT") == 0 ||
            strcmp(procName, "glDrawElementsInstancedBaseVertexOES") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(
                DrawElementsInstancedBaseVertexMinimizedProc);
        }
        if (strcmp(procName, "glDrawRangeElements") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(DrawRangeElementsMinimizedProc);
        }
        if (strcmp(procName, "glDrawArrays") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(DrawArraysMinimizedProc);
        }
        if (strcmp(procName, "glDrawArraysInstanced") == 0 ||
            strcmp(procName, "glDrawArraysInstancedEXT") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(DrawArraysInstancedMinimizedProc);
        }
        if (strcmp(procName, "glDrawArraysIndirect") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(DrawArraysIndirectMinimizedProc);
        }
        if (strcmp(procName, "glDispatchCompute") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(DispatchComputeMinimizedProc);
        }
        if (strcmp(procName, "glDispatchComputeIndirect") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(DispatchComputeIndirectMinimizedProc);
        }

        // Interpose the calls that generate data copying work
        if (strcmp(procName, "glBufferData") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(BufferDataMinimizedProc);
        }
        if (strcmp(procName, "glBufferSubData") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(BufferSubDataMinimizedProc);
        }
        if (strcmp(procName, "glMapBufferRange") == 0 ||
            strcmp(procName, "glMapBufferRangeEXT") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(MapBufferRangeMinimizedProc);
        }
        if (strcmp(procName, "glTexImage2D") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(TexImage2DMinimizedProc);
        }
        if (strcmp(procName, "glTexImage3D") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(TexImage3DMinimizedProc);
        }
        if (strcmp(procName, "glTexSubImage2D") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(TexSubImage2DMinimizedProc);
        }
        if (strcmp(procName, "glTexSubImage3D") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(TexSubImage3DMinimizedProc);
        }
        if (strcmp(procName, "glGenerateMipmap") == 0 ||
            strcmp(procName, "glGenerateMipmapOES") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(GenerateMipmapMinimizedProc);
        }
        if (strcmp(procName, "glBlitFramebuffer") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(BlitFramebufferMinimizedProc);
        }
        if (strcmp(procName, "glReadPixels") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(ReadPixelsMinimizedProc);
        }
        if (strcmp(procName, "glBeginTransformFeedback") == 0)
        {
            return reinterpret_cast<angle::GenericProc>(BeginTransformFeedbackMinimizedProc);
        }
    }

    return gCurrentTracePerfTest->getGLWindow()->getProcAddress(procName);
}

void ValidateSerializedState(const char *serializedState, const char *fileName, uint32_t line)
{
    gCurrentTracePerfTest->validateSerializedState(serializedState, fileName, line);
}

constexpr char kTraceTestFolder[] = "src/tests/restricted_traces";

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

TracePerfTest::TracePerfTest(const TracePerfParams &params)
    : ANGLERenderTest("TracePerf", params, "ms"), mParams(params), mStartFrame(0), mEndFrame(0)
{
    // TODO: http://anglebug.com/4533 This fails after the upgrade to the 26.20.100.7870 driver.
    if (IsWindows() && IsIntel() && mParams.isVulkan() && traceNameIs("manhattan_10"))
    {
        mSkipTest = true;
    }

    // TODO: http://anglebug.com/4731 Fails on older Intel drivers. Passes in newer.
    if (IsWindows() && IsIntel() && !mParams.isANGLE() && traceNameIs("angry_birds_2_1500"))
    {
        mSkipTest = true;
    }

    if (traceNameIs("cod_mobile"))
    {
        // TODO: http://anglebug.com/4967 Vulkan: GL_EXT_color_buffer_float not supported on Pixel 2
        // The COD:Mobile trace uses a framebuffer attachment with:
        //   format = GL_RGB
        //   type = GL_UNSIGNED_INT_10F_11F_11F_REV
        // That combination is only renderable if GL_EXT_color_buffer_float is supported.
        // It happens to not be supported on Pixel 2's Vulkan driver.
        addExtensionPrerequisite("GL_EXT_color_buffer_float");

        // TODO: http://anglebug.com/4731 This extension is missing on older Intel drivers.
        addExtensionPrerequisite("GL_OES_EGL_image_external");

        // Flaky on Intel/windows http://anglebug.com/6568
        if (IsWindows() && IsIntel())
        {
            mSkipTest = true;
        }
    }

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

        // TODO(https://anglebug.com/5517) Linux+Intel generates "Framebuffer is incomplete" errors.
        if (IsLinux() && IsIntel() && mParams.isVulkan())
        {
            mSkipTest = true;
        }
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

        // Failing on Linux Intel and AMD due to invalid enum. http://anglebug.com/5822
        if (IsLinux() && (IsIntel() || IsAMD()) && mParams.driver != GLESDriverType::AngleEGL)
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("asphalt_8"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("hearthstone"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("efootball_pes_2021"))
    {
        // TODO(https://anglebug.com/5517) Linux+Intel and Pixel 2 generate "Framebuffer is
        // incomplete" errors with the Vulkan backend.
        if (mParams.isVulkan() && ((IsLinux() && IsIntel()) || IsPixel2()))
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("manhattan_31"))
    {
        // TODO: http://anglebug.com/5591 Trace crashes on Pixel 2 in vulkan driver
        if (IsPixel2() && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("idle_heroes"))
    {
        // TODO: http://anglebug.com/5591 Trace crashes on Pixel 2
        if (IsPixel2())
        {
            mSkipTest = true;
        }
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

    if (traceNameIs("happy_color"))
    {
        if (IsWindows() && IsAMD() && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("bus_simulator_indonesia"))
    {
        // TODO(https://anglebug.com/5629) Linux+(Intel|AMD) native GLES generates
        // GL_INVALID_OPERATION
        if (IsLinux() && (IsIntel() || IsAMD()) && !mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("messenger_lite"))
    {
        // TODO: https://anglebug.com/5663 Incorrect pixels on NVIDIA Windows for first frame
        if (IsWindows() && IsNVIDIA() && mParams.isVulkan() && !mParams.isSwiftshader())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("among_us"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("car_parking_multiplayer"))
    {
        // TODO: https://anglebug.com/5613 NVIDIA native driver spews undefined behavior warnings
        if (IsNVIDIA() && !mParams.isVulkan())
        {
            mSkipTest = true;
        }
        // TODO: https://anglebug.com/5724 Device lost on Win Intel
        if (IsWindows() && IsIntel() && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("fifa_mobile"))
    {
        // TODO: http://anglebug.com/5875 Intel Windows Vulkan flakily renders entirely black
        if (IsWindows() && IsIntel() && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("rope_hero_vice_town"))
    {
        // TODO: http://anglebug.com/5716 Trace crashes on Pixel 2 in vulkan driver
        if (IsPixel2() && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("extreme_car_driving_simulator"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("plants_vs_zombies_2"))
    {
        // TODO: http://crbug.com/1187752 Corrupted image
        if (IsWindows() && IsAMD() && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("junes_journey"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("ragnarok_m_eternal_love"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");

        // TODO: http://anglebug.com/5772 Pixel 2 errors with "Framebuffer is incomplete" on Vulkan
        if (IsPixel2() && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("real_cricket_20"))
    {
        // TODO: http://anglebug.com/5777 ARM doesn't have enough VS storage blocks
        if (IsAndroid() && IsARM())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("league_of_legends_wild_rift"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");

        // TODO: http://anglebug.com/5815 Trace is crashing on Intel Linux
        if (IsLinux() && IsIntel() && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("aztec_ruins"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");

        // TODO: http://anglebug.com/5553 Pixel 2 errors with "Framebuffer is incomplete" on Vulkan
        if (IsPixel2() && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("dragon_raja"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");

        // TODO: http://anglebug.com/5807 Intel Linux and Pixel 2 error with "Framebuffer is
        // incomplete" on Vulkan
        if (((IsLinux() && IsIntel()) || IsPixel2()) && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    // Adreno gives a driver error with empty/small draw calls. http://anglebug.com/5823
    if (traceNameIs("hill_climb_racing") || traceNameIs("dead_trigger_2"))
    {
        if (IsAndroid() && (IsPixel2() || IsPixel4() || IsPixel4XL()) &&
            mParams.driver == GLESDriverType::SystemEGL)
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("avakin_life"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("professional_baseball_spirits"))
    {
        // TODO(https://anglebug.com/5827) Linux+Mesa/RADV Vulkan generates
        // GL_INVALID_FRAMEBUFFER_OPERATION.
        // Mesa versions below 20.3.5 produce the same issue on Linux+Mesa/Intel Vulkan
        if (IsLinux() && (IsAMD() || IsIntel()) && mParams.isVulkan() && !mParams.isSwiftshader())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("call_break_offline_card_game"))
    {
        // TODO: http://anglebug.com/5837 Intel Linux Vulkan errors with "Framebuffer is incomplete"
        if ((IsLinux() && IsIntel()) && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("slingshot_test1") || traceNameIs("slingshot_test2"))
    {
        // TODO: http://anglebug.com/5877 Trace crashes on Pixel 2 in vulkan driver
        if (IsPixel2() && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("ludo_king"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    // TODO: http://anglebug.com/5943 GL_INVALID_ENUM on Windows/Intel.
    if (traceNameIs("summoners_war"))
    {
        if (IsWindows() && IsIntel() && mParams.driver != GLESDriverType::AngleEGL)
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("pokemon_go"))
    {
        addExtensionPrerequisite("GL_EXT_texture_cube_map_array");
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");

        // TODO: http://anglebug.com/5989 Intel Linux crashing on teardown
        // TODO: http://anglebug.com/5994 Intel Windows timing out periodically
        if ((IsLinux() || IsWindows()) && IsIntel() && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("cookie_run_kingdom"))
    {
        addExtensionPrerequisite("GL_EXT_texture_cube_map_array");
        addExtensionPrerequisite("GL_OES_EGL_image_external");

        // TODO: http://anglebug.com/6017 ARM doesn't have enough VS storage blocks
        if (IsAndroid() && IsARM())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("genshin_impact"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");

        // TODO: http://anglebug.com/6023 Crashes on Pixel 2 in vulkan driver
        // TODO: http://anglebug.com/6029 Crashes on Linux Intel Vulkan
        if (((IsLinux() && IsIntel()) || IsPixel2()) && mParams.isVulkan())
        {
            mSkipTest = true;
        }

        // Genshin is too large to handle in 32-bit mode.
        if (!Is64Bit())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("mario_kart_tour"))
    {
        // Fails on native Mesa. http://anglebug.com/6711
        if (IsLinux() && IsIntel() && !mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("pubg_mobile_skydive") || traceNameIs("pubg_mobile_battle_royale"))
    {
        addExtensionPrerequisite("GL_EXT_texture_buffer");

        // TODO: http://anglebug.com/6240 Internal errors on Windows/Intel and NVIDIA
        if (((IsWindows() && IsIntel()) || IsNVIDIA()) && !mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("sakura_school_simulator"))
    {
        // Flaky on Intel. http://anglebug.com/6294
        if (IsWindows() && IsIntel())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("scrabble_go"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("world_of_kings"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
        // Flaky on Intel. http://anglebug.com/6372
        if (IsWindows() && IsIntel())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("nier_reincarnation"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("mini_world"))
    {
        // TODO: http://anglebug.com/6443 Vulkan Test failure on Pixel4XL due to vulkan validation
        // error VUID-vkDestroyBuffer-buffer-00922
        if (IsQualcomm() && mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("pokemon_unite"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");

        // http://anglebug.com/6548 - nondeterministic on Intel+Windows
        // Crashes on Linux Intel
        if (IsIntel())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("world_cricket_championship_2"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");

        // http://anglebug.com/6657 - Native test timing out on Intel Linux
        if (IsLinux() && IsIntel() && !mParams.isVulkan())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("zillow"))
    {
        // http://anglebug.com/6658 - Crashing in Vulkan backend
        if ((IsLinux() || IsWindows()) && IsNVIDIA() &&
            mParams.driver == GLESDriverType::AngleEGL && !mParams.isSwiftshader())
        {
            mSkipTest = true;
        }
    }

    if (traceNameIs("township"))
    {
        addExtensionPrerequisite("GL_OES_EGL_image_external");
    }

    if (traceNameIs("asphalt_9"))
    {
        addExtensionPrerequisite("GL_KHR_texture_compression_astc_ldr");
    }

    if (traceNameIs("pubg_mobile_launch"))
    {
        // http://anglebug.com/6850 - Crashing in Nvidia GLES driver.
        if (IsLinux() && IsNVIDIA() && mParams.driver != GLESDriverType::AngleEGL)
        {
            mSkipTest = true;
        }
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
        // http://anglebug.com/7000 - glTexStorage2DEXT is not exposed on Pixel 4 native
        addExtensionPrerequisite("GL_EXT_texture_storage");
    }

    if (traceNameIs("marvel_strike_force"))
    {
        // http://anglebug.com/7017 - Qualcomm native driver gets confused about the state of a
        // buffer that was recreated during the trace
        if ((IsAndroid() && IsQualcomm()) && mParams.driver != GLESDriverType::AngleEGL)
        {
            mSkipTest = true;
        }
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

    ASSERT(mParams.surfaceType == SurfaceType::Window || gEnableAllTraceTests);
    ASSERT(mParams.eglParameters.deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE ||
           gEnableAllTraceTests);

    // We already swap in TracePerfTest::drawBenchmark, no need to swap again in the harness.
    disableTestHarnessSwap();

    gCurrentTracePerfTest = this;

    if (gTraceTestValidation)
    {
        mStepsToRun = frameCount();
    }
}

void TracePerfTest::initializeBenchmark()
{
    const TraceInfo &traceInfo = mParams.traceInfo;

    mStartingDirectory = angle::GetCWD().value();

    std::stringstream traceNameStr;
    traceNameStr << "angle_restricted_trace_" << traceInfo.name;
    std::string traceName = traceNameStr.str();
    mTraceLibrary.reset(new TraceLibrary(traceName.c_str()));

    // To load the trace data path correctly we set the CWD to the executable dir.
    if (!IsAndroid())
    {
        std::string exeDir = angle::GetExecutableDirectory();
        angle::SetCWD(exeDir.c_str());
    }

    trace_angle::LoadEGL(TraceLoadProc);
    trace_angle::LoadGLES(TraceLoadProc);

    if (!mTraceLibrary->valid())
    {
        mSkipTest = true;
        FAIL() << "Could not load trace library.";
    }

    mStartFrame = traceInfo.frameStart;
    mEndFrame   = traceInfo.frameEnd;
    mTraceLibrary->setBinaryDataDecompressCallback(DecompressBinaryData);

    mTraceLibrary->setValidateSerializedStateCallback(ValidateSerializedState);

    char testDataDir[kMaxPath] = {};
    if (!FindTraceTestDataPath(traceInfo.name, testDataDir, kMaxPath))
    {
        mSkipTest = true;
        FAIL() << "Could not find test data folder.";
    }

    mTraceLibrary->setBinaryDataDir(testDataDir);

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
    mCurrentFrame = mStartFrame;

    if (IsAndroid())
    {
        // On Android, set the orientation used by the app, based on width/height
        getWindow()->setOrientation(mTestParams.windowWidth, mTestParams.windowHeight);
    }

    // If we're rendering offscreen we set up a default back buffer.
    if (mParams.surfaceType == SurfaceType::Offscreen)
    {
        if (!IsAndroid())
        {
            mWindowWidth *= 4;
            mWindowHeight *= 4;
        }

        glGenRenderbuffers(1, &mOffscreenDepthStencil);
        glBindRenderbuffer(GL_RENDERBUFFER, mOffscreenDepthStencil);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mWindowWidth, mWindowHeight);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        glGenFramebuffers(mMaxOffscreenBufferCount, mOffscreenFramebuffers.data());
        glGenTextures(mMaxOffscreenBufferCount, mOffscreenTextures.data());
        for (int i = 0; i < mMaxOffscreenBufferCount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, mOffscreenFramebuffers[i]);

            // Hard-code RGBA8/D24S8. This should be specified in the trace info.
            glBindTexture(GL_TEXTURE_2D, mOffscreenTextures[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mWindowWidth, mWindowHeight, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, nullptr);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   mOffscreenTextures[i], 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                      mOffscreenDepthStencil);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                      mOffscreenDepthStencil);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    // Potentially slow. Can load a lot of resources.
    mTraceLibrary->setupReplay();

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
    if (mParams.surfaceType == SurfaceType::Offscreen)
    {
        glDeleteTextures(mMaxOffscreenBufferCount, mOffscreenTextures.data());
        mOffscreenTextures.fill(0);

        glDeleteRenderbuffers(1, &mOffscreenDepthStencil);
        mOffscreenDepthStencil = 0;

        glDeleteFramebuffers(mMaxOffscreenBufferCount, mOffscreenFramebuffers.data());
        mOffscreenFramebuffers.fill(0);
    }

    mTraceLibrary->finishReplay();
    mTraceLibrary.reset(nullptr);

    // In order for the next test to load, restore the working directory
    angle::SetCWD(mStartingDirectory.c_str());
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
    constexpr uint32_t kFramesPerXY = kFramesPerY * kFramesPerX;

    const uint32_t kOffscreenOffsetX =
        static_cast<uint32_t>(static_cast<double>(mTestParams.windowWidth) / 3.0f);
    const uint32_t kOffscreenOffsetY =
        static_cast<uint32_t>(static_cast<double>(mTestParams.windowHeight) / 3.0f);
    const uint32_t kOffscreenWidth  = kOffscreenOffsetX;
    const uint32_t kOffscreenHeight = kOffscreenOffsetY;

    const uint32_t kOffscreenFrameWidth = static_cast<uint32_t>(
        static_cast<double>(kOffscreenWidth / static_cast<double>(kFramesPerX)));
    const uint32_t kOffscreenFrameHeight = static_cast<uint32_t>(
        static_cast<double>(kOffscreenHeight / static_cast<double>(kFramesPerY)));

    // Add a time sample from GL and the host.
    if (mCurrentFrame == mStartFrame)
    {
        sampleTime();
    }

    if (mParams.surfaceType == SurfaceType::Offscreen)
    {
        // Some driver (ARM and ANGLE) try to nop or defer the glFlush if it is called within the
        // renderpass to avoid breaking renderpass (performance reason). For app traces that does
        // not use any FBO, when we run in the offscreen mode, there is no frame boundary and
        // glFlush call we issued at end of frame will get skipped. To overcome this (and also
        // matches what onscreen double buffering behavior as well), we use two offscreen FBOs and
        // ping pong between them for each frame.
        glBindFramebuffer(GL_FRAMEBUFFER,
                          mOffscreenFramebuffers[mTotalFrameCount % mMaxOffscreenBufferCount]);
    }

    char frameName[32];
    sprintf(frameName, "Frame %u", mCurrentFrame);
    beginInternalTraceEvent(frameName);

    startGpuTimer();
    mTraceLibrary->replayFrame(mCurrentFrame);
    stopGpuTimer();

    updatePerfCounters();

    if (mParams.surfaceType == SurfaceType::Offscreen)
    {
        if (gMinimizeGPUWork)
        {
            // To keep GPU work minimum, we skip the blit.
            glFlush();
            mOffscreenFrameCount++;
        }
        else
        {
            GLint currentDrawFBO, currentReadFBO;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &currentDrawFBO);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &currentReadFBO);

            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBindFramebuffer(
                GL_READ_FRAMEBUFFER,
                mOffscreenFramebuffers[mOffscreenFrameCount % mMaxOffscreenBufferCount]);

            uint32_t frameX  = (mOffscreenFrameCount % kFramesPerXY) % kFramesPerX;
            uint32_t frameY  = (mOffscreenFrameCount % kFramesPerXY) / kFramesPerX;
            uint32_t windowX = kOffscreenOffsetX + frameX * kOffscreenFrameWidth;
            uint32_t windowY = kOffscreenOffsetY + frameY * kOffscreenFrameHeight;

            GLboolean scissorTest = GL_FALSE;
            glGetBooleanv(GL_SCISSOR_TEST, &scissorTest);

            if (scissorTest)
            {
                glDisable(GL_SCISSOR_TEST);
            }

            glBlitFramebuffer(0, 0, mWindowWidth, mWindowHeight, windowX, windowY,
                              windowX + kOffscreenFrameWidth, windowY + kOffscreenFrameHeight,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);

            if (frameX == kFramesPerX - 1 && frameY == kFramesPerY - 1)
            {
                swap();
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, currentDrawFBO);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, currentReadFBO);
        }

        mTotalFrameCount++;
    }
    else
    {
        swap();
    }

    endInternalTraceEvent(frameName);

    if (mCurrentFrame == mEndFrame)
    {
        mTraceLibrary->resetReplay();
        mCurrentFrame = mStartFrame;
    }
    else
    {
        mCurrentFrame++;
    }

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
            sprintf(fboName, "FBO %u", query.framebuffer);

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

// Triggered when the replay calls glBindFramebuffer.
void TracePerfTest::onReplayFramebufferChange(GLenum target, GLuint framebuffer)
{
    if (framebuffer == 0 && mParams.surfaceType == SurfaceType::Offscreen)
    {
        glBindFramebuffer(target,
                          mOffscreenFramebuffers[mTotalFrameCount % mMaxOffscreenBufferCount]);
    }
    else
    {
        glBindFramebuffer(target, framebuffer);
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

    char aFilePath[kMaxPath] = {};
    if (CreateTemporaryFile(aFilePath, kMaxPath))
    {
        printf("Saving \"expected\" capture serialization to \"%s\".\n", aFilePath);
        FILE *fpA = fopen(aFilePath, "wt");
        ASSERT(fpA);
        fprintf(fpA, "%s", expectedCapturedSerializedState);
        fclose(fpA);
    }

    char bFilePath[kMaxPath] = {};
    if (CreateTemporaryFile(bFilePath, kMaxPath))
    {
        printf("Saving \"actual\" replay serialization to \"%s\".\n", bFilePath);
        FILE *fpB = fopen(bFilePath, "wt");
        ASSERT(fpB);
        fprintf(fpB, "%s", actualReplayedSerializedState);
        fclose(fpB);
    }

    PrintFileDiff(aFilePath, bFilePath);
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
    if (mParams.surfaceType != SurfaceType::Offscreen || !isDefaultFramebuffer(target))
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
    if (mParams.surfaceType != SurfaceType::Offscreen || !isDefaultFramebuffer(target))
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
    if (mParams.surfaceType != SurfaceType::Offscreen || !isDefaultFramebuffer(GL_DRAW_FRAMEBUFFER))
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
    if (mParams.surfaceType != SurfaceType::Offscreen || !isDefaultFramebuffer(GL_READ_FRAMEBUFFER))
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
    if (mParams.surfaceType != SurfaceType::Offscreen || !isDefaultFramebuffer(target))
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

void TracePerfTest::swap()
{
    // Capture a screenshot if enabled.
    if (gScreenShotDir != nullptr && !mScreenshotSaved &&
        static_cast<uint32_t>(gScreenShotFrame) == mCurrentFrame)
    {
        std::stringstream screenshotNameStr;
        screenshotNameStr << gScreenShotDir << GetPathSeparator() << "angle" << mBackend << "_"
                          << mStory;

        // Add a marker to the name for any screenshot that isn't start frame
        if (mStartFrame != static_cast<uint32_t>(gScreenShotFrame))
        {
            screenshotNameStr << "_frame" << gScreenShotFrame;
        }

        screenshotNameStr << ".png";

        std::string screenshotName = screenshotNameStr.str();
        saveScreenshot(screenshotName);
        mScreenshotSaved = true;
    }

    getGLWindow()->swap();
}

void TracePerfTest::saveScreenshot(const std::string &screenshotName)
{
    // The frame is already rendered and is waiting in the default framebuffer.

    // RGBA 4-byte data.
    uint32_t pixelCount = mTestParams.windowWidth * mTestParams.windowHeight;
    std::vector<uint8_t> pixelData(pixelCount * 4);

    // Only unbind the framebuffer on context versions where it's available.
    if (mParams.traceInfo.contextClientMajorVersion > 1)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glReadPixels(0, 0, mTestParams.windowWidth, mTestParams.windowHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixelData.data());

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
        FAIL() << "Error saving screenshot: " << screenshotName;
    }
    else
    {
        printf("Saved screenshot: '%s'\n", screenshotName.c_str());
    }
}

TracePerfParams CombineWithTraceInfo(const TracePerfParams &in, const TraceInfo &traceInfo)
{
    TracePerfParams out = in;
    out.traceInfo       = traceInfo;
    out.majorVersion    = traceInfo.contextClientMajorVersion;
    out.minorVersion    = traceInfo.contextClientMinorVersion;
    out.windowWidth     = traceInfo.drawSurfaceWidth;
    out.windowHeight    = traceInfo.drawSurfaceHeight;
    out.colorSpace      = traceInfo.drawSurfaceColorSpace;
    return out;
}

TracePerfParams CombineWithSurfaceType(const TracePerfParams &in, SurfaceType surfaceType)
{
    TracePerfParams out = in;
    out.surfaceType     = surfaceType;

    if (!IsAndroid() && surfaceType == SurfaceType::Offscreen)
    {
        out.windowWidth /= 4;
        out.windowHeight /= 4;
    }

    // We track GPU time only in frame-rate-limited cases.
    out.trackGpuTime = surfaceType == SurfaceType::WindowWithVSync;

    return out;
}

}  // anonymous namespace

using namespace params;
using P  = TracePerfParams;
using PV = std::vector<P>;

void RegisterTraceTests()
{
    // To load the trace data path correctly we set the CWD to the executable dir.
    std::string previousCWD;
    if (!IsAndroid())
    {
        previousCWD        = GetCWD().value();
        std::string exeDir = GetExecutableDirectory();
        SetCWD(exeDir.c_str());
    }

    char rootTracePath[kMaxPath] = {};
    if (!FindRootTraceTestDataPath(rootTracePath, kMaxPath))
    {
        ERR() << "Unable to find trace folder.";
        return;
    }

    // Load JSON data.
    std::vector<std::string> traces;
    {
        std::stringstream tracesJsonStream;
        tracesJsonStream << rootTracePath << GetPathSeparator() << "restricted_traces.json";
        std::string tracesJsonPath = tracesJsonStream.str();

        if (!LoadTraceNamesFromJSON(tracesJsonPath, &traces))
        {
            ERR() << "Unable to load traces from JSON file: " << tracesJsonPath;
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
        if (!LoadTraceInfoFromJSON(trace, traceJsonPath, &traceInfo))
        {
            ERR() << "Unable to load traced data from JSON file: " << traceJsonPath;
            return;
        }

        traceInfos.push_back(traceInfo);
    }

    std::vector<SurfaceType> surfaceTypes = {SurfaceType::Window};
    if (gEnableAllTraceTests)
    {
        surfaceTypes.push_back(SurfaceType::Offscreen);
        surfaceTypes.push_back(SurfaceType::WindowWithVSync);
    }

    std::vector<ModifierFunc<P>> renderers = {Vulkan<P>, Native<P>};
    if (gEnableAllTraceTests)
    {
        if (!IsAndroid())
        {
            renderers.push_back(VulkanMockICD<P>);
        }
        renderers.push_back(VulkanSwiftShader<P>);
    }

    PV withTraceInfo   = CombineWithValues({P()}, traceInfos, CombineWithTraceInfo);
    PV withSurfaceType = CombineWithValues(withTraceInfo, surfaceTypes, CombineWithSurfaceType);
    PV withRenderer    = CombineWithFuncs(withSurfaceType, renderers);

    for (const TracePerfParams &params : withRenderer)
    {
        if (!IsPlatformAvailable(params))
        {
            continue;
        }

        // Force on features if we're validating serialization.
        TracePerfParams overrideParams = params;
        if (gTraceTestValidation)
        {
            // Enable limits when validating traces because we usually turn off capture.
            overrideParams.eglParameters.captureLimits = EGL_TRUE;

            // This feature should also be enabled in capture to mirror the replay.
            overrideParams.eglParameters.forceInitShaderVariables = EGL_TRUE;
        }

        auto factory          = [overrideParams]() { return new TracePerfTest(overrideParams); };
        std::string paramName = testing::PrintToString(params);
        std::stringstream testNameStr;
        testNameStr << "Run/" << paramName;
        std::string testName = testNameStr.str();
        testing::RegisterTest("TracePerfTest", testName.c_str(), nullptr, paramName.c_str(),
                              __FILE__, __LINE__, factory);
    }

    if (!previousCWD.empty())
    {
        SetCWD(previousCWD.c_str());
    }
}
