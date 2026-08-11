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
 * \brief Negative Vertex Array API tests.
 *//*--------------------------------------------------------------------*/

#include "es2fNegativeVertexArrayApiTests.hpp"
#include "es2fApiCase.hpp"
#include "gluShaderProgram.hpp"
#include "gluContextInfo.hpp"
#include "deString.h"

#include "glwEnums.hpp"
#include "glwDefs.hpp"

using namespace glw; // GL types

namespace deqp
{
namespace gles2
{
namespace Functional
{

static const char *vertexShaderSource   = "void main (void) { gl_Position = vec4(0.0); }\n\0";
static const char *fragmentShaderSource = "void main (void) { gl_FragColor = vec4(0.0); }\n\0";

using tcu::TestLog;

NegativeVertexArrayApiTests::NegativeVertexArrayApiTests(Context &context)
    : TestCaseGroup(context, "vertex_array", "Negative Vertex Array API Cases")
{
}

NegativeVertexArrayApiTests::~NegativeVertexArrayApiTests(void)
{
}

void NegativeVertexArrayApiTests::init(void)
{
    ES2F_ADD_API_CASE(vertex_attrib, "Invalid glVertexAttrib() usage", {
        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        int maxVertexAttribs = m_context.getContextInfo().getInt(GL_MAX_VERTEX_ATTRIBS);
        glVertexAttrib1f(maxVertexAttribs, 0.0f);
        expectError(GL_INVALID_VALUE);
        glVertexAttrib2f(maxVertexAttribs, 0.0f, 0.0f);
        expectError(GL_INVALID_VALUE);
        glVertexAttrib3f(maxVertexAttribs, 0.0f, 0.0f, 0.0f);
        expectError(GL_INVALID_VALUE);
        glVertexAttrib4f(maxVertexAttribs, 0.0f, 0.0f, 0.0f, 0.0f);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(vertex_attribv, "Invalid glVertexAttribv() usage", {
        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        int maxVertexAttribs = m_context.getContextInfo().getInt(GL_MAX_VERTEX_ATTRIBS);
        float v[4]           = {0.0f};
        glVertexAttrib1fv(maxVertexAttribs, &v[0]);
        expectError(GL_INVALID_VALUE);
        glVertexAttrib2fv(maxVertexAttribs, &v[0]);
        expectError(GL_INVALID_VALUE);
        glVertexAttrib3fv(maxVertexAttribs, &v[0]);
        expectError(GL_INVALID_VALUE);
        glVertexAttrib4fv(maxVertexAttribs, &v[0]);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(vertex_attrib_pointer, "Invalid glVertexAttribPointer() usage", {
        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if type is not an accepted value.");
        glVertexAttribPointer(0, 1, 0, GL_TRUE, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        int maxVertexAttribs = m_context.getContextInfo().getInt(GL_MAX_VERTEX_ATTRIBS);
        glVertexAttribPointer(maxVertexAttribs, 1, GL_BYTE, GL_TRUE, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if size is not 1, 2, 3, or 4.");
        glVertexAttribPointer(0, 0, GL_BYTE, GL_TRUE, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if stride is negative.");
        glVertexAttribPointer(0, 1, GL_BYTE, GL_TRUE, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(enable_vertex_attrib_array, "Invalid glEnableVertexAttribArray() usage", {
        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        int maxVertexAttribs = m_context.getContextInfo().getInt(GL_MAX_VERTEX_ATTRIBS);
        glEnableVertexAttribArray(maxVertexAttribs);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(disable_vertex_attrib_array, "Invalid glDisableVertexAttribArray() usage", {
        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        int maxVertexAttribs = m_context.getContextInfo().getInt(GL_MAX_VERTEX_ATTRIBS);
        glDisableVertexAttribArray(maxVertexAttribs);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(draw_arrays, "Invalid glDrawArrays() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawArrays(-1, 0, 1);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count is negative.");
        glDrawArrays(GL_POINTS, 0, -1);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawArrays(GL_POINTS, 0, 1);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES2F_ADD_API_CASE(draw_arrays_invalid_program, "Invalid glDrawArrays() usage", {
        glUseProgram(0);

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawArrays(-1, 0, 1);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count is negative.");
        glDrawArrays(GL_POINTS, 0, -1);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawArrays(GL_POINTS, 0, 1);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(draw_arrays_incomplete_primitive, "Invalid glDrawArrays() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawArrays(-1, 0, 1);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count is negative.");
        glDrawArrays(GL_TRIANGLES, 0, -1);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawArrays(GL_TRIANGLES, 0, 1);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES2F_ADD_API_CASE(draw_elements, "Invalid glDrawElements() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());
        GLfloat vertices[1] = {0.0f};

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawElements(-1, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_ENUM is generated if type is not GL_UNSIGNED_BYTE or GL_UNSIGNED_SHORT.");
        glDrawElements(GL_POINTS, 1, -1, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count is negative.");
        glDrawElements(GL_POINTS, -1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES2F_ADD_API_CASE(draw_elements_invalid_program, "Invalid glDrawElements() usage", {
        glUseProgram(0);
        GLfloat vertices[1] = {0.0f};

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawElements(-1, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_ENUM is generated if type is not GL_UNSIGNED_BYTE or GL_UNSIGNED_SHORT.");
        glDrawElements(GL_POINTS, 1, -1, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count is negative.");
        glDrawElements(GL_POINTS, -1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(draw_elements_incomplete_primitive, "Invalid glDrawElements() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());
        GLfloat vertices[1] = {0.0f};

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawElements(-1, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_ENUM is generated if type is not GL_UNSIGNED_BYTE or GL_UNSIGNED_SHORT.");
        glDrawElements(GL_TRIANGLES, 1, -1, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count is negative.");
        glDrawElements(GL_TRIANGLES, -1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawElements(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
