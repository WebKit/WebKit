//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FramebufferFetchTest:
//   Tests the correctness of the EXT_shader_framebuffer_fetch and the
//   EXT_shader_framebuffer_fetch_non_coherent extensions.
//

#include "common/debug.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

namespace angle
{
//
// Shared Vertex Shaders for the tests below
//
// A 1.0 GLSL vertex shader
static constexpr char k100VS[] = R"(#version 100
attribute vec4 a_position;

void main (void)
{
    gl_Position = a_position;
})";

// A 3.1 GLSL vertex shader
static constexpr char k310VS[] = R"(#version 310 es
in highp vec4 a_position;

void main (void)
{
    gl_Position = a_position;
})";

// Shared simple (i.e. no framebuffer fetch) Fragment Shaders for the tests below
//
// Simple (i.e. no framebuffer fetch) 3.1 GLSL fragment shader that writes to 1 attachment
static constexpr char k310NoFetch1AttachmentFS[] = R"(#version 310 es
layout(location = 0) out highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color = u_color;
})";

// Shared Coherent Fragment Shaders for the tests below
//
// Coherent version of a 1.0 GLSL fragment shader that uses gl_LastFragData
static constexpr char k100CoherentFS[] = R"(#version 100
#extension GL_EXT_shader_framebuffer_fetch : require
mediump vec4 gl_LastFragData[gl_MaxDrawBuffers];
uniform highp vec4 u_color;

void main (void)
{
    gl_FragColor = u_color + gl_LastFragData[0];
})";

// Coherent version of a 3.1 GLSL fragment shader that writes to 1 attachment
static constexpr char k310Coherent1AttachmentFS[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch : require
layout(location = 0) inout highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color += u_color;
})";

// Coherent version of a 3.1 GLSL fragment shader that writes the output to a storage buffer.
static constexpr char k310CoherentStorageBuffer[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch : require
layout(location = 0) inout highp vec4 o_color;

layout(std140, binding = 0) buffer outBlock {
    highp vec4 data[256];
};

uniform highp vec4 u_color;
void main (void)
{
    uint index = uint(gl_FragCoord.y) * 16u + uint(gl_FragCoord.x);
    data[index] = o_color;
    o_color += u_color;
})";

// Coherent version of a 3.1 GLSL fragment shader that writes to 4 attachments
static constexpr char k310Coherent4AttachmentFS[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch : require
layout(location = 0) inout highp vec4 o_color0;
layout(location = 1) inout highp vec4 o_color1;
layout(location = 2) inout highp vec4 o_color2;
layout(location = 3) inout highp vec4 o_color3;
uniform highp vec4 u_color;

void main (void)
{
    o_color0 += u_color;
    o_color1 += u_color;
    o_color2 += u_color;
    o_color3 += u_color;
})";

// Coherent version of a 3.1 GLSL fragment shader that writes to 4 attachments via an inout
// array
static constexpr char k310Coherent4AttachmentArrayFS[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch : require
inout highp vec4 o_color[4];
uniform highp vec4 u_color;

void main (void)
{
    o_color[0] += u_color;
    o_color[1] += u_color;
    o_color[2] += u_color;
    o_color[3] += u_color;
})";

// Coherent version of a 3.1 GLSL fragment shader that writes to 4 attachments with the order of
// non-fetch program and fetch program with different attachments (version 1)
static constexpr char k310CoherentDifferent4AttachmentFS1[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch : require
layout(location = 0) inout highp vec4 o_color0;
layout(location = 1) out highp vec4 o_color1;
layout(location = 2) inout highp vec4 o_color2;
layout(location = 3) out highp vec4 o_color3;
uniform highp vec4 u_color;

void main (void)
{
    o_color0 += u_color;
    o_color1 = u_color;
    o_color2 += u_color;
    o_color3 = u_color;
})";

// Coherent version of a 3.1 GLSL fragment shader that writes to 4 attachments with the order
// of non-fetch program and fetch program with different attachments (version 2)
static constexpr char k310CoherentDifferent4AttachmentFS2[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch : require
layout(location = 0) inout highp vec4 o_color0;
layout(location = 1) out highp vec4 o_color1;
layout(location = 2) out highp vec4 o_color2;
layout(location = 3) inout highp vec4 o_color3;
uniform highp vec4 u_color;

void main (void)
{
    o_color0 += u_color;
    o_color1 = u_color;
    o_color2 = u_color;
    o_color3 += u_color;
})";

// Shared Non-Coherent Fragment Shaders for the tests below
//
// Non-coherent version of a 1.0 GLSL fragment shader that uses gl_LastFragData
static constexpr char k100NonCoherentFS[] = R"(#version 100
#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require
layout(noncoherent) mediump vec4 gl_LastFragData[gl_MaxDrawBuffers];
uniform highp vec4 u_color;

void main (void)
{
    gl_FragColor = u_color + gl_LastFragData[0];
})";

// Non-coherent version of a 3.1 GLSL fragment shader that writes to 1 attachment
static constexpr char k310NonCoherent1AttachmentFS[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require
layout(noncoherent, location = 0) inout highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color += u_color;
})";

// Non-coherent version of a 3.1 GLSL fragment shader that writes the output to a storage buffer.
static constexpr char k310NonCoherentStorageBuffer[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require
layout(noncoherent) inout highp vec4 o_color;

layout(std140, binding = 0) buffer outBlock {
    highp vec4 data[256];
};

uniform highp vec4 u_color;
void main (void)
{
    uint index = uint(gl_FragCoord.y) * 16u + uint(gl_FragCoord.x);
    data[index] = o_color;
    o_color += u_color;
})";

// Non-coherent version of a 3.1 GLSL fragment shader that writes to 4 attachments
static constexpr char k310NonCoherent4AttachmentFS[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require
layout(noncoherent, location = 0) inout highp vec4 o_color0;
layout(noncoherent, location = 1) inout highp vec4 o_color1;
layout(noncoherent, location = 2) inout highp vec4 o_color2;
layout(noncoherent, location = 3) inout highp vec4 o_color3;
uniform highp vec4 u_color;

void main (void)
{
    o_color0 += u_color;
    o_color1 += u_color;
    o_color2 += u_color;
    o_color3 += u_color;
})";

// Non-coherent version of a 3.1 GLSL fragment shader that writes to 4 attachments via an inout
// array
static constexpr char k310NonCoherent4AttachmentArrayFS[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require
layout(noncoherent, location = 0) inout highp vec4 o_color[4];
uniform highp vec4 u_color;

void main (void)
{
    o_color[0] += u_color;
    o_color[1] += u_color;
    o_color[2] += u_color;
    o_color[3] += u_color;
})";

// Non-coherent version of a 3.1 GLSL fragment shader that writes to 4 attachments with the order
// of non-fetch program and fetch program with different attachments (version 1)
static constexpr char k310NonCoherentDifferent4AttachmentFS1[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require
layout(noncoherent, location = 0) inout highp vec4 o_color0;
layout(location = 1) out highp vec4 o_color1;
layout(noncoherent, location = 2) inout highp vec4 o_color2;
layout(location = 3) out highp vec4 o_color3;
uniform highp vec4 u_color;

void main (void)
{
    o_color0 += u_color;
    o_color1 = u_color;
    o_color2 += u_color;
    o_color3 = u_color;
})";

// Non-coherent version of a 3.1 GLSL fragment shader that writes to 4 attachments with the order
// of non-fetch program and fetch program with different attachments (version 2)
static constexpr char k310NonCoherentDifferent4AttachmentFS2[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require
layout(noncoherent, location = 0) inout highp vec4 o_color0;
layout(location = 1) out highp vec4 o_color1;
layout(location = 2) out highp vec4 o_color2;
layout(noncoherent, location = 3) inout highp vec4 o_color3;
uniform highp vec4 u_color;

void main (void)
{
    o_color0 += u_color;
    o_color1 = u_color;
    o_color2 = u_color;
    o_color3 += u_color;
})";

// Shared Coherent Fragment Shaders for the tests below
//
// Coherent version of a 1.0 GLSL fragment shader that uses gl_LastFragColorARM
static constexpr char k100ARMFS[] = R"(#version 100
#extension GL_ARM_shader_framebuffer_fetch : require
mediump vec4 gl_LastFragColorARM;
uniform highp vec4 u_color;

void main (void)
{
    gl_FragColor = u_color + gl_LastFragColorARM;
})";

// ARM version of a 3.1 GLSL fragment shader that writes to 1 attachment
static constexpr char k310ARM1AttachmentFS[] = R"(#version 310 es
#extension GL_ARM_shader_framebuffer_fetch : require
layout(location = 0) out highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color = u_color + gl_LastFragColorARM;
})";

// ARM version of a 3.1 GLSL fragment shader that writes the output to a storage buffer.
static constexpr char k310ARMStorageBuffer[] = R"(#version 310 es
#extension GL_ARM_shader_framebuffer_fetch : require
layout(location = 0) out highp vec4 o_color;

layout(std140, binding = 0) buffer outBlock {
    highp vec4 data[256];
};

uniform highp vec4 u_color;
void main (void)
{
    uint index = uint(gl_FragCoord.y) * 16u + uint(gl_FragCoord.x);
    data[index] = gl_LastFragColorARM;
    o_color = u_color + gl_LastFragColorARM;
})";

class FramebufferFetchES31 : public ANGLETest<>
{
  protected:
    static constexpr GLuint kMaxColorBuffer = 4u;
    static constexpr GLuint kViewportWidth  = 16u;
    static constexpr GLuint kViewportHeight = 16u;

    FramebufferFetchES31()
    {
        setWindowWidth(16);
        setWindowHeight(16);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);

        mCoherentExtension = false;
        mARMExtension      = false;
    }

    enum WhichExtension
    {
        COHERENT,
        NON_COHERENT,
        ARM,
    };
    void setWhichExtension(WhichExtension whichExtension)
    {
        mCoherentExtension = (whichExtension == COHERENT || whichExtension == ARM) ? true : false;
        mARMExtension      = (whichExtension == ARM) ? true : false;
    }

    enum WhichFragmentShader
    {
        GLSL100,
        GLSL310_NO_FETCH_1ATTACHMENT,
        GLSL310_1ATTACHMENT,
        GLSL310_1ATTACHMENT_WITH_STORAGE_BUFFER,
        GLSL310_4ATTACHMENT,
        GLSL310_4ATTACHMENT_ARRAY,
        GLSL310_4ATTACHMENT_DIFFERENT1,
        GLSL310_4ATTACHMENT_DIFFERENT2,
    };
    const char *getFragmentShader(WhichFragmentShader whichFragmentShader)
    {
        if (mARMExtension)
        {
            // gl_LastFragColorARM cannot support multiple attachments
            switch (whichFragmentShader)
            {
                case GLSL100:
                    return k100ARMFS;
                case GLSL310_NO_FETCH_1ATTACHMENT:
                    return k310NoFetch1AttachmentFS;
                case GLSL310_1ATTACHMENT:
                    return k310ARM1AttachmentFS;
                case GLSL310_1ATTACHMENT_WITH_STORAGE_BUFFER:
                    return k310ARMStorageBuffer;
                default:
                    UNREACHABLE();
                    return nullptr;
            }
        }
        else if (mCoherentExtension)
        {
            switch (whichFragmentShader)
            {
                case GLSL100:
                    return k100CoherentFS;
                case GLSL310_NO_FETCH_1ATTACHMENT:
                    return k310NoFetch1AttachmentFS;
                case GLSL310_1ATTACHMENT:
                    return k310Coherent1AttachmentFS;
                case GLSL310_1ATTACHMENT_WITH_STORAGE_BUFFER:
                    return k310CoherentStorageBuffer;
                case GLSL310_4ATTACHMENT:
                    return k310Coherent4AttachmentFS;
                case GLSL310_4ATTACHMENT_ARRAY:
                    return k310Coherent4AttachmentArrayFS;
                case GLSL310_4ATTACHMENT_DIFFERENT1:
                    return k310CoherentDifferent4AttachmentFS1;
                case GLSL310_4ATTACHMENT_DIFFERENT2:
                    return k310CoherentDifferent4AttachmentFS2;
                default:
                    UNREACHABLE();
                    return nullptr;
            }
        }
        else
        {
            switch (whichFragmentShader)
            {
                case GLSL100:
                    return k100NonCoherentFS;
                case GLSL310_NO_FETCH_1ATTACHMENT:
                    return k310NoFetch1AttachmentFS;
                case GLSL310_1ATTACHMENT:
                    return k310NonCoherent1AttachmentFS;
                case GLSL310_1ATTACHMENT_WITH_STORAGE_BUFFER:
                    return k310NonCoherentStorageBuffer;
                case GLSL310_4ATTACHMENT:
                    return k310NonCoherent4AttachmentFS;
                case GLSL310_4ATTACHMENT_ARRAY:
                    return k310NonCoherent4AttachmentArrayFS;
                case GLSL310_4ATTACHMENT_DIFFERENT1:
                    return k310NonCoherentDifferent4AttachmentFS1;
                case GLSL310_4ATTACHMENT_DIFFERENT2:
                    return k310NonCoherentDifferent4AttachmentFS2;
                default:
                    UNREACHABLE();
                    return nullptr;
            }
        }
    }

    void render(GLuint coordLoc, GLboolean needsFramebufferFetchBarrier)
    {
        const GLfloat coords[] = {
            -1.0f, -1.0f, +1.0f, -1.0f, +1.0f, +1.0f, -1.0f, +1.0f,
        };

        const GLushort indices[] = {
            0, 1, 2, 2, 3, 0,
        };

        glViewport(0, 0, kViewportWidth, kViewportHeight);

        GLBuffer coordinatesBuffer;
        GLBuffer elementsBuffer;

        glBindBuffer(GL_ARRAY_BUFFER, coordinatesBuffer);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(coords), coords, GL_STATIC_DRAW);
        glEnableVertexAttribArray(coordLoc);
        glVertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementsBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)sizeof(indices), &indices[0],
                     GL_STATIC_DRAW);

        if (needsFramebufferFetchBarrier)
        {
            glFramebufferFetchBarrierEXT();
        }

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);

        ASSERT_GL_NO_ERROR();
    }

    void BasicTest(GLProgram &program)
    {
        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        std::vector<GLColor> greenColor(kViewportWidth * kViewportHeight, GLColor::green);
        GLTexture colorBufferTex;
        glBindTexture(GL_TEXTURE_2D, colorBufferTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, greenColor.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBufferTex,
                               0);

        ASSERT_GL_NO_ERROR();

        float color[4]      = {1.0f, 0.0f, 0.0f, 1.0f};
        GLint colorLocation = glGetUniformLocation(program, "u_color");
        glUniform4fv(colorLocation, 1, color);

        GLint positionLocation = glGetAttribLocation(program, "a_position");
        render(positionLocation, !mCoherentExtension);

        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void MultipleRenderTargetTest(GLProgram &program)
    {
        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        std::vector<GLColor> color0(kViewportWidth * kViewportHeight, GLColor::black);
        std::vector<GLColor> color1(kViewportWidth * kViewportHeight, GLColor::green);
        std::vector<GLColor> color2(kViewportWidth * kViewportHeight, GLColor::blue);
        std::vector<GLColor> color3(kViewportWidth * kViewportHeight, GLColor::cyan);
        GLTexture colorBufferTex[kMaxColorBuffer];
        GLenum colorAttachments[kMaxColorBuffer] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                                                    GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
        glBindTexture(GL_TEXTURE_2D, colorBufferTex[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color0.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color1.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex[2]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color2.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex[3]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color3.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        for (unsigned int i = 0; i < kMaxColorBuffer; i++)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, colorAttachments[i], GL_TEXTURE_2D,
                                   colorBufferTex[i], 0);
        }
        glDrawBuffers(kMaxColorBuffer, &colorAttachments[0]);

        ASSERT_GL_NO_ERROR();

        float color[4]      = {1.0f, 0.0f, 0.0f, 1.0f};
        GLint colorLocation = glGetUniformLocation(program, "u_color");
        glUniform4fv(colorLocation, 1, color);

        GLint positionLocation = glGetAttribLocation(program, "a_position");
        render(positionLocation, !mCoherentExtension);

        ASSERT_GL_NO_ERROR();

        glReadBuffer(colorAttachments[0]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
        glReadBuffer(colorAttachments[1]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);
        glReadBuffer(colorAttachments[2]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);
        glReadBuffer(colorAttachments[3]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::white);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void MultipleRenderTargetArrayTest(GLProgram &program)
    {
        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        std::vector<GLColor> color0(kViewportWidth * kViewportHeight, GLColor::black);
        std::vector<GLColor> color1(kViewportWidth * kViewportHeight, GLColor::green);
        std::vector<GLColor> color2(kViewportWidth * kViewportHeight, GLColor::blue);
        std::vector<GLColor> color3(kViewportWidth * kViewportHeight, GLColor::cyan);
        GLTexture colorBufferTex[kMaxColorBuffer];
        GLenum colorAttachments[kMaxColorBuffer] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                                                    GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
        glBindTexture(GL_TEXTURE_2D, colorBufferTex[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color0.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color1.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex[2]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color2.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex[3]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color3.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        for (unsigned int i = 0; i < kMaxColorBuffer; i++)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, colorAttachments[i], GL_TEXTURE_2D,
                                   colorBufferTex[i], 0);
        }
        glDrawBuffers(kMaxColorBuffer, &colorAttachments[0]);

        ASSERT_GL_NO_ERROR();

        float color[4]      = {1.0f, 0.0f, 0.0f, 1.0f};
        GLint colorLocation = glGetUniformLocation(program, "u_color");
        glUniform4fv(colorLocation, 1, color);

        GLint positionLocation = glGetAttribLocation(program, "a_position");
        render(positionLocation, !mCoherentExtension);

        ASSERT_GL_NO_ERROR();

        glReadBuffer(colorAttachments[0]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
        glReadBuffer(colorAttachments[1]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);
        glReadBuffer(colorAttachments[2]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);
        glReadBuffer(colorAttachments[3]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::white);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void MultipleDrawTest(GLProgram &program)
    {
        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        std::vector<GLColor> greenColor(kViewportWidth * kViewportHeight, GLColor::green);
        GLTexture colorBufferTex;
        glBindTexture(GL_TEXTURE_2D, colorBufferTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, greenColor.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBufferTex,
                               0);

        ASSERT_GL_NO_ERROR();

        float color1[4]     = {1.0f, 0.0f, 0.0f, 1.0f};
        GLint colorLocation = glGetUniformLocation(program, "u_color");
        glUniform4fv(colorLocation, 1, color1);

        GLint positionLocation = glGetAttribLocation(program, "a_position");
        render(positionLocation, !mCoherentExtension);

        float color2[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        glUniform4fv(colorLocation, 1, color2);

        render(positionLocation, !mCoherentExtension);

        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::white);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void DrawNonFetchDrawFetchTest(GLProgram &programNonFetch, GLProgram &programFetch)
    {
        glUseProgram(programNonFetch);
        ASSERT_GL_NO_ERROR();

        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        std::vector<GLColor> greenColor(kViewportWidth * kViewportHeight, GLColor::green);
        GLTexture colorBufferTex;
        glBindTexture(GL_TEXTURE_2D, colorBufferTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, greenColor.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBufferTex,
                               0);

        ASSERT_GL_NO_ERROR();

        float colorRed[4]           = {1.0f, 0.0f, 0.0f, 1.0f};
        GLint colorLocationNonFetch = glGetUniformLocation(programNonFetch, "u_color");
        glUniform4fv(colorLocationNonFetch, 1, colorRed);

        GLint positionLocationNonFetch = glGetAttribLocation(programNonFetch, "a_position");
        // Render without regard to glFramebufferFetchBarrierEXT()
        render(positionLocationNonFetch, GL_FALSE);

        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);

        glUseProgram(programFetch);

        float colorGreen[4]      = {0.0f, 1.0f, 0.0f, 1.0f};
        GLint colorLocationFetch = glGetUniformLocation(programFetch, "u_color");
        glUniform4fv(colorLocationFetch, 1, colorGreen);

        GLint positionLocationFetch = glGetAttribLocation(programFetch, "a_position");
        // Render potentially with a glFramebufferFetchBarrierEXT() depending on the [non-]coherent
        // extension being used
        render(positionLocationFetch, !mCoherentExtension);

        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);

        glUseProgram(programNonFetch);
        glUniform4fv(colorLocationNonFetch, 1, colorRed);
        // Render without regard to glFramebufferFetchBarrierEXT()
        render(positionLocationNonFetch, GL_FALSE);

        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);

        glUseProgram(programFetch);
        glUniform4fv(colorLocationFetch, 1, colorGreen);
        // Render potentially with a glFramebufferFetchBarrierEXT() depending on the [non-]coherent
        // extension being used
        render(positionLocationFetch, !mCoherentExtension);

        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void DrawFetchDrawNonFetchTest(GLProgram &programNonFetch, GLProgram &programFetch)
    {
        glUseProgram(programFetch);
        ASSERT_GL_NO_ERROR();

        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        std::vector<GLColor> greenColor(kViewportWidth * kViewportHeight, GLColor::green);
        GLTexture colorBufferTex;
        glBindTexture(GL_TEXTURE_2D, colorBufferTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, greenColor.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBufferTex,
                               0);

        ASSERT_GL_NO_ERROR();

        float colorRed[4]        = {1.0f, 0.0f, 0.0f, 1.0f};
        GLint colorLocationFetch = glGetUniformLocation(programFetch, "u_color");
        glUniform4fv(colorLocationFetch, 1, colorRed);

        GLint positionLocationFetch = glGetAttribLocation(programFetch, "a_position");
        // Render potentially with a glFramebufferFetchBarrierEXT() depending on the [non-]coherent
        // extension being used
        render(positionLocationFetch, !mCoherentExtension);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);

        glUseProgram(programNonFetch);

        GLint colorLocationNonFetch = glGetUniformLocation(programNonFetch, "u_color");
        glUniform4fv(colorLocationNonFetch, 1, colorRed);

        GLint positionLocationNonFetch = glGetAttribLocation(programNonFetch, "a_position");
        // Render without regard to glFramebufferFetchBarrierEXT()
        render(positionLocationNonFetch, GL_FALSE);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);

        float colorGreen[4] = {0.0f, 1.0f, 0.0f, 1.0f};
        glUseProgram(programFetch);
        glUniform4fv(colorLocationFetch, 1, colorGreen);
        // Render potentially with a glFramebufferFetchBarrierEXT() depending on the [non-]coherent
        // extension being used
        render(positionLocationFetch, !mCoherentExtension);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);

        glUseProgram(programNonFetch);
        glUniform4fv(colorLocationNonFetch, 1, colorRed);
        // Render without regard to glFramebufferFetchBarrierEXT()
        render(positionLocationNonFetch, GL_FALSE);

        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    enum class StorageBufferTestPostFetchAction
    {
        Nothing,
        Clear,
    };

    void DrawNonFetchDrawFetchInStorageBufferTest(GLProgram &programNonFetch,
                                                  GLProgram &programFetch,
                                                  StorageBufferTestPostFetchAction postFetchAction)
    {
        // Create output buffer
        constexpr GLsizei kBufferSize = kViewportWidth * kViewportHeight * sizeof(float[4]);
        GLBuffer buffer;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, kBufferSize, nullptr, GL_STATIC_DRAW);
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, buffer, 0, kBufferSize);

        // Zero-initialize it
        void *bufferData = glMapBufferRange(
            GL_SHADER_STORAGE_BUFFER, 0, kBufferSize,
            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
        memset(bufferData, 0, kBufferSize);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        glUseProgram(programNonFetch);
        ASSERT_GL_NO_ERROR();

        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        std::vector<GLColor> initColor(kViewportWidth * kViewportHeight, GLColor{10, 20, 30, 40});
        GLTexture colorBufferTex;
        glBindTexture(GL_TEXTURE_2D, colorBufferTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, initColor.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBufferTex,
                               0);

        ASSERT_GL_NO_ERROR();

        float colorRed[4]           = {1.0f, 0.0f, 0.0f, 1.0f};
        GLint colorLocationNonFetch = glGetUniformLocation(programNonFetch, "u_color");
        glUniform4fv(colorLocationNonFetch, 1, colorRed);

        GLint positionLocationNonFetch = glGetAttribLocation(programNonFetch, "a_position");

        // Mask color output.  The no-fetch draw call should be a no-op, and the fetch draw-call
        // should only output to the storage buffer, but not the color attachment.
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

        // Render without regard to glFramebufferFetchBarrierEXT()
        render(positionLocationNonFetch, GL_FALSE);

        ASSERT_GL_NO_ERROR();

        glUseProgram(programFetch);

        float colorBlue[4]       = {0.0f, 0.0f, 1.0f, 1.0f};
        GLint colorLocationFetch = glGetUniformLocation(programFetch, "u_color");
        glUniform4fv(colorLocationFetch, 1, colorBlue);

        GLint positionLocationFetch = glGetAttribLocation(programFetch, "a_position");
        // Render potentially with a glFramebufferFetchBarrierEXT() depending on the [non-]coherent
        // extension being used
        render(positionLocationFetch, !mCoherentExtension);

        ASSERT_GL_NO_ERROR();

        // Enable the color mask and clear the alpha channel.  This shouldn't be reordered with the
        // fetch draw.
        GLColor expect = initColor[0];
        if (postFetchAction == StorageBufferTestPostFetchAction::Clear)
        {
            expect.A = 200;
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
            glClearColor(0.5, 0.6, 0.7, expect.A / 255.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // Since color is completely masked out, the texture should retain its original green color.
        EXPECT_PIXEL_COLOR_NEAR(kViewportWidth / 2, kViewportHeight / 2, expect, 1);

        // Read back the storage buffer and make sure framebuffer fetch worked as intended despite
        // masked color.
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

        const float *colorData = static_cast<const float *>(
            glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, kBufferSize, GL_MAP_READ_BIT));
        for (uint32_t y = 0; y < kViewportHeight; ++y)
        {
            for (uint32_t x = 0; x < kViewportWidth; ++x)
            {
                uint32_t ssboIndex = (y * kViewportWidth + x) * 4;
                EXPECT_NEAR(colorData[ssboIndex + 0], initColor[0].R / 255.0, 0.05);
                EXPECT_NEAR(colorData[ssboIndex + 1], initColor[0].G / 255.0, 0.05);
                EXPECT_NEAR(colorData[ssboIndex + 2], initColor[0].B / 255.0, 0.05);
                EXPECT_NEAR(colorData[ssboIndex + 3], initColor[0].A / 255.0, 0.05);
            }
        }
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void DrawNonFetchDrawFetchWithDifferentAttachmentsTest(GLProgram &programNonFetch,
                                                           GLProgram &programFetch)
    {
        glUseProgram(programNonFetch);
        ASSERT_GL_NO_ERROR();

        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        std::vector<GLColor> greenColor(kViewportWidth * kViewportHeight, GLColor::green);
        GLTexture colorTex;
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, greenColor.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

        ASSERT_GL_NO_ERROR();

        float colorRed[4]           = {1.0f, 0.0f, 0.0f, 1.0f};
        GLint colorLocationNonFetch = glGetUniformLocation(programNonFetch, "u_color");
        glUniform4fv(colorLocationNonFetch, 1, colorRed);

        GLint positionLocationNonFetch = glGetAttribLocation(programNonFetch, "a_position");
        // Render without regard to glFramebufferFetchBarrierEXT()
        render(positionLocationNonFetch, GL_FALSE);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);

        glUseProgram(programFetch);
        ASSERT_GL_NO_ERROR();

        GLFramebuffer framebufferMRT1;
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferMRT1);
        std::vector<GLColor> color1(kViewportWidth * kViewportHeight, GLColor::green);
        std::vector<GLColor> color2(kViewportWidth * kViewportHeight, GLColor::blue);
        GLTexture colorBufferTex1[kMaxColorBuffer];
        GLenum colorAttachments[kMaxColorBuffer] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                                                    GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
        glBindTexture(GL_TEXTURE_2D, colorBufferTex1[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color1.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex1[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color1.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex1[2]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color2.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex1[3]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color2.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        for (unsigned int i = 0; i < kMaxColorBuffer; i++)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, colorAttachments[i], GL_TEXTURE_2D,
                                   colorBufferTex1[i], 0);
        }
        glDrawBuffers(kMaxColorBuffer, &colorAttachments[0]);
        ASSERT_GL_NO_ERROR();

        GLint colorLocation = glGetUniformLocation(programFetch, "u_color");
        glUniform4fv(colorLocation, 1, colorRed);

        GLint positionLocation = glGetAttribLocation(programFetch, "a_position");
        // Render potentially with a glFramebufferFetchBarrierEXT() depending on the [non-]coherent
        // extension being used
        render(positionLocation, !mCoherentExtension);
        ASSERT_GL_NO_ERROR();

        glReadBuffer(colorAttachments[0]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);
        glReadBuffer(colorAttachments[1]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
        glReadBuffer(colorAttachments[2]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);
        glReadBuffer(colorAttachments[3]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);

        GLFramebuffer framebufferMRT2;
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferMRT2);
        GLTexture colorBufferTex2[kMaxColorBuffer];
        glBindTexture(GL_TEXTURE_2D, colorBufferTex2[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color2.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex2[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color2.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex2[2]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color1.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex2[3]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color1.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        for (unsigned int i = 0; i < kMaxColorBuffer; i++)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, colorAttachments[i], GL_TEXTURE_2D,
                                   colorBufferTex2[i], 0);
        }
        glDrawBuffers(kMaxColorBuffer, &colorAttachments[0]);
        ASSERT_GL_NO_ERROR();

        glUniform4fv(colorLocation, 1, colorRed);
        // Render potentially with a glFramebufferFetchBarrierEXT() depending on the [non-]coherent
        // extension being used
        render(positionLocation, !mCoherentExtension);
        ASSERT_GL_NO_ERROR();

        glReadBuffer(colorAttachments[0]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);
        glReadBuffer(colorAttachments[1]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
        glReadBuffer(colorAttachments[2]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);
        glReadBuffer(colorAttachments[3]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void DrawNonFetchDrawFetchWithDifferentProgramsTest(GLProgram &programNonFetch,
                                                        GLProgram &programFetch1,
                                                        GLProgram &programFetch2)
    {
        glUseProgram(programNonFetch);
        ASSERT_GL_NO_ERROR();
        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        std::vector<GLColor> greenColor(kViewportWidth * kViewportHeight, GLColor::green);
        GLTexture colorTex;
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, greenColor.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

        ASSERT_GL_NO_ERROR();

        float colorRed[4]           = {1.0f, 0.0f, 0.0f, 1.0f};
        GLint colorLocationNonFetch = glGetUniformLocation(programNonFetch, "u_color");
        glUniform4fv(colorLocationNonFetch, 1, colorRed);

        GLint positionLocationNonFetch = glGetAttribLocation(programNonFetch, "a_position");
        // Render without regard to glFramebufferFetchBarrierEXT()
        render(positionLocationNonFetch, GL_FALSE);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);

        glUseProgram(programFetch1);
        ASSERT_GL_NO_ERROR();

        GLFramebuffer framebufferMRT1;
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferMRT1);
        std::vector<GLColor> color1(kViewportWidth * kViewportHeight, GLColor::green);
        GLTexture colorBufferTex1[kMaxColorBuffer];
        GLenum colorAttachments[kMaxColorBuffer] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                                                    GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
        glBindTexture(GL_TEXTURE_2D, colorBufferTex1[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color1.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex1[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color1.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex1[2]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color1.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex1[3]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color1.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        for (unsigned int i = 0; i < kMaxColorBuffer; i++)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, colorAttachments[i], GL_TEXTURE_2D,
                                   colorBufferTex1[i], 0);
        }
        glDrawBuffers(kMaxColorBuffer, &colorAttachments[0]);
        ASSERT_GL_NO_ERROR();

        GLint colorLocation = glGetUniformLocation(programFetch1, "u_color");
        glUniform4fv(colorLocation, 1, colorRed);

        GLint positionLocation = glGetAttribLocation(programFetch1, "a_position");
        // Render potentially with a glFramebufferFetchBarrierEXT() depending on the [non-]coherent
        // extension being used
        render(positionLocation, !mCoherentExtension);
        ASSERT_GL_NO_ERROR();

        glReadBuffer(colorAttachments[0]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);
        glReadBuffer(colorAttachments[1]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
        glReadBuffer(colorAttachments[2]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);
        glReadBuffer(colorAttachments[3]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);

        glUseProgram(programFetch2);
        ASSERT_GL_NO_ERROR();

        glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        GLint colorLocation1 = glGetUniformLocation(programFetch2, "u_color");
        glUniform4fv(colorLocation1, 1, colorRed);

        GLint positionLocation1 = glGetAttribLocation(programFetch2, "a_position");
        // Render potentially with a glFramebufferFetchBarrierEXT() depending on the [non-]coherent
        // extension being used
        render(positionLocation1, !mCoherentExtension);
        ASSERT_GL_NO_ERROR();

        glReadBuffer(colorAttachments[0]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);
        glReadBuffer(colorAttachments[1]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
        glReadBuffer(colorAttachments[2]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
        glReadBuffer(colorAttachments[3]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void DrawFetchBlitDrawFetchTest(GLProgram &programNonFetch, GLProgram &programFetch)
    {
        glUseProgram(programFetch);
        ASSERT_GL_NO_ERROR();

        GLFramebuffer framebufferMRT1;
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferMRT1);
        std::vector<GLColor> color1(kViewportWidth * kViewportHeight, GLColor::green);
        std::vector<GLColor> color2(kViewportWidth * kViewportHeight, GLColor::blue);
        GLTexture colorBufferTex1[kMaxColorBuffer];
        GLenum colorAttachments[kMaxColorBuffer] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                                                    GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
        glBindTexture(GL_TEXTURE_2D, colorBufferTex1[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color1.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex1[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color1.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex1[2]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color2.data());
        glBindTexture(GL_TEXTURE_2D, colorBufferTex1[3]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color2.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        for (unsigned int i = 0; i < kMaxColorBuffer; i++)
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, colorAttachments[i], GL_TEXTURE_2D,
                                   colorBufferTex1[i], 0);
        }
        glDrawBuffers(kMaxColorBuffer, &colorAttachments[0]);
        ASSERT_GL_NO_ERROR();

        float colorRed[4]   = {1.0f, 0.0f, 0.0f, 1.0f};
        GLint colorLocation = glGetUniformLocation(programFetch, "u_color");
        glUniform4fv(colorLocation, 1, colorRed);

        GLint positionLocation = glGetAttribLocation(programFetch, "a_position");
        // Render potentially with a glFramebufferFetchBarrierEXT() depending on the [non-]coherent
        // extension being used
        render(positionLocation, !mCoherentExtension);
        ASSERT_GL_NO_ERROR();

        glReadBuffer(colorAttachments[0]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);
        glReadBuffer(colorAttachments[1]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
        glReadBuffer(colorAttachments[2]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);
        glReadBuffer(colorAttachments[3]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);

        GLFramebuffer framebufferColor;
        glBindFramebuffer(GL_FRAMEBUFFER, framebufferColor);

        GLTexture colorTex;
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, color2.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

        glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, framebufferColor);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, framebufferMRT1);

        glBlitFramebuffer(0, 0, kViewportWidth, kViewportHeight, 0, 0, kViewportWidth,
                          kViewportHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_FRAMEBUFFER, framebufferMRT1);
        glReadBuffer(colorAttachments[0]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::blue);
        glReadBuffer(colorAttachments[1]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::blue);
        glReadBuffer(colorAttachments[2]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::blue);
        glReadBuffer(colorAttachments[3]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::blue);

        float colorGreen[4] = {0.0f, 1.0f, 0.0f, 1.0f};
        glUniform4fv(colorLocation, 1, colorGreen);

        // Render potentially with a glFramebufferFetchBarrierEXT() depending on the [non-]coherent
        // extension being used
        render(positionLocation, !mCoherentExtension);
        ASSERT_GL_NO_ERROR();

        glReadBuffer(colorAttachments[0]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::cyan);
        glReadBuffer(colorAttachments[1]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::green);
        glReadBuffer(colorAttachments[2]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::cyan);
        glReadBuffer(colorAttachments[3]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::green);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void ProgramPipelineTest(const char *kVS, const char *kFS1, const char *kFS2)
    {
        GLProgram programVert, programNonFetch, programFetch;
        const char *sourceArray[3] = {kVS, kFS1, kFS2};

        GLShader vertShader(GL_VERTEX_SHADER);
        glShaderSource(vertShader, 1, &sourceArray[0], nullptr);
        glCompileShader(vertShader);
        glProgramParameteri(programVert, GL_PROGRAM_SEPARABLE, GL_TRUE);
        glAttachShader(programVert, vertShader);
        glLinkProgram(programVert);
        ASSERT_GL_NO_ERROR();

        GLShader fragShader1(GL_FRAGMENT_SHADER);
        glShaderSource(fragShader1, 1, &sourceArray[1], nullptr);
        glCompileShader(fragShader1);
        glProgramParameteri(programNonFetch, GL_PROGRAM_SEPARABLE, GL_TRUE);
        glAttachShader(programNonFetch, fragShader1);
        glLinkProgram(programNonFetch);
        ASSERT_GL_NO_ERROR();

        GLShader fragShader2(GL_FRAGMENT_SHADER);
        glShaderSource(fragShader2, 1, &sourceArray[2], nullptr);
        glCompileShader(fragShader2);
        glProgramParameteri(programFetch, GL_PROGRAM_SEPARABLE, GL_TRUE);
        glAttachShader(programFetch, fragShader2);
        glLinkProgram(programFetch);
        ASSERT_GL_NO_ERROR();

        GLProgramPipeline pipeline1, pipeline2, pipeline3, pipeline4;
        glUseProgramStages(pipeline1, GL_VERTEX_SHADER_BIT, programVert);
        glUseProgramStages(pipeline1, GL_FRAGMENT_SHADER_BIT, programNonFetch);
        glBindProgramPipeline(pipeline1);
        ASSERT_GL_NO_ERROR();

        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        std::vector<GLColor> greenColor(kViewportWidth * kViewportHeight, GLColor::green);
        GLTexture colorBufferTex;
        glBindTexture(GL_TEXTURE_2D, colorBufferTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, greenColor.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBufferTex,
                               0);
        ASSERT_GL_NO_ERROR();

        glActiveShaderProgram(pipeline1, programNonFetch);
        float colorRed[4]           = {1.0f, 0.0f, 0.0f, 1.0f};
        GLint colorLocationNonFetch = glGetUniformLocation(programNonFetch, "u_color");
        glUniform4fv(colorLocationNonFetch, 1, colorRed);
        ASSERT_GL_NO_ERROR();

        glActiveShaderProgram(pipeline1, programVert);
        GLint positionLocation = glGetAttribLocation(programVert, "a_position");
        // Render without regard to glFramebufferFetchBarrierEXT()
        render(positionLocation, GL_FALSE);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);

        glUseProgramStages(pipeline2, GL_VERTEX_SHADER_BIT, programVert);
        glUseProgramStages(pipeline2, GL_FRAGMENT_SHADER_BIT, programFetch);
        glBindProgramPipeline(pipeline2);
        ASSERT_GL_NO_ERROR();

        glActiveShaderProgram(pipeline2, programFetch);
        float colorGreen[4]      = {0.0f, 1.0f, 0.0f, 1.0f};
        GLint colorLocationFetch = glGetUniformLocation(programFetch, "u_color");
        glUniform4fv(colorLocationFetch, 1, colorGreen);

        // Render potentially with a glFramebufferFetchBarrierEXT() depending on the [non-]coherent
        // extension being used
        render(positionLocation, !mCoherentExtension);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);

        glUseProgramStages(pipeline3, GL_VERTEX_SHADER_BIT, programVert);
        glUseProgramStages(pipeline3, GL_FRAGMENT_SHADER_BIT, programNonFetch);
        glBindProgramPipeline(pipeline3);
        ASSERT_GL_NO_ERROR();

        glActiveShaderProgram(pipeline3, programNonFetch);
        colorLocationNonFetch = glGetUniformLocation(programNonFetch, "u_color");
        glUniform4fv(colorLocationNonFetch, 1, colorRed);

        ASSERT_GL_NO_ERROR();

        // Render without regard to glFramebufferFetchBarrierEXT()
        render(positionLocation, GL_FALSE);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);

        glUseProgramStages(pipeline4, GL_VERTEX_SHADER_BIT, programVert);
        glUseProgramStages(pipeline4, GL_FRAGMENT_SHADER_BIT, programFetch);
        glBindProgramPipeline(pipeline4);
        ASSERT_GL_NO_ERROR();

        glActiveShaderProgram(pipeline4, programFetch);
        colorLocationFetch = glGetUniformLocation(programFetch, "u_color");
        glUniform4fv(colorLocationFetch, 1, colorGreen);
        // Render potentially with a glFramebufferFetchBarrierEXT() depending on the [non-]coherent
        // extension being used
        render(positionLocation, !mCoherentExtension);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    bool mCoherentExtension;
    bool mARMExtension;
};

// Test coherent extension with inout qualifier
TEST_P(FramebufferFetchES31, BasicInout_Coherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    GLProgram program;
    program.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT));
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    BasicTest(program);
}

// Test non-coherent extension with inout qualifier
TEST_P(FramebufferFetchES31, BasicInout_NonCoherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));
    setWhichExtension(NON_COHERENT);

    GLProgram program;
    program.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT));
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    BasicTest(program);
}

// Test coherent extension with gl_LastFragData
TEST_P(FramebufferFetchES31, BasicLastFragData_Coherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    GLProgram program;
    program.makeRaster(k100VS, getFragmentShader(GLSL100));
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    BasicTest(program);
}

// Test non-coherent extension with gl_LastFragData
TEST_P(FramebufferFetchES31, BasicLastFragData_NonCoherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));
    setWhichExtension(NON_COHERENT);

    GLProgram program;
    program.makeRaster(k100VS, getFragmentShader(GLSL100));
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    BasicTest(program);
}

// Testing coherent extension with multiple render target
TEST_P(FramebufferFetchES31, MultipleRenderTarget_Coherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    GLProgram program;
    program.makeRaster(k310VS, getFragmentShader(GLSL310_4ATTACHMENT));
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    MultipleRenderTargetTest(program);
}

// Testing non-coherent extension with multiple render target
TEST_P(FramebufferFetchES31, MultipleRenderTarget_NonCoherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));
    setWhichExtension(NON_COHERENT);

    GLProgram program;
    program.makeRaster(k310VS, getFragmentShader(GLSL310_4ATTACHMENT));
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    MultipleRenderTargetTest(program);
}

// Testing non-coherent extension with multiple render target using inout array
TEST_P(FramebufferFetchES31, MultipleRenderTargetWithInoutArray_NonCoherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));
    setWhichExtension(NON_COHERENT);

    GLProgram program;
    program.makeRaster(k310VS, getFragmentShader(GLSL310_4ATTACHMENT));
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    MultipleRenderTargetTest(program);
}

// Testing coherent extension with multiple render target using inout array
TEST_P(FramebufferFetchES31, MultipleRenderTargetWithInoutArray_Coherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    GLProgram program;
    program.makeRaster(k310VS, getFragmentShader(GLSL310_4ATTACHMENT));
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    MultipleRenderTargetTest(program);
}

// Test coherent extension with multiple draw
TEST_P(FramebufferFetchES31, MultipleDraw_Coherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    GLProgram program;
    program.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT));
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    MultipleDrawTest(program);
}

// Test non-coherent extension with multiple draw
TEST_P(FramebufferFetchES31, MultipleDraw_NonCoherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));
    setWhichExtension(NON_COHERENT);

    GLProgram program;
    program.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT));
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    MultipleDrawTest(program);
}

// Testing coherent extension with the order of non-fetch program and fetch program
TEST_P(FramebufferFetchES31, DrawNonFetchDrawFetch_Coherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT));
    ASSERT_GL_NO_ERROR();

    DrawNonFetchDrawFetchTest(programNonFetch, programFetch);
}

// Testing non-coherent extension with the order of non-fetch program and fetch program
TEST_P(FramebufferFetchES31, DrawNonFetchDrawFetch_NonCoherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));
    setWhichExtension(NON_COHERENT);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT));
    ASSERT_GL_NO_ERROR();

    DrawNonFetchDrawFetchTest(programNonFetch, programFetch);
}

// Testing coherent extension with the order of fetch program and non-fetch program
TEST_P(FramebufferFetchES31, DrawFetchDrawNonFetch_Coherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT));
    ASSERT_GL_NO_ERROR();

    DrawFetchDrawNonFetchTest(programNonFetch, programFetch);
}

// Testing non-coherent extension with the order of fetch program and non-fetch program
TEST_P(FramebufferFetchES31, DrawFetchDrawNonFetch_NonCoherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));
    setWhichExtension(NON_COHERENT);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT));
    ASSERT_GL_NO_ERROR();

    DrawFetchDrawNonFetchTest(programNonFetch, programFetch);
}

// Testing coherent extension with framebuffer fetch read in combination with color attachment mask
TEST_P(FramebufferFetchES31, DrawNonFetchDrawFetchInStorageBuffer_Coherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    GLint maxFragmentShaderStorageBlocks = 0;
    glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &maxFragmentShaderStorageBlocks);
    ANGLE_SKIP_TEST_IF(maxFragmentShaderStorageBlocks == 0);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT_WITH_STORAGE_BUFFER));
    ASSERT_GL_NO_ERROR();

    DrawNonFetchDrawFetchInStorageBufferTest(programNonFetch, programFetch,
                                             StorageBufferTestPostFetchAction::Nothing);
}

// Testing non-coherent extension with framebuffer fetch read in combination with color attachment
// mask
TEST_P(FramebufferFetchES31, DrawNonFetchDrawFetchInStorageBuffer_NonCoherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));
    setWhichExtension(NON_COHERENT);

    GLint maxFragmentShaderStorageBlocks = 0;
    glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &maxFragmentShaderStorageBlocks);
    ANGLE_SKIP_TEST_IF(maxFragmentShaderStorageBlocks == 0);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT_WITH_STORAGE_BUFFER));
    ASSERT_GL_NO_ERROR();

    DrawNonFetchDrawFetchInStorageBufferTest(programNonFetch, programFetch,
                                             StorageBufferTestPostFetchAction::Nothing);
}

// Testing coherent extension with the order of non-fetch program and fetch program with
// different attachments
TEST_P(FramebufferFetchES31, DrawNonFetchDrawFetchWithDifferentAttachments_Coherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_4ATTACHMENT_DIFFERENT1));
    ASSERT_GL_NO_ERROR();

    DrawNonFetchDrawFetchWithDifferentAttachmentsTest(programNonFetch, programFetch);
}

// Testing coherent extension with framebuffer fetch read in combination with color attachment mask
// and clear
TEST_P(FramebufferFetchES31, DrawNonFetchDrawFetchInStorageBufferThenClear_Coherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    GLint maxFragmentShaderStorageBlocks = 0;
    glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &maxFragmentShaderStorageBlocks);
    ANGLE_SKIP_TEST_IF(maxFragmentShaderStorageBlocks == 0);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT_WITH_STORAGE_BUFFER));
    ASSERT_GL_NO_ERROR();

    DrawNonFetchDrawFetchInStorageBufferTest(programNonFetch, programFetch,
                                             StorageBufferTestPostFetchAction::Clear);
}

// Testing non-coherent extension with framebuffer fetch read in combination with color attachment
// mask and clear
TEST_P(FramebufferFetchES31, DrawNonFetchDrawFetchInStorageBufferThenClear_NonCoherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));
    setWhichExtension(NON_COHERENT);

    GLint maxFragmentShaderStorageBlocks = 0;
    glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &maxFragmentShaderStorageBlocks);
    ANGLE_SKIP_TEST_IF(maxFragmentShaderStorageBlocks == 0);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT_WITH_STORAGE_BUFFER));
    ASSERT_GL_NO_ERROR();

    DrawNonFetchDrawFetchInStorageBufferTest(programNonFetch, programFetch,
                                             StorageBufferTestPostFetchAction::Clear);
}

// Testing non-coherent extension with the order of non-fetch program and fetch program with
// different attachments
TEST_P(FramebufferFetchES31, DrawNonFetchDrawFetchWithDifferentAttachments_NonCoherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));
    setWhichExtension(NON_COHERENT);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_4ATTACHMENT_DIFFERENT1));
    ASSERT_GL_NO_ERROR();

    DrawNonFetchDrawFetchWithDifferentAttachmentsTest(programNonFetch, programFetch);
}

// Testing coherent extension with the order of non-fetch program and fetch with different
// programs
TEST_P(FramebufferFetchES31, DrawNonFetchDrawFetchWithDifferentPrograms_Coherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    GLProgram programNonFetch, programFetch1, programFetch2;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch1.makeRaster(k310VS, getFragmentShader(GLSL310_4ATTACHMENT_DIFFERENT1));
    programFetch2.makeRaster(k310VS, getFragmentShader(GLSL310_4ATTACHMENT_DIFFERENT2));
    ASSERT_GL_NO_ERROR();

    DrawNonFetchDrawFetchWithDifferentProgramsTest(programNonFetch, programFetch1, programFetch2);
}

// Testing non-coherent extension with the order of non-fetch program and fetch with different
// programs
TEST_P(FramebufferFetchES31, DrawNonFetchDrawFetchWithDifferentPrograms_NonCoherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));
    setWhichExtension(NON_COHERENT);

    GLProgram programNonFetch, programFetch1, programFetch2;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch1.makeRaster(k310VS, getFragmentShader(GLSL310_4ATTACHMENT_DIFFERENT1));
    programFetch2.makeRaster(k310VS, getFragmentShader(GLSL310_4ATTACHMENT_DIFFERENT2));
    ASSERT_GL_NO_ERROR();

    DrawNonFetchDrawFetchWithDifferentProgramsTest(programNonFetch, programFetch1, programFetch2);
}

// Testing coherent extension with the order of draw fetch, blit and draw fetch
TEST_P(FramebufferFetchES31, DrawFetchBlitDrawFetch_Coherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_4ATTACHMENT_DIFFERENT1));
    ASSERT_GL_NO_ERROR();

    DrawFetchBlitDrawFetchTest(programNonFetch, programFetch);
}

// Testing non-coherent extension with the order of draw fetch, blit and draw fetch
TEST_P(FramebufferFetchES31, DrawFetchBlitDrawFetch_NonCoherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));
    setWhichExtension(NON_COHERENT);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_4ATTACHMENT_DIFFERENT1));
    ASSERT_GL_NO_ERROR();

    DrawFetchBlitDrawFetchTest(programNonFetch, programFetch);
}

// Testing coherent extension with program pipeline
TEST_P(FramebufferFetchES31, ProgramPipeline_Coherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    ProgramPipelineTest(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT),
                        getFragmentShader(GLSL310_1ATTACHMENT));
}

// Testing non-coherent extension with program pipeline
TEST_P(FramebufferFetchES31, ProgramPipeline_NonCoherent)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));
    setWhichExtension(NON_COHERENT);

    ProgramPipelineTest(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT),
                        getFragmentShader(GLSL310_1ATTACHMENT));
}

// TODO: http://anglebug.com/5792
TEST_P(FramebufferFetchES31, DISABLED_UniformUsageCombinations)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));

    constexpr char kVS[] = R"(#version 310 es
in highp vec4 a_position;
out highp vec2 texCoord;

void main()
{
    gl_Position = a_position;
    texCoord = (a_position.xy * 0.5) + 0.5;
})";

    constexpr char kFS[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require

layout(binding=0, offset=0) uniform atomic_uint atDiff;
uniform sampler2D tex;

layout(noncoherent, location = 0) inout highp vec4 o_color[4];
in highp vec2 texCoord;

void main()
{
    highp vec4 texColor = texture(tex, texCoord);

    if (texColor != o_color[0])
    {
        atomicCounterIncrement(atDiff);
        o_color[0] = texColor;
    }
    else
    {
        if (atomicCounter(atDiff) > 0u)
        {
            atomicCounterDecrement(atDiff);
        }
    }

    if (texColor != o_color[1])
    {
        atomicCounterIncrement(atDiff);
        o_color[1] = texColor;
    }
    else
    {
        if (atomicCounter(atDiff) > 0u)
        {
            atomicCounterDecrement(atDiff);
        }
    }

    if (texColor != o_color[2])
    {
        atomicCounterIncrement(atDiff);
        o_color[2] = texColor;
    }
    else
    {
        if (atomicCounter(atDiff) > 0u)
        {
            atomicCounterDecrement(atDiff);
        }
    }

    if (texColor != o_color[3])
    {
        atomicCounterIncrement(atDiff);
        o_color[3] = texColor;
    }
    else
    {
        if (atomicCounter(atDiff) > 0u)
        {
            atomicCounterDecrement(atDiff);
        }
    }
})";

    GLProgram program;
    program.makeRaster(kVS, kFS);
    glUseProgram(program);

    ASSERT_GL_NO_ERROR();

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    std::vector<GLColor> color0(kViewportWidth * kViewportHeight, GLColor::cyan);
    std::vector<GLColor> color1(kViewportWidth * kViewportHeight, GLColor::green);
    std::vector<GLColor> color2(kViewportWidth * kViewportHeight, GLColor::blue);
    std::vector<GLColor> color3(kViewportWidth * kViewportHeight, GLColor::black);
    GLTexture colorBufferTex[kMaxColorBuffer];
    GLenum colorAttachments[kMaxColorBuffer] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                                                GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
    glBindTexture(GL_TEXTURE_2D, colorBufferTex[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, color0.data());
    glBindTexture(GL_TEXTURE_2D, colorBufferTex[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, color1.data());
    glBindTexture(GL_TEXTURE_2D, colorBufferTex[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, color2.data());
    glBindTexture(GL_TEXTURE_2D, colorBufferTex[3]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, color3.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    for (unsigned int i = 0; i < kMaxColorBuffer; i++)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, colorAttachments[i], GL_TEXTURE_2D,
                               colorBufferTex[i], 0);
    }
    glDrawBuffers(kMaxColorBuffer, &colorAttachments[0]);

    ASSERT_GL_NO_ERROR();

    GLBuffer atomicBuffer;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicBuffer);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);

    // Reset atomic counter buffer
    GLuint *userCounters;
    userCounters = static_cast<GLuint *>(glMapBufferRange(
        GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint),
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT));
    memset(userCounters, 0, sizeof(GLuint));
    glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomicBuffer);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

    float color[4]      = {1.0f, 0.0f, 0.0f, 1.0f};
    GLint colorLocation = glGetUniformLocation(program, "u_color");
    glUniform4fv(colorLocation, 1, color);

    GLint positionLocation = glGetAttribLocation(program, "a_position");
    render(positionLocation, GL_TRUE);

    ASSERT_GL_NO_ERROR();

    for (unsigned int i = 0; i < kMaxColorBuffer; i++)
    {
        glReadBuffer(colorAttachments[i]);
        EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::black);
    }

    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicBuffer);
    userCounters = static_cast<GLuint *>(
        glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), GL_MAP_READ_BIT));
    EXPECT_EQ(*userCounters, kViewportWidth * kViewportHeight * 2);
    glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Testing that binding the location value using GLES API is conflicted to the location value of the
// fragment inout.
TEST_P(FramebufferFetchES31, FixedUniformLocation)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));

    constexpr char kVS[] = R"(#version 310 es
in highp vec4 a_position;

void main (void)
{
    gl_Position = a_position;
})";

    constexpr char kFS[] = R"(#version 310 es
#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require
layout(noncoherent, location = 0) inout highp vec4 o_color;

layout(location = 0) uniform highp vec4 u_color;
void main (void)
{
    o_color += u_color;
})";

    GLProgram program;
    program.makeRaster(kVS, kFS);
    glUseProgram(program);

    ASSERT_GL_NO_ERROR();

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    std::vector<GLColor> greenColor(kViewportWidth * kViewportHeight, GLColor::green);
    GLTexture colorBufferTex;
    glBindTexture(GL_TEXTURE_2D, colorBufferTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, greenColor.data());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBufferTex, 0);

    ASSERT_GL_NO_ERROR();

    float color[4]      = {1.0f, 0.0f, 0.0f, 1.0f};
    GLint colorLocation = glGetUniformLocation(program, "u_color");
    glUniform4fv(colorLocation, 1, color);

    GLint positionLocation = glGetAttribLocation(program, "a_position");
    render(positionLocation, GL_TRUE);

    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Verify we can use inout with the default framebuffer
// http://anglebug.com/6893
TEST_P(FramebufferFetchES31, DefaultFramebufferTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));

    constexpr char kVS[] = R"(#version 300 es
in highp vec4 a_position;

void main (void)
{
    gl_Position = a_position;
})";

    constexpr char kFS[] = R"(#version 300 es
#extension GL_EXT_shader_framebuffer_fetch : require
layout(location = 0) inout highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color += u_color;
})";

    GLProgram program;
    program.makeRaster(kVS, kFS);
    glUseProgram(program);

    ASSERT_GL_NO_ERROR();

    // Ensure that we're rendering to the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Start with a clear buffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLint positionLocation = glGetAttribLocation(program, "a_position");
    GLint colorLocation    = glGetUniformLocation(program, "u_color");

    // Draw once with red
    glUniform4fv(colorLocation, 1, GLColor::red.toNormalizedVector().data());
    render(positionLocation, GL_FALSE);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
    ASSERT_GL_NO_ERROR();

    // Draw again with blue, adding it to the existing red, ending up with magenta
    glUniform4fv(colorLocation, 1, GLColor::blue.toNormalizedVector().data());
    render(positionLocation, GL_FALSE);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);
    ASSERT_GL_NO_ERROR();
}

// Verify we can render to the default framebuffer without fetch, then switch to a program
// that does fetch.
// http://anglebug.com/6893
TEST_P(FramebufferFetchES31, DefaultFramebufferMixedProgramsTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));

    constexpr char kVS[] = R"(#version 300 es
in highp vec4 a_position;

void main (void)
{
    gl_Position = a_position;
})";

    constexpr char kFS[] = R"(#version 300 es
layout(location = 0) out highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color = u_color;
})";

    constexpr char kFetchFS[] = R"(#version 300 es
#extension GL_EXT_shader_framebuffer_fetch : require
layout(location = 0) inout highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color += u_color;
})";

    // Create a program that simply writes out a color, no fetching
    GLProgram program;
    program.makeRaster(kVS, kFS);
    glUseProgram(program);

    ASSERT_GL_NO_ERROR();

    // Ensure that we're rendering to the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Start with a clear buffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLint positionLocation = glGetAttribLocation(program, "a_position");
    GLint colorLocation    = glGetUniformLocation(program, "u_color");

    // Draw once with red
    glUniform4fv(colorLocation, 1, GLColor::red.toNormalizedVector().data());
    render(positionLocation, false);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
    ASSERT_GL_NO_ERROR();

    // Create another program that DOES fetch from the framebuffer
    GLProgram program2;
    program2.makeRaster(kVS, kFetchFS);
    glUseProgram(program2);

    GLint positionLocation2 = glGetAttribLocation(program2, "a_position");
    GLint colorLocation2    = glGetUniformLocation(program2, "u_color");

    // Draw again with blue, fetching red from the framebuffer, adding it together
    glUniform4fv(colorLocation2, 1, GLColor::blue.toNormalizedVector().data());
    render(positionLocation2, false);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);
    ASSERT_GL_NO_ERROR();

    // Switch back to the non-fetched framebuffer, and render green
    glUseProgram(program);
    glUniform4fv(colorLocation, 1, GLColor::green.toNormalizedVector().data());
    render(positionLocation, false);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::green);
    ASSERT_GL_NO_ERROR();
}

// Verify we can render to a framebuffer with fetch, then switch to another framebuffer (without
// changing programs) http://anglebug.com/6893
TEST_P(FramebufferFetchES31, FramebufferMixedFetchTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));

    constexpr char kVS[] = R"(#version 300 es
in highp vec4 a_position;

void main (void)
{
    gl_Position = a_position;
})";

    constexpr char kFS[] = R"(#version 300 es
layout(location = 0) out highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color = u_color;
})";

    constexpr char kFetchFS[] = R"(#version 300 es
#extension GL_EXT_shader_framebuffer_fetch : require
layout(location = 0) inout highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color += u_color;
})";

    // Create a program that simply writes out a color, no fetching
    GLProgram program;
    program.makeRaster(kVS, kFS);
    GLint positionLocation = glGetAttribLocation(program, "a_position");
    GLint colorLocation    = glGetUniformLocation(program, "u_color");
    ASSERT_GL_NO_ERROR();

    // Create a program that DOES fetch from the framebuffer
    GLProgram fetchProgram;
    fetchProgram.makeRaster(kVS, kFetchFS);
    GLint fetchPositionLocation = glGetAttribLocation(fetchProgram, "a_position");
    GLint fetchColorLocation    = glGetUniformLocation(fetchProgram, "u_color");
    ASSERT_GL_NO_ERROR();

    // Create an empty framebuffer to use without fetch
    GLFramebuffer framebuffer1;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer1);
    std::vector<GLColor> clearColor(kViewportWidth * kViewportHeight, GLColor::transparentBlack);
    GLTexture colorBufferTex1;
    glBindTexture(GL_TEXTURE_2D, colorBufferTex1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, clearColor.data());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBufferTex1, 0);
    ASSERT_GL_NO_ERROR();

    // Draw to it with green, without using fetch, overwriting any contents
    glUseProgram(program);
    glUniform4fv(colorLocation, 1, GLColor::green.toNormalizedVector().data());
    render(positionLocation, false);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::green);
    ASSERT_GL_NO_ERROR();

    // Create another framebuffer to use WITH fetch, and initialize it with blue
    GLFramebuffer framebuffer2;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer2);
    std::vector<GLColor> blueColor(kViewportWidth * kViewportHeight, GLColor::blue);
    GLTexture colorBufferTex2;
    glBindTexture(GL_TEXTURE_2D, colorBufferTex2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, blueColor.data());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBufferTex2, 0);
    ASSERT_GL_NO_ERROR();

    // Draw once with red, fetching blue from the framebuffer, adding it together
    glUseProgram(fetchProgram);
    glUniform4fv(fetchColorLocation, 1, GLColor::red.toNormalizedVector().data());
    render(fetchPositionLocation, false);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);
    ASSERT_GL_NO_ERROR();

    // Now use the same program (WITH fetch) and render to the other framebuffer that was NOT used
    // with fetch. This verifies the framebuffer state is appropriately updated to match the
    // program.
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer1);
    render(fetchPositionLocation, false);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);
    ASSERT_GL_NO_ERROR();
}

// Verify that switching between single sampled framebuffer fetch and multi sampled framebuffer
// fetch works fine
TEST_P(FramebufferFetchES31, SingleSampledMultiSampledMixedTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch"));
    setWhichExtension(COHERENT);

    // Create a program that fetches from the framebuffer
    GLProgram fetchProgram;
    fetchProgram.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT));
    GLint positionLocation = glGetAttribLocation(fetchProgram, "a_position");
    GLint colorLocation    = glGetUniformLocation(fetchProgram, "u_color");
    ASSERT_GL_NO_ERROR();

    // Create two single sampled framebuffer
    GLRenderbuffer singleSampledRenderbuffer1;
    glBindRenderbuffer(GL_RENDERBUFFER, singleSampledRenderbuffer1);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kViewportWidth, kViewportHeight);
    GLFramebuffer singleSampledFramebuffer1;
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              singleSampledRenderbuffer1);

    GLRenderbuffer singleSampledRenderbuffer2;
    glBindRenderbuffer(GL_RENDERBUFFER, singleSampledRenderbuffer2);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kViewportWidth, kViewportHeight);
    GLFramebuffer singleSampledFramebuffer2;
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer2);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              singleSampledRenderbuffer2);

    // Create one multi sampled framebuffer
    GLRenderbuffer multiSampledRenderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, multiSampledRenderbuffer);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, kViewportWidth, kViewportHeight);
    GLFramebuffer multiSampledFramebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, multiSampledFramebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              multiSampledRenderbuffer);

    // Create a singlesampled render buffer for blit and read
    GLRenderbuffer resolvedRbo;
    glBindRenderbuffer(GL_RENDERBUFFER, resolvedRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kViewportWidth, kViewportHeight);
    GLFramebuffer resolvedFbo;
    glBindFramebuffer(GL_FRAMEBUFFER, resolvedFbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, resolvedRbo);

    // Clear three Framebuffers with different colors
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer1);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::black);

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer2);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::blue);

    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, multiSampledFramebuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolvedFbo);
    glBlitFramebuffer(0, 0, kViewportWidth, kViewportHeight, 0, 0, kViewportWidth, kViewportHeight,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolvedFbo);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::green);

    // Bind first single sampled framebuffer, draw once with red, fetching black from the
    // framebuffer
    glUseProgram(fetchProgram);
    glUniform4fv(colorLocation, 1, GLColor::red.toNormalizedVector().data());
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer1);
    render(positionLocation, false);
    ASSERT_GL_NO_ERROR();

    // Bind the multi sampled framebuffer, draw once with red, fetching green from the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, multiSampledFramebuffer);
    render(positionLocation, false);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolvedFbo);
    glBlitFramebuffer(0, 0, kViewportWidth, kViewportHeight, 0, 0, kViewportWidth, kViewportHeight,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolvedFbo);
    ASSERT_GL_NO_ERROR();

    // Bind the single sampled framebuffer, draw once with red, fetching blue from the framebuffer
    glUniform4fv(colorLocation, 1, GLColor::red.toNormalizedVector().data());
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer2);
    render(positionLocation, false);
    ASSERT_GL_NO_ERROR();

    // Verify the rendering result on all three framebuffers

    // Verify the last framebuffer being drawn: singleSampledFramebuffer2
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);

    // Verify the second last framebuffer being drawn: multisampledFramebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, multiSampledFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolvedFbo);
    glBlitFramebuffer(0, 0, kViewportWidth, kViewportHeight, 0, 0, kViewportWidth, kViewportHeight,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolvedFbo);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);

    // Verify the first framebuffer being drawn: singleSampledFramebuffer1
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer1);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
}

// Verify that calling glFramebufferFetchBarrierEXT without an open render pass is ok.
TEST_P(FramebufferFetchES31, BarrierBeforeDraw)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch") ||
                       !IsGLExtensionEnabled("GL_EXT_shader_framebuffer_fetch_non_coherent"));

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());

    glFramebufferFetchBarrierEXT();
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.0f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test ARM extension with gl_LastFragColorARM
TEST_P(FramebufferFetchES31, BasicLastFragData_ARM)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ARM_shader_framebuffer_fetch"));
    setWhichExtension(ARM);

    GLProgram program;
    program.makeRaster(k100VS, getFragmentShader(GLSL100));
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    BasicTest(program);
}

// Test ARM extension with multiple draw
TEST_P(FramebufferFetchES31, MultipleDraw_ARM)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ARM_shader_framebuffer_fetch"));
    setWhichExtension(ARM);

    GLProgram program;
    program.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT));
    glUseProgram(program);
    ASSERT_GL_NO_ERROR();

    MultipleDrawTest(program);
}

// Testing ARM extension with the order of non-fetch program and fetch program
TEST_P(FramebufferFetchES31, DrawNonFetchDrawFetch_ARM)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ARM_shader_framebuffer_fetch"));
    setWhichExtension(ARM);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT));
    ASSERT_GL_NO_ERROR();

    DrawNonFetchDrawFetchTest(programNonFetch, programFetch);
}

// Testing ARM extension with the order of fetch program and non-fetch program
TEST_P(FramebufferFetchES31, DrawFetchDrawNonFetch_ARM)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ARM_shader_framebuffer_fetch"));
    setWhichExtension(ARM);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT));
    ASSERT_GL_NO_ERROR();

    DrawFetchDrawNonFetchTest(programNonFetch, programFetch);
}

// Testing ARM extension with framebuffer fetch read in combination with color attachment mask
TEST_P(FramebufferFetchES31, DrawNonFetchDrawFetchInStorageBuffer_ARM)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ARM_shader_framebuffer_fetch"));
    setWhichExtension(ARM);

    GLint maxFragmentShaderStorageBlocks = 0;
    glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &maxFragmentShaderStorageBlocks);
    ANGLE_SKIP_TEST_IF(maxFragmentShaderStorageBlocks == 0);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT_WITH_STORAGE_BUFFER));
    ASSERT_GL_NO_ERROR();

    DrawNonFetchDrawFetchInStorageBufferTest(programNonFetch, programFetch,
                                             StorageBufferTestPostFetchAction::Nothing);
}

// Testing ARM extension with framebuffer fetch read in combination with color attachment mask
// and clear
TEST_P(FramebufferFetchES31, DrawNonFetchDrawFetchInStorageBufferThenClear_ARM)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ARM_shader_framebuffer_fetch"));
    setWhichExtension(ARM);

    GLint maxFragmentShaderStorageBlocks = 0;
    glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &maxFragmentShaderStorageBlocks);
    ANGLE_SKIP_TEST_IF(maxFragmentShaderStorageBlocks == 0);

    GLProgram programNonFetch, programFetch;
    programNonFetch.makeRaster(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT));
    programFetch.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT_WITH_STORAGE_BUFFER));
    ASSERT_GL_NO_ERROR();

    DrawNonFetchDrawFetchInStorageBufferTest(programNonFetch, programFetch,
                                             StorageBufferTestPostFetchAction::Clear);
}

// Testing ARM extension with program pipeline
TEST_P(FramebufferFetchES31, ProgramPipeline_ARM)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ARM_shader_framebuffer_fetch"));
    setWhichExtension(ARM);

    ProgramPipelineTest(k310VS, getFragmentShader(GLSL310_NO_FETCH_1ATTACHMENT),
                        getFragmentShader(GLSL310_1ATTACHMENT));
}

// Verify we can use the default framebuffer
// http://anglebug.com/6893
TEST_P(FramebufferFetchES31, DefaultFramebufferTest_ARM)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ARM_shader_framebuffer_fetch"));

    constexpr char kVS[] = R"(#version 300 es
in highp vec4 a_position;

void main (void)
{
    gl_Position = a_position;
})";

    constexpr char kFS[] = R"(#version 300 es
#extension GL_ARM_shader_framebuffer_fetch : require
layout(location = 0) out highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color = u_color + gl_LastFragColorARM;
})";

    GLProgram program;
    program.makeRaster(kVS, kFS);
    glUseProgram(program);

    ASSERT_GL_NO_ERROR();

    // Ensure that we're rendering to the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Start with a clear buffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLint positionLocation = glGetAttribLocation(program, "a_position");
    GLint colorLocation    = glGetUniformLocation(program, "u_color");

    // Draw once with red
    glUniform4fv(colorLocation, 1, GLColor::red.toNormalizedVector().data());
    render(positionLocation, GL_FALSE);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
    ASSERT_GL_NO_ERROR();

    // Draw again with blue, adding it to the existing red, ending up with magenta
    glUniform4fv(colorLocation, 1, GLColor::blue.toNormalizedVector().data());
    render(positionLocation, GL_FALSE);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);
    ASSERT_GL_NO_ERROR();
}

// Verify we can redeclare gl_LastFragColorARM with a new precision
// http://anglebug.com/6893
TEST_P(FramebufferFetchES31, NondefaultPrecisionTest_ARM)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ARM_shader_framebuffer_fetch"));

    constexpr char kVS[] = R"(#version 300 es
in highp vec4 a_position;

void main (void)
{
    gl_Position = a_position;
})";

    constexpr char kFS[] = R"(#version 300 es
#extension GL_ARM_shader_framebuffer_fetch : require
highp vec4 gl_LastFragColorARM;
layout(location = 0) out highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color = u_color + gl_LastFragColorARM;
})";

    GLProgram program;
    program.makeRaster(kVS, kFS);
    glUseProgram(program);

    ASSERT_GL_NO_ERROR();

    // Ensure that we're rendering to the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Start with a clear buffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLint positionLocation = glGetAttribLocation(program, "a_position");
    GLint colorLocation    = glGetUniformLocation(program, "u_color");

    // Draw once with red
    glUniform4fv(colorLocation, 1, GLColor::red.toNormalizedVector().data());
    render(positionLocation, GL_FALSE);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
    ASSERT_GL_NO_ERROR();

    // Draw again with blue, adding it to the existing red, ending up with magenta
    glUniform4fv(colorLocation, 1, GLColor::blue.toNormalizedVector().data());
    render(positionLocation, GL_FALSE);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);
    ASSERT_GL_NO_ERROR();
}

// Verify we can render to the default framebuffer without fetch, then switch to a program
// that does fetch.
// http://anglebug.com/6893
TEST_P(FramebufferFetchES31, DefaultFramebufferMixedProgramsTest_ARM)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ARM_shader_framebuffer_fetch"));

    constexpr char kVS[] = R"(#version 300 es
in highp vec4 a_position;

void main (void)
{
    gl_Position = a_position;
})";

    constexpr char kFS[] = R"(#version 300 es
layout(location = 0) out highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color = u_color;
})";

    constexpr char kFetchFS[] = R"(#version 300 es
#extension GL_ARM_shader_framebuffer_fetch : require
layout(location = 0) out highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color = u_color + gl_LastFragColorARM;
})";

    // Create a program that simply writes out a color, no fetching
    GLProgram program;
    program.makeRaster(kVS, kFS);
    glUseProgram(program);

    ASSERT_GL_NO_ERROR();

    // Ensure that we're rendering to the default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Start with a clear buffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLint positionLocation = glGetAttribLocation(program, "a_position");
    GLint colorLocation    = glGetUniformLocation(program, "u_color");

    // Draw once with red
    glUniform4fv(colorLocation, 1, GLColor::red.toNormalizedVector().data());
    render(positionLocation, false);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
    ASSERT_GL_NO_ERROR();

    // Create another program that DOES fetch from the framebuffer
    GLProgram program2;
    program2.makeRaster(kVS, kFetchFS);
    glUseProgram(program2);

    GLint positionLocation2 = glGetAttribLocation(program2, "a_position");
    GLint colorLocation2    = glGetUniformLocation(program2, "u_color");

    // Draw again with blue, fetching red from the framebuffer, adding it together
    glUniform4fv(colorLocation2, 1, GLColor::blue.toNormalizedVector().data());
    render(positionLocation2, false);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);
    ASSERT_GL_NO_ERROR();

    // Switch back to the non-fetched framebuffer, and render green
    glUseProgram(program);
    glUniform4fv(colorLocation, 1, GLColor::green.toNormalizedVector().data());
    render(positionLocation, false);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::green);
    ASSERT_GL_NO_ERROR();
}

// Verify we can render to a framebuffer with fetch, then switch to another framebuffer (without
// changing programs) http://anglebug.com/6893
TEST_P(FramebufferFetchES31, FramebufferMixedFetchTest_ARM)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ARM_shader_framebuffer_fetch"));

    constexpr char kVS[] = R"(#version 300 es
in highp vec4 a_position;

void main (void)
{
    gl_Position = a_position;
})";

    constexpr char kFS[] = R"(#version 300 es
layout(location = 0) out highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color = u_color;
})";

    constexpr char kFetchFS[] = R"(#version 300 es
#extension GL_ARM_shader_framebuffer_fetch : require
layout(location = 0) out highp vec4 o_color;

uniform highp vec4 u_color;
void main (void)
{
    o_color = u_color + gl_LastFragColorARM;
})";

    // Create a program that simply writes out a color, no fetching
    GLProgram program;
    program.makeRaster(kVS, kFS);
    GLint positionLocation = glGetAttribLocation(program, "a_position");
    GLint colorLocation    = glGetUniformLocation(program, "u_color");
    ASSERT_GL_NO_ERROR();

    // Create a program that DOES fetch from the framebuffer
    GLProgram fetchProgram;
    fetchProgram.makeRaster(kVS, kFetchFS);
    GLint fetchPositionLocation = glGetAttribLocation(fetchProgram, "a_position");
    GLint fetchColorLocation    = glGetUniformLocation(fetchProgram, "u_color");
    ASSERT_GL_NO_ERROR();

    // Create an empty framebuffer to use without fetch
    GLFramebuffer framebuffer1;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer1);
    std::vector<GLColor> clearColor(kViewportWidth * kViewportHeight, GLColor::transparentBlack);
    GLTexture colorBufferTex1;
    glBindTexture(GL_TEXTURE_2D, colorBufferTex1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, clearColor.data());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBufferTex1, 0);
    ASSERT_GL_NO_ERROR();

    // Draw to it with green, without using fetch, overwriting any contents
    glUseProgram(program);
    glUniform4fv(colorLocation, 1, GLColor::green.toNormalizedVector().data());
    render(positionLocation, false);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::green);
    ASSERT_GL_NO_ERROR();

    // Create another framebuffer to use WITH fetch, and initialize it with blue
    GLFramebuffer framebuffer2;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer2);
    std::vector<GLColor> blueColor(kViewportWidth * kViewportHeight, GLColor::blue);
    GLTexture colorBufferTex2;
    glBindTexture(GL_TEXTURE_2D, colorBufferTex2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kViewportWidth, kViewportHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, blueColor.data());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBufferTex2, 0);
    ASSERT_GL_NO_ERROR();

    // Draw once with red, fetching blue from the framebuffer, adding it together
    glUseProgram(fetchProgram);
    glUniform4fv(fetchColorLocation, 1, GLColor::red.toNormalizedVector().data());
    render(fetchPositionLocation, false);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);
    ASSERT_GL_NO_ERROR();

    // Now use the same program (WITH fetch) and render to the other framebuffer that was NOT used
    // with fetch. This verifies the framebuffer state is appropriately updated to match the
    // program.
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer1);
    render(fetchPositionLocation, false);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);
    ASSERT_GL_NO_ERROR();
}

// Verify that switching between single sampled framebuffer fetch and multi sampled framebuffer
// fetch works fine
TEST_P(FramebufferFetchES31, SingleSampledMultiSampledMixedTest_ARM)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ARM_shader_framebuffer_fetch"));
    setWhichExtension(ARM);

    // Create a program that fetches from the framebuffer
    GLProgram fetchProgram;
    fetchProgram.makeRaster(k310VS, getFragmentShader(GLSL310_1ATTACHMENT));
    GLint positionLocation = glGetAttribLocation(fetchProgram, "a_position");
    GLint colorLocation    = glGetUniformLocation(fetchProgram, "u_color");
    ASSERT_GL_NO_ERROR();

    // Create two single sampled framebuffer
    GLRenderbuffer singleSampledRenderbuffer1;
    glBindRenderbuffer(GL_RENDERBUFFER, singleSampledRenderbuffer1);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kViewportWidth, kViewportHeight);
    GLFramebuffer singleSampledFramebuffer1;
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer1);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              singleSampledRenderbuffer1);

    GLRenderbuffer singleSampledRenderbuffer2;
    glBindRenderbuffer(GL_RENDERBUFFER, singleSampledRenderbuffer2);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kViewportWidth, kViewportHeight);
    GLFramebuffer singleSampledFramebuffer2;
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer2);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              singleSampledRenderbuffer2);

    // Create one multi sampled framebuffer
    GLRenderbuffer multiSampledRenderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, multiSampledRenderbuffer);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, kViewportWidth, kViewportHeight);
    GLFramebuffer multiSampledFramebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, multiSampledFramebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              multiSampledRenderbuffer);

    // Create a singlesampled render buffer for blit and read
    GLRenderbuffer resolvedRbo;
    glBindRenderbuffer(GL_RENDERBUFFER, resolvedRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kViewportWidth, kViewportHeight);
    GLFramebuffer resolvedFbo;
    glBindFramebuffer(GL_FRAMEBUFFER, resolvedFbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, resolvedRbo);

    // Clear three Framebuffers with different colors
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer1);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::black);

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer2);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::blue);

    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glBindFramebuffer(GL_FRAMEBUFFER, multiSampledFramebuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolvedFbo);
    glBlitFramebuffer(0, 0, kViewportWidth, kViewportHeight, 0, 0, kViewportWidth, kViewportHeight,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolvedFbo);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::green);

    // Bind first single sampled framebuffer, draw once with red, fetching black from the
    // framebuffer
    glUseProgram(fetchProgram);
    glUniform4fv(colorLocation, 1, GLColor::red.toNormalizedVector().data());
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer1);
    render(positionLocation, false);
    ASSERT_GL_NO_ERROR();

    // Bind the multi sampled framebuffer, draw once with red, fetching green from the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, multiSampledFramebuffer);
    render(positionLocation, false);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolvedFbo);
    glBlitFramebuffer(0, 0, kViewportWidth, kViewportHeight, 0, 0, kViewportWidth, kViewportHeight,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolvedFbo);
    ASSERT_GL_NO_ERROR();

    // Bind the single sampled framebuffer, draw once with red, fetching blue from the framebuffer
    glUniform4fv(colorLocation, 1, GLColor::red.toNormalizedVector().data());
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer2);
    render(positionLocation, false);
    ASSERT_GL_NO_ERROR();

    // Verify the rendering result on all three framebuffers

    // Verify the last framebuffer being drawn: singleSampledFramebuffer2
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::magenta);

    // Verify the second last framebuffer being drawn: multisampledFramebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, multiSampledFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolvedFbo);
    glBlitFramebuffer(0, 0, kViewportWidth, kViewportHeight, 0, 0, kViewportWidth, kViewportHeight,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolvedFbo);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::yellow);

    // Verify the first framebuffer being drawn: singleSampledFramebuffer1
    glBindFramebuffer(GL_FRAMEBUFFER, singleSampledFramebuffer1);
    EXPECT_PIXEL_COLOR_EQ(kViewportWidth / 2, kViewportHeight / 2, GLColor::red);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(FramebufferFetchES31);
ANGLE_INSTANTIATE_TEST_ES31(FramebufferFetchES31);
}  // namespace angle
