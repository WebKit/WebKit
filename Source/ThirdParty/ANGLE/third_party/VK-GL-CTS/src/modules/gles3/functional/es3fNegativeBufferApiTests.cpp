/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Negative Buffer API tests.
 *//*--------------------------------------------------------------------*/

#include "es3fNegativeBufferApiTests.hpp"
#include "es3fApiCase.hpp"
#include "gluContextInfo.hpp"

#include "glwDefs.hpp"
#include "glwEnums.hpp"

using namespace glw; // GL types

namespace deqp
{
namespace gles3
{
namespace Functional
{

using tcu::TestLog;

NegativeBufferApiTests::NegativeBufferApiTests(Context &context)
    : TestCaseGroup(context, "buffer", "Negative Buffer API Cases")
{
}

NegativeBufferApiTests::~NegativeBufferApiTests(void)
{
}

void NegativeBufferApiTests::init(void)
{
    // Buffers

    ES3F_ADD_API_CASE(bind_buffer, "Invalid glBindBuffer() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not one of the allowable values.");
        glBindBuffer(-1, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(delete_buffers, "Invalid glDeleteBuffers() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glDeleteBuffers(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(gen_buffers, "Invalid glGenBuffers() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glGenBuffers(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(buffer_data, "Invalid glBufferData() usage", {
        GLuint buffer;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target is not GL_ARRAY_BUFFER or GL_ELEMENT_ARRAY_BUFFER.");
        glBufferData(-1, 0, NULL, GL_STREAM_DRAW);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if usage is not GL_STREAM_DRAW, GL_STATIC_DRAW, or GL_DYNAMIC_DRAW.");
        glBufferData(GL_ARRAY_BUFFER, 0, NULL, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if size is negative.");
        glBufferData(GL_ARRAY_BUFFER, -1, NULL, GL_STREAM_DRAW);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the reserved buffer object name 0 is bound to target.");
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STREAM_DRAW);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buffer);
    });
    ES3F_ADD_API_CASE(buffer_sub_data, "Invalid glBufferSubData() usage", {
        GLuint buffer;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER, 10, 0, GL_STREAM_DRAW);

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target is not GL_ARRAY_BUFFER or GL_ELEMENT_ARRAY_BUFFER.");
        glBufferSubData(-1, 1, 1, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the reserved buffer object name 0 is bound to target.");
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBufferSubData(GL_ARRAY_BUFFER, 1, 1, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_OPERATION is generated if the buffer object being updated is mapped.");
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glMapBufferRange(GL_ARRAY_BUFFER, 0, 5, GL_MAP_READ_BIT);
        expectError(GL_NO_ERROR);
        glBufferSubData(GL_ARRAY_BUFFER, 1, 1, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buffer);
    });
    ES3F_ADD_API_CASE(buffer_sub_data_size_offset, "Invalid glBufferSubData() usage", {
        GLuint buffer;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER, 10, 0, GL_STREAM_DRAW);

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if offset or size is negative, or if together they define a region of "
                "memory that extends beyond the buffer object's allocated data store.");
        glBufferSubData(GL_ARRAY_BUFFER, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glBufferSubData(GL_ARRAY_BUFFER, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glBufferSubData(GL_ARRAY_BUFFER, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glBufferSubData(GL_ARRAY_BUFFER, 15, 1, 0);
        expectError(GL_INVALID_VALUE);
        glBufferSubData(GL_ARRAY_BUFFER, 1, 15, 0);
        expectError(GL_INVALID_VALUE);
        glBufferSubData(GL_ARRAY_BUFFER, 8, 8, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buffer);
    });
    ES3F_ADD_API_CASE(clear, "Invalid glClear() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if any bit other than the three defined bits is set in mask.");
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        expectError(GL_NO_ERROR);
        glClear(0x00000200);
        expectError(GL_INVALID_VALUE);
        glClear(0x00001000);
        expectError(GL_INVALID_VALUE);
        glClear(0x00000010);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(read_pixels, "Invalid glReadPixels() usage", {
        std::vector<GLubyte> ubyteData(4);

        /* GL_LUMINANCE_ALPHA not supported by GL Core / ARB_ES3_compatibility. */
        if (!glu::isContextTypeGLCore(m_context.getRenderContext().getType()))
        {
            m_log << TestLog::Section(
                "", "GL_INVALID_OPERATION is generated if the combination of format and type is unsupported.");
            glReadPixels(0, 0, 1, 1, GL_LUMINANCE_ALPHA, GL_UNSIGNED_SHORT_4_4_4_4, &ubyteData[0]);
            expectError(GL_INVALID_OPERATION);
            m_log << TestLog::EndSection;
        }

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if either width or height is negative.");
        glReadPixels(0, 0, -1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &ubyteData[0]);
        expectError(GL_INVALID_VALUE);
        glReadPixels(0, 0, 1, -1, GL_RGBA, GL_UNSIGNED_BYTE, &ubyteData[0]);
        expectError(GL_INVALID_VALUE);
        glReadPixels(0, 0, -1, -1, GL_RGBA, GL_UNSIGNED_BYTE, &ubyteData[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                      "framebuffer is not framebuffer complete.");
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &ubyteData[0]);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteFramebuffers(1, &fbo);
    });
    ES3F_ADD_API_CASE(read_pixels_format_mismatch, "Invalid glReadPixels() usage", {
        std::vector<GLubyte> ubyteData(4);
        std::vector<GLushort> ushortData(4);

        m_log << TestLog::Section(
            "", "Unsupported combinations of format and type will generate an INVALID_OPERATION error.");
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, &ushortData[0]);
        expectError(GL_INVALID_OPERATION);
        glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, &ushortData[0]);
        expectError(GL_INVALID_OPERATION);
        glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, &ushortData[0]);
        expectError(GL_INVALID_OPERATION);

        /* GL_ALPHA not supported by GL Core / ARB_ES3_compatibility. */
        if (!glu::isContextTypeGLCore(m_context.getRenderContext().getType()))
        {
            glReadPixels(0, 0, 1, 1, GL_ALPHA, GL_UNSIGNED_SHORT_5_6_5, &ushortData[0]);
            expectError(GL_INVALID_OPERATION);
            glReadPixels(0, 0, 1, 1, GL_ALPHA, GL_UNSIGNED_SHORT_4_4_4_4, &ushortData[0]);
            expectError(GL_INVALID_OPERATION);
            glReadPixels(0, 0, 1, 1, GL_ALPHA, GL_UNSIGNED_SHORT_5_5_5_1, &ushortData[0]);
            expectError(GL_INVALID_OPERATION);
        }

        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_RGBA/GL_UNSIGNED_BYTE is always accepted and the other acceptable pair can be discovered by "
                "querying GL_IMPLEMENTATION_COLOR_READ_FORMAT and GL_IMPLEMENTATION_COLOR_READ_TYPE.");
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &ubyteData[0]);
        expectError(GL_NO_ERROR);
        GLint readFormat;
        GLint readType;
        glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &readFormat);
        glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &readType);
        glReadPixels(0, 0, 1, 1, readFormat, readType, &ubyteData[0]);
        expectError(GL_NO_ERROR);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(read_pixels_fbo_format_mismatch, "Invalid glReadPixels() usage", {
        std::vector<GLubyte> ubyteData(4);
        std::vector<float> floatData(4);
        uint32_t fbo;
        uint32_t texture;
        bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if currently bound framebuffer format is "
                                      "incompatible with format and type.");

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        expectError(GL_NO_ERROR);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, &floatData[0]);
        expectError(isES ? GL_INVALID_OPERATION : GL_NO_ERROR);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, 32, 32, 0, GL_RGBA_INTEGER, GL_INT, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        expectError(GL_NO_ERROR);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, &floatData[0]);
        expectError(GL_INVALID_OPERATION);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 32, 32, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        expectError(GL_NO_ERROR);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, &floatData[0]);
        expectError(GL_INVALID_OPERATION);

        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if GL_READ_FRAMEBUFFER_BINDING is non-zero, the read framebuffer is "
                "complete, and the value of GL_SAMPLE_BUFFERS for the read framebuffer is greater than zero.");

        int binding = -1;
        int sampleBuffers;
        uint32_t rbo;

        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, 32, 32);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);

        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &binding);
        m_log << TestLog::Message << "// GL_READ_FRAMEBUFFER_BINDING: " << binding << TestLog::EndMessage;
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glGetIntegerv(GL_SAMPLE_BUFFERS, &sampleBuffers);
        m_log << TestLog::Message << "// GL_SAMPLE_BUFFERS: " << sampleBuffers << TestLog::EndMessage;
        expectError(GL_NO_ERROR);

        if (binding == 0 || sampleBuffers <= 0)
        {
            m_log << TestLog::Message
                  << "// ERROR: expected GL_READ_FRAMEBUFFER_BINDING to be non-zero and GL_SAMPLE_BUFFERS to be "
                     "greater than zero"
                  << TestLog::EndMessage;
            if (m_testCtx.getTestResult() == QP_TEST_RESULT_PASS)
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got invalid value");
        }
        else
        {
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &ubyteData[0]);
            expectError(GL_INVALID_OPERATION);
        }

        m_log << TestLog::EndSection;

        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &texture);
        glDeleteRenderbuffers(1, &rbo);
    });
    ES3F_ADD_API_CASE(bind_buffer_range, "Invalid glBindBufferRange() usage", {
        uint32_t bufU;
        glGenBuffers(1, &bufU);
        glBindBuffer(GL_UNIFORM_BUFFER, bufU);
        glBufferData(GL_UNIFORM_BUFFER, 16, NULL, GL_STREAM_DRAW);

        uint32_t bufTF;
        glGenBuffers(1, &bufTF);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, bufTF);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 16, NULL, GL_STREAM_DRAW);

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target is not GL_TRANSFORM_FEEDBACK_BUFFER or GL_UNIFORM_BUFFER.");
        glBindBufferRange(GL_ARRAY_BUFFER, 0, bufU, 0, 4);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if target is GL_TRANSFORM_FEEDBACK_BUFFER and "
                                      "index is greater than or equal to GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS.");
        int maxTFSize;
        glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxTFSize);
        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, maxTFSize, bufTF, 0, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if target is GL_UNIFORM_BUFFER and index is "
                                      "greater than or equal to GL_MAX_UNIFORM_BUFFER_BINDINGS.");
        int maxUSize;
        glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxUSize);
        glBindBufferRange(GL_UNIFORM_BUFFER, maxUSize, bufU, 0, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if size is less than or equal to zero.");
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, bufU, 0, -1);
        expectError(GL_INVALID_VALUE);
        glBindBufferRange(GL_UNIFORM_BUFFER, 0, bufU, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if target is GL_TRANSFORM_FEEDBACK_BUFFER and "
                                      "size or offset are not multiples of 4.");
        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, bufTF, 4, 5);
        expectError(GL_INVALID_VALUE);
        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, bufTF, 5, 4);
        expectError(GL_INVALID_VALUE);
        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, bufTF, 5, 7);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if target is GL_UNIFORM_BUFFER and offset is not "
                                      "a multiple of GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT.");
        int alignment;
        glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);

        if (alignment > 1)
        {
            glBindBufferRange(GL_UNIFORM_BUFFER, 0, bufU, alignment + 1, 4);
            expectError(GL_INVALID_VALUE);
        }
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &bufU);
        glDeleteBuffers(1, &bufTF);
    });
    ES3F_ADD_API_CASE(bind_buffer_base, "Invalid glBindBufferBase() usage", {
        uint32_t bufU;
        glGenBuffers(1, &bufU);
        glBindBuffer(GL_UNIFORM_BUFFER, bufU);
        glBufferData(GL_UNIFORM_BUFFER, 16, NULL, GL_STREAM_DRAW);

        uint32_t bufTF;
        glGenBuffers(1, &bufTF);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, bufTF);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 16, NULL, GL_STREAM_DRAW);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target is not GL_TRANSFORM_FEEDBACK_BUFFER or GL_UNIFORM_BUFFER.");
        glBindBufferBase(-1, 0, bufU);
        expectError(GL_INVALID_ENUM);
        glBindBufferBase(GL_ARRAY_BUFFER, 0, bufU);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if target is GL_UNIFORM_BUFFER and index is "
                                      "greater than or equal to GL_MAX_UNIFORM_BUFFER_BINDINGS.");
        int maxUSize;
        glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxUSize);
        glBindBufferBase(GL_UNIFORM_BUFFER, maxUSize, bufU);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if target is GL_TRANSFORM_FEEDBACK_BUFFER andindex is "
                                  "greater than or equal to GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS.");
        int maxTFSize;
        glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxTFSize);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, maxTFSize, bufTF);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &bufU);
        glDeleteBuffers(1, &bufTF);
    });
    ES3F_ADD_API_CASE(clear_bufferiv, "Invalid glClearBufferiv() usage", {
        std::vector<int> data(32 * 32);
        uint32_t fbo;
        uint32_t texture;

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, 32, 32, 0, GL_RGBA_INTEGER, GL_INT, NULL);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if buffer is not an accepted value.");
        glClearBufferiv(-1, 0, &data[0]);
        expectError(GL_INVALID_ENUM);
        glClearBufferiv(GL_FRAMEBUFFER, 0, &data[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if buffer is GL_COLOR, GL_FRONT, GL_BACK, GL_LEFT, GL_RIGHT, or "
                "GL_FRONT_AND_BACK and drawBuffer is greater than or equal to GL_MAX_DRAW_BUFFERS.");
        int maxDrawBuffers;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
        glClearBufferiv(GL_COLOR, maxDrawBuffers, &data[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if buffer is GL_DEPTH or GL_DEPTH_STENCIL.");
        glClearBufferiv(GL_DEPTH, 1, &data[0]);
        expectError(GL_INVALID_ENUM);
        glClearBufferiv(GL_DEPTH_STENCIL, 1, &data[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if buffer is GL_STENCIL and drawBuffer is not zero.");
        glClearBufferiv(GL_STENCIL, 1, &data[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(clear_bufferuiv, "Invalid glClearBufferuiv() usage", {
        std::vector<uint32_t> data(32 * 32);
        uint32_t fbo;
        uint32_t texture;

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 32, 32, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, NULL);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if buffer is not an accepted value.");
        glClearBufferuiv(-1, 0, &data[0]);
        expectError(GL_INVALID_ENUM);
        glClearBufferuiv(GL_FRAMEBUFFER, 0, &data[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if buffer is GL_COLOR, GL_FRONT, GL_BACK, GL_LEFT, GL_RIGHT, or "
                "GL_FRONT_AND_BACK and drawBuffer is greater than or equal to GL_MAX_DRAW_BUFFERS.");
        int maxDrawBuffers;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
        glClearBufferuiv(GL_COLOR, maxDrawBuffers, &data[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if buffer is GL_DEPTH, GL_STENCIL or GL_DEPTH_STENCIL.");
        glClearBufferuiv(GL_DEPTH, 1, &data[0]);
        expectError(GL_INVALID_ENUM);
        glClearBufferuiv(GL_STENCIL, 1, &data[0]);
        expectError(GL_INVALID_ENUM);
        glClearBufferuiv(GL_DEPTH_STENCIL, 1, &data[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(clear_bufferfv, "Invalid glClearBufferfv() usage", {
        std::vector<float> data(32 * 32);
        uint32_t fbo;
        uint32_t texture;

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 32, 32, 0, GL_RGBA, GL_FLOAT, NULL);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if buffer is not an accepted value.");
        glClearBufferfv(-1, 0, &data[0]);
        expectError(GL_INVALID_ENUM);
        glClearBufferfv(GL_FRAMEBUFFER, 0, &data[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if buffer is GL_COLOR, GL_FRONT, GL_BACK, GL_LEFT, GL_RIGHT, or "
                "GL_FRONT_AND_BACK and drawBuffer is greater than or equal to GL_MAX_DRAW_BUFFERS.");
        int maxDrawBuffers;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
        glClearBufferfv(GL_COLOR, maxDrawBuffers, &data[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if buffer is GL_STENCIL or GL_DEPTH_STENCIL.");
        glClearBufferfv(GL_STENCIL, 1, &data[0]);
        expectError(GL_INVALID_ENUM);
        glClearBufferfv(GL_DEPTH_STENCIL, 1, &data[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if buffer is GL_DEPTH and drawBuffer is not zero.");
        glClearBufferfv(GL_DEPTH, 1, &data[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(clear_bufferfi, "Invalid glClearBufferfi() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if buffer is not an accepted value.");
        glClearBufferfi(-1, 0, 1.0f, 1);
        expectError(GL_INVALID_ENUM);
        glClearBufferfi(GL_FRAMEBUFFER, 0, 1.0f, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if buffer is not GL_DEPTH_STENCIL.");
        glClearBufferfi(GL_DEPTH, 0, 1.0f, 1);
        expectError(GL_INVALID_ENUM);
        glClearBufferfi(GL_STENCIL, 0, 1.0f, 1);
        expectError(GL_INVALID_ENUM);
        glClearBufferfi(GL_COLOR, 0, 1.0f, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if buffer is GL_DEPTH_STENCIL and drawBuffer is not zero.");
        glClearBufferfi(GL_DEPTH_STENCIL, 1, 1.0f, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(copy_buffer_sub_data, "Invalid glCopyBufferSubData() usage", {
        uint32_t buf[2];
        std::vector<float> data(32 * 32);

        glGenBuffers(2, buf);
        glBindBuffer(GL_COPY_READ_BUFFER, buf[0]);
        glBufferData(GL_COPY_READ_BUFFER, 32, &data[0], GL_DYNAMIC_COPY);
        glBindBuffer(GL_COPY_WRITE_BUFFER, buf[1]);
        glBufferData(GL_COPY_WRITE_BUFFER, 32, &data[0], GL_DYNAMIC_COPY);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if any of readoffset, writeoffset or size is negative.");
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, -4);
        expectError(GL_INVALID_VALUE);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, -1, 0, 4);
        expectError(GL_INVALID_VALUE);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, -1, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if readoffset + size exceeds the size of the "
                                      "buffer object bound to readtarget.");
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, 36);
        expectError(GL_INVALID_VALUE);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 24, 0, 16);
        expectError(GL_INVALID_VALUE);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 36, 0, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if writeoffset + size exceeds the size of the "
                                      "buffer object bound to writetarget.");
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, 36);
        expectError(GL_INVALID_VALUE);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 24, 16);
        expectError(GL_INVALID_VALUE);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 36, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if the same buffer object is bound to both readtarget and writetarget "
                "and the ranges [readoffset, readoffset + size) and [writeoffset, writeoffset + size) overlap.");
        glBindBuffer(GL_COPY_WRITE_BUFFER, buf[0]);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 16, 4);
        expectError(GL_NO_ERROR);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, 4);
        expectError(GL_INVALID_VALUE);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 16, 18);
        expectError(GL_INVALID_VALUE);
        glBindBuffer(GL_COPY_WRITE_BUFFER, buf[1]);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_OPERATION is generated if zero is bound to readtarget or writetarget.");
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, 16);
        expectError(GL_INVALID_OPERATION);

        glBindBuffer(GL_COPY_READ_BUFFER, buf[0]);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, 16);
        expectError(GL_INVALID_OPERATION);

        glBindBuffer(GL_COPY_WRITE_BUFFER, buf[1]);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if the buffer object bound to either "
                                      "readtarget or writetarget is mapped.");
        glMapBufferRange(GL_COPY_READ_BUFFER, 0, 4, GL_MAP_READ_BIT);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, 16);
        expectError(GL_INVALID_OPERATION);
        glUnmapBuffer(GL_COPY_READ_BUFFER);

        glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, 4, GL_MAP_READ_BIT);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, 16);
        expectError(GL_INVALID_OPERATION);
        glUnmapBuffer(GL_COPY_WRITE_BUFFER);
        m_log << TestLog::EndSection;

        glDeleteBuffers(2, buf);
    });
    ES3F_ADD_API_CASE(draw_buffers, "Invalid glDrawBuffers() usage", {
        uint32_t fbo;
        uint32_t texture;
        int maxDrawBuffers;
        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
        std::vector<uint32_t> values(maxDrawBuffers + 1);
        values[0] = GL_NONE;
        values[1] = GL_BACK;
        values[2] = GL_COLOR_ATTACHMENT0;
        values[3] = GL_DEPTH_ATTACHMENT;
        std::vector<GLfloat> data(32 * 32);
        bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if one of the values in bufs is not an accepted value.");
        glDrawBuffers(2, &values[2]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the GL is bound to the default framebuffer and n is not 1.");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffers(2, &values[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if the GL is bound to the default framebuffer "
                                      "and the value in bufs is one of the GL_COLOR_ATTACHMENTn tokens.");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffers(1, &values[2]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the GL is bound to a framebuffer object and the ith buffer "
                "listed in bufs is anything other than GL_NONE or GL_COLOR_ATTACHMENTSi.");
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glDrawBuffers(1, &values[1]);
        expectError(isES ? GL_INVALID_OPERATION : GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if n is less than 0 or greater than GL_MAX_DRAW_BUFFERS.");
        glDrawBuffers(-1, &values[1]);
        expectError(GL_INVALID_VALUE);
        glDrawBuffers(maxDrawBuffers + 1, &values[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
        glDeleteFramebuffers(1, &fbo);
    });
    ES3F_ADD_API_CASE(flush_mapped_buffer_range, "Invalid glFlushMappedBufferRange() usage", {
        uint32_t buf;
        std::vector<GLfloat> data(32);

        glGenBuffers(1, &buf);
        glBindBuffer(GL_ARRAY_BUFFER, buf);
        glBufferData(GL_ARRAY_BUFFER, 32, &data[0], GL_STATIC_READ);
        glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if offset or length is negative, or if offset + "
                                      "length exceeds the size of the mapping.");
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, -1, 1);
        expectError(GL_INVALID_VALUE);
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, -1);
        expectError(GL_INVALID_VALUE);
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, 12, 8);
        expectError(GL_INVALID_VALUE);
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, 24, 4);
        expectError(GL_INVALID_VALUE);
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, 24);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if zero is bound to target.");
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, 8);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if the buffer bound to target is not mapped, "
                                      "or is mapped without the GL_MAP_FLUSH_EXPLICIT flag.");
        glBindBuffer(GL_ARRAY_BUFFER, buf);
        glUnmapBuffer(GL_ARRAY_BUFFER);
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, 8);
        expectError(GL_INVALID_OPERATION);
        glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, GL_MAP_WRITE_BIT);
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, 8);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glUnmapBuffer(GL_ARRAY_BUFFER);
        glDeleteBuffers(1, &buf);
    });
    ES3F_ADD_API_CASE(map_buffer_range, "Invalid glMapBufferRange() usage", {
        uint32_t buf;
        std::vector<GLfloat> data(32);

        glGenBuffers(1, &buf);
        glBindBuffer(GL_ARRAY_BUFFER, buf);
        glBufferData(GL_ARRAY_BUFFER, 32, &data[0], GL_DYNAMIC_COPY);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if either of offset or length is negative.");
        glMapBufferRange(GL_ARRAY_BUFFER, -1, 1, GL_MAP_READ_BIT);
        expectError(GL_INVALID_VALUE);

        glMapBufferRange(GL_ARRAY_BUFFER, 1, -1, GL_MAP_READ_BIT);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if offset + length is greater than the value of GL_BUFFER_SIZE.");
        glMapBufferRange(GL_ARRAY_BUFFER, 0, 33, GL_MAP_READ_BIT);
        expectError(GL_INVALID_VALUE);

        glMapBufferRange(GL_ARRAY_BUFFER, 32, 1, GL_MAP_READ_BIT);
        expectError(GL_INVALID_VALUE);

        glMapBufferRange(GL_ARRAY_BUFFER, 16, 17, GL_MAP_READ_BIT);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if access has any bits set other than those accepted.");
        glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, GL_MAP_READ_BIT | 0x1000);
        expectError(GL_INVALID_VALUE);

        glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, GL_MAP_WRITE_BIT | 0x1000);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if the buffer is already in a mapped state.");
        glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, GL_MAP_WRITE_BIT);
        expectError(GL_NO_ERROR);
        glMapBufferRange(GL_ARRAY_BUFFER, 16, 8, GL_MAP_READ_BIT);
        expectError(GL_INVALID_OPERATION);
        glUnmapBuffer(GL_ARRAY_BUFFER);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if neither GL_MAP_READ_BIT or GL_MAP_WRITE_BIT is set.");
        glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, GL_MAP_INVALIDATE_RANGE_BIT);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if ");
        glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, GL_MAP_INVALIDATE_RANGE_BIT);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if GL_MAP_READ_BIT is set and any of GL_MAP_INVALIDATE_RANGE_BIT, "
                "GL_MAP_INVALIDATE_BUFFER_BIT, or GL_MAP_UNSYNCHRONIZED_BIT is set.");
        glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, GL_MAP_READ_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
        expectError(GL_INVALID_OPERATION);

        glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, GL_MAP_READ_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        expectError(GL_INVALID_OPERATION);

        glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, GL_MAP_READ_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "",
            "GL_INVALID_OPERATION is generated if GL_MAP_FLUSH_EXPLICIT_BIT is set and GL_MAP_WRITE_BIT is not set.");
        glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, GL_MAP_READ_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buf);
    });
    ES3F_ADD_API_CASE(read_buffer, "Invalid glReadBuffer() usage", {
        uint32_t fbo;
        uint32_t texture;
        int maxColorAttachments;
        bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());

        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if mode is not GL_BACK, GL_NONE, or GL_COLOR_ATTACHMENTi.");
        glReadBuffer(GL_NONE);
        expectError(GL_NO_ERROR);
        glReadBuffer(1);
        expectError(GL_INVALID_ENUM);
        glReadBuffer(GL_FRAMEBUFFER);
        expectError(GL_INVALID_ENUM);
        glReadBuffer(GL_COLOR_ATTACHMENT0 - 1);
        expectError(GL_INVALID_ENUM);
        glReadBuffer(GL_FRONT);
        expectError(isES ? GL_INVALID_ENUM : GL_INVALID_OPERATION);

        // \ note Spec isn't actually clear here, but it is safe to assume that
        //          GL_DEPTH_ATTACHMENT can't be interpreted as GL_COLOR_ATTACHMENTm
        //          where m = (GL_DEPTH_ATTACHMENT - GL_COLOR_ATTACHMENT0).
        glReadBuffer(GL_DEPTH_ATTACHMENT);
        expectError(GL_INVALID_ENUM);
        glReadBuffer(GL_STENCIL_ATTACHMENT);
        expectError(GL_INVALID_ENUM);
        glReadBuffer(GL_STENCIL_ATTACHMENT + 1);
        expectError(GL_INVALID_ENUM);
        glReadBuffer(0xffffffffu);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION error is generated if src is GL_BACK or if src is GL_COLOR_ATTACHMENTm where m "
                "is greater than or equal to the value of GL_MAX_COLOR_ATTACHMENTS.");
        glReadBuffer(GL_BACK);
        expectError(GL_INVALID_OPERATION);
        glReadBuffer(GL_COLOR_ATTACHMENT0 + maxColorAttachments);
        expectError(GL_INVALID_OPERATION);

        if (GL_COLOR_ATTACHMENT0 + maxColorAttachments < GL_DEPTH_ATTACHMENT - 1)
        {
            glReadBuffer(GL_DEPTH_ATTACHMENT - 1);
            expectError(GL_INVALID_OPERATION);
        }

        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if the current framebuffer is the default "
                                      "framebuffer and mode is not GL_NONE or GL_BACK.");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if the current framebuffer is a named "
                                      "framebuffer and mode is not GL_NONE or GL_COLOR_ATTACHMENTi.");
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glReadBuffer(GL_BACK);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
        glDeleteFramebuffers(1, &fbo);
    });
    ES3F_ADD_API_CASE(unmap_buffer, "Invalid glUnmapBuffer() usage", {
        uint32_t buf;
        std::vector<GLfloat> data(32);

        glGenBuffers(1, &buf);
        glBindBuffer(GL_ARRAY_BUFFER, buf);
        glBufferData(GL_ARRAY_BUFFER, 32, &data[0], GL_DYNAMIC_COPY);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the buffer data store is already in an unmapped state.");
        glUnmapBuffer(GL_ARRAY_BUFFER);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buf);
    });
    // Framebuffer Objects

    ES3F_ADD_API_CASE(bind_framebuffer, "Invalid glBindFramebuffer() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_FRAMEBUFFER.");
        glBindFramebuffer(-1, 0);
        expectError(GL_INVALID_ENUM);
        glBindFramebuffer(GL_RENDERBUFFER, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(bind_renderbuffer, "Invalid glBindRenderbuffer() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_RENDERBUFFER.");
        glBindRenderbuffer(-1, 0);
        expectError(GL_INVALID_ENUM);
        glBindRenderbuffer(GL_FRAMEBUFFER, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(check_framebuffer_status, "Invalid glCheckFramebufferStatus() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_FRAMEBUFFER.");
        glCheckFramebufferStatus(-1);
        expectError(GL_INVALID_ENUM);
        glCheckFramebufferStatus(GL_RENDERBUFFER);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(gen_framebuffers, "Invalid glGenFramebuffers() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glGenFramebuffers(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(gen_renderbuffers, "Invalid glGenRenderbuffers() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glGenRenderbuffers(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(delete_framebuffers, "Invalid glDeleteFramebuffers() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glDeleteFramebuffers(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(delete_renderbuffers, "Invalid glDeleteRenderbuffers() usage", {
        ;
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glDeleteRenderbuffers(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(framebuffer_renderbuffer, "Invalid glFramebufferRenderbuffer() usage", {
        GLuint fbo;
        GLuint rbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenRenderbuffers(1, &rbo);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not one of the accepted tokens.");
        glFramebufferRenderbuffer(-1, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if renderbuffertarget is not GL_RENDERBUFFER.");
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, -1, rbo);
        expectError(GL_INVALID_ENUM);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if renderbuffer is neither 0 nor the name of "
                                      "an existing renderbuffer object.");
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, -1);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if zero is bound to target.");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteRenderbuffers(1, &rbo);
        glDeleteFramebuffers(1, &fbo);
    });
    ES3F_ADD_API_CASE(framebuffer_texture2d, "Invalid glFramebufferTexture2D() usage", {
        GLuint fbo;
        GLuint tex2D;
        GLuint texCube;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &tex2D);
        glBindTexture(GL_TEXTURE_2D, tex2D);
        glGenTextures(1, &texCube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texCube);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not one of the accepted tokens.");
        glFramebufferTexture2D(-1, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if textarget is not an accepted texture target.");
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, -1, tex2D, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if level is less than 0 or larger than log_2 of maximum texture size.");
        int maxSize;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2D, -1);
        expectError(GL_INVALID_VALUE);
        maxSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2D, maxSize);
        expectError(GL_INVALID_VALUE);
        maxSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE)) + 1;
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, texCube, maxSize);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "",
            "GL_INVALID_OPERATION is generated if texture is neither 0 nor the name of an existing texture object.");
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, -1, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if textarget and texture are not compatible.");
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, tex2D, 0);
        expectError(GL_INVALID_OPERATION);
        glDeleteTextures(1, &tex2D);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texCube, 0);
        expectError(GL_INVALID_OPERATION);
        glDeleteTextures(1, &texCube);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if zero is bound to target.");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteFramebuffers(1, &fbo);
    });
    ES3F_ADD_API_CASE(renderbuffer_storage, "Invalid glRenderbufferStorage() usage", {
        uint32_t rbo;
        bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());

        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_RENDERBUFFER.");
        glRenderbufferStorage(-1, GL_RGBA4, 1, 1);
        expectError(GL_INVALID_ENUM);
        glRenderbufferStorage(GL_FRAMEBUFFER, GL_RGBA4, 1, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if internalformat is not a color-renderable, "
                                      "depth-renderable, or stencil-renderable format.");
        glRenderbufferStorage(GL_RENDERBUFFER, -1, 1, 1);
        expectError(GL_INVALID_ENUM);

        if (!m_context.getContextInfo().isExtensionSupported(
                "GL_EXT_color_buffer_half_float")) // GL_EXT_color_buffer_half_float disables error
        {
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB16F, 1, 1);
            expectError(isES ? GL_INVALID_ENUM : GL_NO_ERROR);
        }

        if (!m_context.getContextInfo().isExtensionSupported(
                "GL_EXT_render_snorm")) // GL_EXT_render_snorm disables error
        {
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8_SNORM, 1, 1);
            expectError(isES ? GL_INVALID_ENUM : GL_NO_ERROR);
        }

        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than zero.");
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, -1, 1);
        expectError(GL_INVALID_VALUE);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 1, -1);
        expectError(GL_INVALID_VALUE);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, -1, -1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_RENDERBUFFER_SIZE.");
        GLint maxSize;
        glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxSize);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 1, maxSize + 1);
        expectError(GL_INVALID_VALUE);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, maxSize + 1, 1);
        expectError(GL_INVALID_VALUE);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, maxSize + 1, maxSize + 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteRenderbuffers(1, &rbo);
    });
    ES3F_ADD_API_CASE(blit_framebuffer, "Invalid glBlitFramebuffer() usage", {
        uint32_t fbo[2];
        uint32_t rbo[2];
        uint32_t texture[2];

        glGenFramebuffers(2, fbo);
        glGenTextures(2, texture);
        glGenRenderbuffers(2, rbo);

        glBindTexture(GL_TEXTURE_2D, texture[0]);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo[0]);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo[0]);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 32, 32);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[0], 0);
        glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo[0]);
        glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);

        glBindTexture(GL_TEXTURE_2D, texture[1]);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo[1]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[1]);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 32, 32);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[1], 0);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo[1]);
        glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if mask contains any of the "
                                      "GL_DEPTH_BUFFER_BIT or GL_STENCIL_BUFFER_BIT and filter is not GL_NEAREST.");
        glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_LINEAR);
        expectError(GL_INVALID_OPERATION);
        glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_LINEAR);
        expectError(GL_INVALID_OPERATION);
        glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_LINEAR);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if mask contains GL_COLOR_BUFFER_BIT and read "
                                      "buffer format is incompatible with draw buffer format.");
        glBindTexture(GL_TEXTURE_2D, texture[0]);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 32, 32, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, NULL);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[0], 0);
        m_log << TestLog::Message << "// Read buffer: GL_RGBA32UI, draw buffer: GL_RGBA" << TestLog::EndMessage;
        glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        expectError(GL_INVALID_OPERATION);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, 32, 32, 0, GL_RGBA_INTEGER, GL_INT, NULL);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[0], 0);
        m_log << TestLog::Message << "// Read buffer: GL_RGBA32I, draw buffer: GL_RGBA" << TestLog::EndMessage;
        glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        expectError(GL_INVALID_OPERATION);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[0], 0);
        glBindTexture(GL_TEXTURE_2D, texture[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, 32, 32, 0, GL_RGBA_INTEGER, GL_INT, NULL);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[1], 0);
        m_log << TestLog::Message << "// Read buffer: GL_RGBA8, draw buffer: GL_RGBA32I" << TestLog::EndMessage;
        glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if filter is GL_LINEAR and the read buffer contains integer data.");
        glBindTexture(GL_TEXTURE_2D, texture[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 32, 32, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, NULL);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[0], 0);

        glBindTexture(GL_TEXTURE_2D, texture[1]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 32, 32, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, NULL);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture[1], 0);
        m_log << TestLog::Message << "// Read buffer: GL_RGBA32UI, draw buffer: GL_RGBA32UI" << TestLog::EndMessage;
        glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if mask contains GL_DEPTH_BUFFER_BIT or GL_STENCIL_BUFFER_BIT and "
                "the source and destination depth and stencil formats do not match.");
        glBindRenderbuffer(GL_RENDERBUFFER, rbo[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, 32, 32);
        glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo[0]);
        glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        expectError(GL_INVALID_OPERATION);
        glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_STENCIL_BUFFER_BIT, GL_NEAREST);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(2, fbo);
        glDeleteTextures(2, texture);
        glDeleteRenderbuffers(2, rbo);
    });
    ES3F_ADD_API_CASE(blit_framebuffer_multisample, "Invalid glBlitFramebuffer() usage", {
        uint32_t fbo[2];
        uint32_t rbo[2];

        glGenFramebuffers(2, fbo);
        glGenRenderbuffers(2, rbo);

        glBindRenderbuffer(GL_RENDERBUFFER, rbo[0]);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo[0]);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, 32, 32);
        glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo[0]);
        glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);

        glBindRenderbuffer(GL_RENDERBUFFER, rbo[1]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo[1]);

        expectError(GL_NO_ERROR);

        if (!m_context.getContextInfo().isExtensionSupported("GL_NV_framebuffer_multisample"))
        {
            bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());

            m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if the value of GL_SAMPLE_BUFFERS for the "
                                          "draw buffer is greater than zero.");
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, 32, 32);
            glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo[1]);
            glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            expectError(isES ? GL_INVALID_OPERATION : GL_NO_ERROR);
            m_log << TestLog::EndSection;

            m_log << TestLog::Section("",
                                      "GL_INVALID_OPERATION is generated if GL_SAMPLE_BUFFERS for the read buffer is "
                                      "greater than zero and the formats of draw and read buffers are not identical.");
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 32, 32);
            glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo[1]);
            glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            expectError(isES ? GL_INVALID_OPERATION : GL_NO_ERROR);
            m_log << TestLog::EndSection;

            m_log << TestLog::Section(
                "",
                "GL_INVALID_OPERATION is generated if GL_SAMPLE_BUFFERS for the read buffer is greater than zero and "
                "the source and destination rectangles are not defined with the same (X0, Y0) and (X1, Y1) bounds.");
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 32, 32);
            glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo[1]);
            glBlitFramebuffer(0, 0, 16, 16, 2, 2, 18, 18, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            expectError(isES ? GL_INVALID_OPERATION : GL_NO_ERROR);
            m_log << TestLog::EndSection;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(2, rbo);
        glDeleteFramebuffers(2, fbo);
    });
    ES3F_ADD_API_CASE(framebuffer_texture_layer, "Invalid glFramebufferTextureLayer() usage", {
        uint32_t fbo;
        uint32_t tex3D;
        uint32_t tex2DArray;
        uint32_t tex2D;

        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &tex3D);
        glGenTextures(1, &tex2DArray);
        glGenTextures(1, &tex2D);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glBindTexture(GL_TEXTURE_3D, tex3D);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex2DArray);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, tex2D);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not one of the accepted tokens.");
        glFramebufferTextureLayer(-1, GL_COLOR_ATTACHMENT0, tex3D, 0, 1);
        expectError(GL_INVALID_ENUM);
        glFramebufferTextureLayer(GL_RENDERBUFFER, GL_COLOR_ATTACHMENT0, tex3D, 0, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if attachment is not one of the accepted tokens.");
        glFramebufferTextureLayer(GL_FRAMEBUFFER, -1, tex3D, 0, 1);
        expectError(GL_INVALID_ENUM);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_BACK, tex3D, 0, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if texture is non-zero and not the name of a "
                                      "3D texture or 2D array texture.");
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, -1, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex2D, 0, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if texture is not zero and layer is negative.");
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex3D, 0, -1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if texture is not zero and layer is greater than "
                                      "GL_MAX_3D_TEXTURE_SIZE-1 for a 3D texture.");
        int max3DTexSize;
        glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max3DTexSize);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex3D, 0, max3DTexSize);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if texture is not zero and layer is greater than "
                                      "GL_MAX_ARRAY_TEXTURE_LAYERS-1 for a 2D array texture.");
        int maxArrayTexLayers;
        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayTexLayers);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex2DArray, 0, maxArrayTexLayers);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if zero is bound to target.");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex3D, 0, 1);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &tex3D);
        glDeleteTextures(1, &tex2DArray);
        glDeleteTextures(1, &tex2D);
        glDeleteFramebuffers(1, &fbo);
    });
    ES3F_ADD_API_CASE(invalidate_framebuffer, "Invalid glInvalidateFramebuffer() usage", {
        uint32_t fbo;
        uint32_t texture;
        uint32_t attachments[2];
        int maxColorAttachments;
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
        attachments[0] = GL_COLOR_ATTACHMENT0;
        attachments[1] = GL_COLOR_ATTACHMENT0 + maxColorAttachments;

        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &texture);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_FRAMEBUFFER, "
                                      "GL_READ_FRAMEBUFFER or GL_DRAW_FRAMEBUFFER.");
        glInvalidateFramebuffer(-1, 1, &attachments[0]);
        expectError(GL_INVALID_ENUM);
        glInvalidateFramebuffer(GL_BACK, 1, &attachments[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if attachments contains GL_COLOR_ATTACHMENTm "
                                      "and m is greater than or equal to the value of GL_MAX_COLOR_ATTACHMENTS.");
        glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &attachments[1]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
        glDeleteFramebuffers(1, &fbo);
    });
    ES3F_ADD_API_CASE(invalidate_sub_framebuffer, "Invalid glInvalidateSubFramebuffer() usage", {
        uint32_t fbo;
        uint32_t texture;
        uint32_t attachments[2];
        int maxColorAttachments;
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
        attachments[0] = GL_COLOR_ATTACHMENT0;
        attachments[1] = GL_COLOR_ATTACHMENT0 + maxColorAttachments;

        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &texture);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_FRAMEBUFFER, "
                                      "GL_READ_FRAMEBUFFER or GL_DRAW_FRAMEBUFFER.");
        glInvalidateSubFramebuffer(-1, 1, &attachments[0], 0, 0, 16, 16);
        expectError(GL_INVALID_ENUM);
        glInvalidateSubFramebuffer(GL_BACK, 1, &attachments[0], 0, 0, 16, 16);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if attachments contains GL_COLOR_ATTACHMENTm "
                                      "and m is greater than or equal to the value of GL_MAX_COLOR_ATTACHMENTS.");
        glInvalidateSubFramebuffer(GL_FRAMEBUFFER, 1, &attachments[1], 0, 0, 16, 16);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
        glDeleteFramebuffers(1, &fbo);
    });
    ES3F_ADD_API_CASE(renderbuffer_storage_multisample, "Invalid glRenderbufferStorageMultisample() usage", {
        uint32_t rbo;
        int maxSamplesSupportedRGBA4 = -1;
        bool isES                    = glu::isContextTypeES(m_context.getRenderContext().getType());

        glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA4, GL_SAMPLES, 1, &maxSamplesSupportedRGBA4);
        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_RENDERBUFFER.");
        glRenderbufferStorageMultisample(-1, 2, GL_RGBA4, 1, 1);
        expectError(GL_INVALID_ENUM);
        glRenderbufferStorageMultisample(GL_FRAMEBUFFER, 2, GL_RGBA4, 1, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if samples is greater than the maximum number "
                                      "of samples supported for internalformat.");
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamplesSupportedRGBA4 + 1, GL_RGBA4, 1, 1);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if internalformat is not a color-renderable, "
                                      "depth-renderable, or stencil-renderable format.");
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, -1, 1, 1);
        expectError(GL_INVALID_ENUM);

        if (!m_context.getContextInfo().isExtensionSupported(
                "GL_EXT_color_buffer_half_float")) // GL_EXT_color_buffer_half_float disables error
        {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_RGB16F, 1, 1);
            expectError(isES ? GL_INVALID_ENUM : GL_NO_ERROR);
        }

        if (!m_context.getContextInfo().isExtensionSupported(
                "GL_EXT_render_snorm")) // GL_EXT_render_snorm disables error
        {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_RGBA8_SNORM, 1, 1);
            expectError(isES ? GL_INVALID_ENUM : GL_NO_ERROR);
        }

        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than zero.");
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_RGBA4, -1, 1);
        expectError(GL_INVALID_VALUE);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_RGBA4, 1, -1);
        expectError(GL_INVALID_VALUE);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_RGBA4, -1, -1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_RENDERBUFFER_SIZE.");
        GLint maxSize;
        glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxSize);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA4, 1, maxSize + 1);
        expectError(GL_INVALID_VALUE);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA4, maxSize + 1, 1);
        expectError(GL_INVALID_VALUE);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA4, maxSize + 1, maxSize + 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteRenderbuffers(1, &rbo);
    });
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
