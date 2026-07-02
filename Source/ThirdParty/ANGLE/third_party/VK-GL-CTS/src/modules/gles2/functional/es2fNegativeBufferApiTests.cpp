/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
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

#include "es2fNegativeBufferApiTests.hpp"
#include "es2fApiCase.hpp"
#include "gluContextInfo.hpp"

#include "glwEnums.hpp"
#include "glwDefs.hpp"

using namespace glw; // GL types

namespace deqp
{
namespace gles2
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
    ES2F_ADD_API_CASE(bind_buffer, "Invalid glBindBuffer() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not one of the allowable values.");
        glBindBuffer(-1, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(delete_buffers, "Invalid glDeleteBuffers() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glDeleteBuffers(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(gen_buffers, "Invalid glGenBuffers() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glGenBuffers(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(buffer_data, "Invalid glBufferData() usage", {
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
    ES2F_ADD_API_CASE(buffer_sub_data, "Invalid glBufferSubData() usage", {
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

        glDeleteBuffers(1, &buffer);
    });
    ES2F_ADD_API_CASE(buffer_sub_data_size_offset, "Invalid glBufferSubData() usage", {
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
    ES2F_ADD_API_CASE(clear, "Invalid glClear() usage", {
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
    ES2F_ADD_API_CASE(read_pixels, "Invalid glReadPixels() usage", {
        std::vector<GLubyte> ubyteData(4);

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the combination of format and type is unsupported.");
        glReadPixels(0, 0, 1, 1, GL_LUMINANCE_ALPHA, GL_UNSIGNED_SHORT_4_4_4_4, &ubyteData[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

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
    });
    ES2F_ADD_API_CASE(read_pixels_format_mismatch, "Invalid glReadPixels() usage", {
        std::vector<GLubyte> ubyteData(4);
        std::vector<GLushort> ushortData(4);

        m_log << TestLog::Section(
            "", "Unsupported combinations of format and type will generate an INVALID_OPERATION error.");
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, &ushortData[0]);
        expectError(GL_INVALID_OPERATION);
        glReadPixels(0, 0, 1, 1, GL_ALPHA, GL_UNSIGNED_SHORT_5_6_5, &ushortData[0]);
        expectError(GL_INVALID_OPERATION);
        glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, &ushortData[0]);
        expectError(GL_INVALID_OPERATION);
        glReadPixels(0, 0, 1, 1, GL_ALPHA, GL_UNSIGNED_SHORT_4_4_4_4, &ushortData[0]);
        expectError(GL_INVALID_OPERATION);
        glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, &ushortData[0]);
        expectError(GL_INVALID_OPERATION);
        glReadPixels(0, 0, 1, 1, GL_ALPHA, GL_UNSIGNED_SHORT_5_5_5_1, &ushortData[0]);
        expectError(GL_INVALID_OPERATION);
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

    // Framebuffer Objects

    ES2F_ADD_API_CASE(bind_framebuffer, "Invalid glBindFramebuffer() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_FRAMEBUFFER.");
        glBindFramebuffer(-1, 0);
        expectError(GL_INVALID_ENUM);
        glBindFramebuffer(GL_RENDERBUFFER, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(bind_renderbuffer, "Invalid glBindRenderbuffer() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_RENDERBUFFER.");
        glBindRenderbuffer(-1, 0);
        expectError(GL_INVALID_ENUM);
        glBindRenderbuffer(GL_FRAMEBUFFER, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(check_framebuffer_status, "Invalid glCheckFramebufferStatus() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_FRAMEBUFFER.");
        glCheckFramebufferStatus(-1);
        expectError(GL_INVALID_ENUM);
        glCheckFramebufferStatus(GL_RENDERBUFFER);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(gen_framebuffers, "Invalid glGenFramebuffers() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glGenFramebuffers(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(gen_renderbuffers, "Invalid glGenRenderbuffers() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glGenRenderbuffers(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(delete_framebuffers, "Invalid glDeleteFramebuffers() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glDeleteFramebuffers(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(delete_renderbuffers, "Invalid glDeleteRenderbuffers() usage", {
        ;
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glDeleteRenderbuffers(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(framebuffer_renderbuffer, "Invalid glFramebufferRenderbuffer() usage", {
        GLuint fbo;
        GLuint rbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenRenderbuffers(1, &rbo);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_FRAMEBUFFER.");
        glFramebufferRenderbuffer(-1, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_ENUM is generated if attachment is not an accepted attachment point.");
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, -1, GL_RENDERBUFFER, 0);
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

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the default framebuffer object name 0 is bound.");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteRenderbuffers(1, &rbo);
        glDeleteFramebuffers(1, &fbo);
    });
    ES2F_ADD_API_CASE(framebuffer_texture2d, "Invalid glFramebufferTexture2D() usage", {
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

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_FRAMEBUFFER.");
        glFramebufferTexture2D(-1, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if textarget is not an accepted texture target.");
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, -1, tex2D, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_ENUM is generated if attachment is not an accepted attachment point.");
        glFramebufferTexture2D(GL_FRAMEBUFFER, -1, GL_TEXTURE_2D, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        if (!(m_context.getContextInfo().isExtensionSupported("GL_OES_fbo_render_mipmap") ||
              m_context.getContextInfo().isES3Compatible()))
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is not 0.");
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2D, 3);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }

        m_log << TestLog::Section(
            "",
            "GL_INVALID_OPERATION is generated if texture is neither 0 nor the name of an existing texture object.");
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, -1, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if texture is the name of an existing "
                                      "two-dimensional texture object but textarget is not GL_TEXTURE_2D.");

        expectError(GL_NO_ERROR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, tex2D, 0);
        expectError(GL_INVALID_OPERATION);
        glDeleteTextures(1, &tex2D);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if texture is the name of an existing cube "
                                      "map texture object but textarget is GL_TEXTURE_2D.");
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texCube, 0);
        expectError(GL_INVALID_OPERATION);
        glDeleteTextures(1, &texCube);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the default framebuffer object name 0 is bound.");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteFramebuffers(1, &fbo);
    });
    ES2F_ADD_API_CASE(renderbuffer_storage, "Invalid glRenderbufferStorage() usage", {
        GLuint rbo;
        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_RENDERBUFFER.");
        glRenderbufferStorage(-1, GL_RGBA4, 1, 1);
        expectError(GL_INVALID_ENUM);
        glRenderbufferStorage(GL_FRAMEBUFFER, GL_RGBA4, 1, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if internalformat is not an accepted format.");
        glRenderbufferStorage(GL_RENDERBUFFER, -1, 1, 1);
        expectError(GL_INVALID_ENUM);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, 1, 1);
        expectError(GL_INVALID_ENUM);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 1, 1);
        expectError(GL_INVALID_ENUM);
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

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the reserved renderbuffer object name 0 is bound.");
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 1, 1);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteRenderbuffers(1, &rbo);
    });
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
