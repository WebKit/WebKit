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
 * \brief Negative Vertex Array API tests.
 *//*--------------------------------------------------------------------*/

#include "es3fNegativeVertexArrayApiTests.hpp"
#include "es3fApiCase.hpp"
#include "gluShaderProgram.hpp"
#include "gluContextInfo.hpp"
#include "deString.h"

#include "glwDefs.hpp"
#include "glwEnums.hpp"

using namespace glw; // GL types

namespace deqp
{
namespace gles3
{
namespace Functional
{

static const char *vertexShaderSource = "#version 300 es\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    gl_Position = vec4(0.0);\n"
                                        "}\n\0";

static const char *fragmentShaderSource = "#version 300 es\n"
                                          "layout(location = 0) out mediump vec4 fragColor;"
                                          "void main (void)\n"
                                          "{\n"
                                          "    fragColor = vec4(0.0);\n"
                                          "}\n\0";

using tcu::TestLog;

// Helper class that enables tests to be executed on GL4.5 context
// and removes code redundancy in each test that requires it.
class VAOHelper : protected glu::CallLogWrapper
{
public:
    VAOHelper(Context &ctx)
        : CallLogWrapper(ctx.getRenderContext().getFunctions(), ctx.getTestContext().getLog())
        , m_vao(0)
    {
        // tests need vao only for GL4.5 context
        if (glu::isContextTypeES(ctx.getRenderContext().getType()))
            return;

        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
        glVertexAttribPointer(0, 1, GL_BYTE, GL_TRUE, 0, NULL);
        glEnableVertexAttribArray(0);
    }

    ~VAOHelper()
    {
        if (m_vao)
            glDeleteVertexArrays(1, &m_vao);
    }

private:
    GLuint m_vao;
};

NegativeVertexArrayApiTests::NegativeVertexArrayApiTests(Context &context)
    : TestCaseGroup(context, "vertex_array", "Negative Vertex Array API Cases")
{
}

NegativeVertexArrayApiTests::~NegativeVertexArrayApiTests(void)
{
}

void NegativeVertexArrayApiTests::init(void)
{
    ES3F_ADD_API_CASE(vertex_attribf, "Invalid glVertexAttrib{1234}f() usage", {
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
    ES3F_ADD_API_CASE(vertex_attribfv, "Invalid glVertexAttrib{1234}fv() usage", {
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
    ES3F_ADD_API_CASE(vertex_attribi4, "Invalid glVertexAttribI4{i|ui}f() usage", {
        int maxVertexAttribs = m_context.getContextInfo().getInt(GL_MAX_VERTEX_ATTRIBS);
        GLint valInt         = 0;
        GLuint valUint       = 0;

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        glVertexAttribI4i(maxVertexAttribs, valInt, valInt, valInt, valInt);
        expectError(GL_INVALID_VALUE);
        glVertexAttribI4ui(maxVertexAttribs, valUint, valUint, valUint, valUint);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(vertex_attribi4v, "Invalid glVertexAttribI4{i|ui}fv() usage", {
        int maxVertexAttribs = m_context.getContextInfo().getInt(GL_MAX_VERTEX_ATTRIBS);
        GLint valInt[4]      = {0};
        GLuint valUint[4]    = {0};

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        glVertexAttribI4iv(maxVertexAttribs, &valInt[0]);
        expectError(GL_INVALID_VALUE);
        glVertexAttribI4uiv(maxVertexAttribs, &valUint[0]);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(vertex_attrib_pointer, "Invalid glVertexAttribPointer() usage", {
        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        if (glu::isContextTypeES(m_context.getRenderContext().getType()))
            glBindVertexArray(0);
        else
            glBindVertexArray(vao);

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

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if type is GL_INT_2_10_10_10_REV or "
                                           "GL_UNSIGNED_INT_2_10_10_10_REV and size is not 4.");
        glVertexAttribPointer(0, 2, GL_INT_2_10_10_10_REV, GL_TRUE, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glVertexAttribPointer(0, 2, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glVertexAttribPointer(0, 4, GL_INT_2_10_10_10_REV, GL_TRUE, 0, 0);
        expectError(GL_NO_ERROR);
        glVertexAttribPointer(0, 4, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, 0, 0);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_OPERATION is generated a non-zero vertex array object is bound, zero is bound to the "
                "GL_ARRAY_BUFFER buffer object binding point and the pointer argument is not NULL.");
        GLbyte offset = 1;
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        expectError(GL_NO_ERROR);

        glVertexAttribPointer(0, 1, GL_BYTE, GL_TRUE, 0, &offset);
        expectError(GL_INVALID_OPERATION);

        glBindVertexArray(0);
        glDeleteVertexArrays(1, &vao);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(vertex_attrib_i_pointer, "Invalid glVertexAttribPointer() usage", {
        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        if (glu::isContextTypeES(m_context.getRenderContext().getType()))
            glBindVertexArray(0);
        else
            glBindVertexArray(vao);

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if type is not an accepted value.");
        glVertexAttribIPointer(0, 1, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        glVertexAttribIPointer(0, 4, GL_INT_2_10_10_10_REV, 0, 0);
        expectError(GL_INVALID_ENUM);
        glVertexAttribIPointer(0, 4, GL_UNSIGNED_INT_2_10_10_10_REV, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        int maxVertexAttribs = m_context.getContextInfo().getInt(GL_MAX_VERTEX_ATTRIBS);
        glVertexAttribIPointer(maxVertexAttribs, 1, GL_BYTE, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if size is not 1, 2, 3, or 4.");
        glVertexAttribIPointer(0, 0, GL_BYTE, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if stride is negative.");
        glVertexAttribIPointer(0, 1, GL_BYTE, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_OPERATION is generated a non-zero vertex array object is bound, zero is bound to the "
                "GL_ARRAY_BUFFER buffer object binding point and the pointer argument is not NULL.");
        GLbyte offset = 1;
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        expectError(GL_NO_ERROR);

        glVertexAttribIPointer(0, 1, GL_BYTE, 0, &offset);
        expectError(GL_INVALID_OPERATION);

        glBindVertexArray(0);
        glDeleteVertexArrays(1, &vao);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(enable_vertex_attrib_array, "Invalid glEnableVertexAttribArray() usage", {
        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        int maxVertexAttribs = m_context.getContextInfo().getInt(GL_MAX_VERTEX_ATTRIBS);
        glEnableVertexAttribArray(maxVertexAttribs);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(disable_vertex_attrib_array, "Invalid glDisableVertexAttribArray() usage", {
        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        int maxVertexAttribs = m_context.getContextInfo().getInt(GL_MAX_VERTEX_ATTRIBS);
        glDisableVertexAttribArray(maxVertexAttribs);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(gen_vertex_arrays, "Invalid glGenVertexArrays() usage", {
        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        GLuint arrays;
        glGenVertexArrays(-1, &arrays);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(bind_vertex_array, "Invalid glBindVertexArray() usage", {
        m_log << tcu::TestLog::Section(
            "",
            "GL_INVALID_OPERATION is generated if array is not zero or the name of an existing vertex array object.");
        glBindVertexArray(-1);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(delete_vertex_arrays, "Invalid glDeleteVertexArrays() usage", {
        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glDeleteVertexArrays(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(vertex_attrib_divisor, "Invalid glVertexAttribDivisor() usage", {
        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        int maxVertexAttribs = m_context.getContextInfo().getInt(GL_MAX_VERTEX_ATTRIBS);
        glVertexAttribDivisor(maxVertexAttribs, 0);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(draw_arrays, "Invalid glDrawArrays() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());
        GLuint fbo;
        VAOHelper vao(m_context);

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
    ES3F_ADD_API_CASE(draw_arrays_invalid_program, "Invalid glDrawArrays() usage", {
        glUseProgram(0);
        GLuint fbo;
        VAOHelper vao(m_context);

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
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawArrays(GL_POINTS, 0, 1);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(draw_arrays_incomplete_primitive, "Invalid glDrawArrays() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());
        GLuint fbo;
        VAOHelper vao(m_context);

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
    ES3F_ADD_API_CASE(draw_elements, "Invalid glDrawElements() usage", {
        const bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());
        GLuint fbo;
        GLuint buf;
        GLuint tfID;
        GLfloat vertices[1] = {0};
        VAOHelper vao(m_context);

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawElements(-1, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if type is not one of the accepted values.");
        glDrawElements(GL_POINTS, 1, -1, vertices);
        expectError(GL_INVALID_ENUM);
        glDrawElements(GL_POINTS, 1, GL_FLOAT, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count is negative.");
        glDrawElements(GL_POINTS, -1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;

        if (isES && !m_context.getContextInfo().isExtensionSupported(
                        "GL_EXT_geometry_shader")) // GL_EXT_geometry_shader removes error
        {
            m_log << tcu::TestLog::Section(
                "", "GL_INVALID_OPERATION is generated if transform feedback is active and not paused.");
            const char *tfVarying = "gl_Position";

            glGenBuffers(1, &buf);
            glGenTransformFeedbacks(1, &tfID);

            glUseProgram(program.getProgram());
            glTransformFeedbackVaryings(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
            glLinkProgram(program.getProgram());
            glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID);
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
            glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
            glBeginTransformFeedback(GL_POINTS);
            expectError(GL_NO_ERROR);

            glDrawElements(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices);
            expectError(GL_INVALID_OPERATION);

            glPauseTransformFeedback();
            glDrawElements(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices);
            expectError(GL_NO_ERROR);

            glEndTransformFeedback();
            glDeleteBuffers(1, &buf);
            glDeleteTransformFeedbacks(1, &tfID);
            expectError(GL_NO_ERROR);
            m_log << tcu::TestLog::EndSection;
        }

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(draw_elements_invalid_program, "Invalid glDrawElements() usage", {
        glUseProgram(0);
        GLuint fbo;
        GLfloat vertices[1] = {0};
        VAOHelper vao(m_context);

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawElements(-1, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if type is not one of the accepted values.");
        glDrawElements(GL_POINTS, 1, -1, vertices);
        expectError(GL_INVALID_ENUM);
        glDrawElements(GL_POINTS, 1, GL_FLOAT, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count is negative.");
        glDrawElements(GL_POINTS, -1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(draw_elements_incomplete_primitive, "Invalid glDrawElements() usage", {
        const bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());
        GLuint fbo;
        GLuint buf;
        GLuint tfID;
        GLfloat vertices[1] = {0};
        VAOHelper vao(m_context);

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawElements(-1, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if type is not one of the accepted values.");
        glDrawElements(GL_TRIANGLES, 1, -1, vertices);
        expectError(GL_INVALID_ENUM);
        glDrawElements(GL_TRIANGLES, 1, GL_FLOAT, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count is negative.");
        glDrawElements(GL_TRIANGLES, -1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawElements(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;

        if (isES && !m_context.getContextInfo().isExtensionSupported(
                        "GL_EXT_geometry_shader")) // GL_EXT_geometry_shader removes error
        {
            m_log << tcu::TestLog::Section(
                "", "GL_INVALID_OPERATION is generated if transform feedback is active and not paused.");
            const char *tfVarying = "gl_Position";

            glGenBuffers(1, &buf);
            glGenTransformFeedbacks(1, &tfID);

            glUseProgram(program.getProgram());
            glTransformFeedbackVaryings(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
            glLinkProgram(program.getProgram());
            glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID);
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
            glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
            glBeginTransformFeedback(GL_TRIANGLES);
            expectError(GL_NO_ERROR);

            glDrawElements(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, vertices);
            expectError(GL_INVALID_OPERATION);

            glPauseTransformFeedback();
            glDrawElements(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, vertices);
            expectError(GL_NO_ERROR);

            glEndTransformFeedback();
            glDeleteBuffers(1, &buf);
            glDeleteTransformFeedbacks(1, &tfID);
            expectError(GL_NO_ERROR);
            m_log << tcu::TestLog::EndSection;
        }

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(draw_arrays_instanced, "Invalid glDrawArraysInstanced() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());
        GLuint fbo;
        VAOHelper vao(m_context);
        glVertexAttribDivisor(0, 1);
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawArraysInstanced(-1, 0, 1, 1);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count or primcount are negative.");
        glDrawArraysInstanced(GL_POINTS, 0, -1, 1);
        expectError(GL_INVALID_VALUE);
        glDrawArraysInstanced(GL_POINTS, 0, 1, -1);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawArraysInstanced(GL_POINTS, 0, 1, 1);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(draw_arrays_instanced_invalid_program, "Invalid glDrawArraysInstanced() usage", {
        glUseProgram(0);
        GLuint fbo;
        VAOHelper vao(m_context);
        glVertexAttribDivisor(0, 1);
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawArraysInstanced(-1, 0, 1, 1);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count or primcount are negative.");
        glDrawArraysInstanced(GL_POINTS, 0, -1, 1);
        expectError(GL_INVALID_VALUE);
        glDrawArraysInstanced(GL_POINTS, 0, 1, -1);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawArraysInstanced(GL_POINTS, 0, 1, 1);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(draw_arrays_instanced_incomplete_primitive, "Invalid glDrawArraysInstanced() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());
        GLuint fbo;
        VAOHelper vao(m_context);
        glVertexAttribDivisor(0, 1);
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawArraysInstanced(-1, 0, 1, 1);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count or primcount are negative.");
        glDrawArraysInstanced(GL_TRIANGLES, 0, -1, 1);
        expectError(GL_INVALID_VALUE);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 1, -1);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 1, 1);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(draw_elements_instanced, "Invalid glDrawElementsInstanced() usage", {
        const bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());
        GLuint fbo;
        GLuint buf;
        GLuint tfID;
        GLfloat vertices[1] = {0};
        VAOHelper vao(m_context);

        glVertexAttribDivisor(0, 1);
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawElementsInstanced(-1, 1, GL_UNSIGNED_BYTE, vertices, 1);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if type is not one of the accepted values.");
        glDrawElementsInstanced(GL_POINTS, 1, -1, vertices, 1);
        expectError(GL_INVALID_ENUM);
        glDrawElementsInstanced(GL_POINTS, 1, GL_FLOAT, vertices, 1);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count or primcount are negative.");
        glDrawElementsInstanced(GL_POINTS, -1, GL_UNSIGNED_BYTE, vertices, 1);
        expectError(GL_INVALID_VALUE);
        glDrawElementsInstanced(GL_POINTS, 11, GL_UNSIGNED_BYTE, vertices, -1);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawElementsInstanced(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices, 1);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;

        if (isES && !m_context.getContextInfo().isExtensionSupported(
                        "GL_EXT_geometry_shader")) // GL_EXT_geometry_shader removes error
        {
            m_log << tcu::TestLog::Section(
                "", "GL_INVALID_OPERATION is generated if transform feedback is active and not paused.");
            const char *tfVarying = "gl_Position";

            glGenBuffers(1, &buf);
            glGenTransformFeedbacks(1, &tfID);

            glUseProgram(program.getProgram());
            glTransformFeedbackVaryings(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
            glLinkProgram(program.getProgram());
            glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID);
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
            glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
            glBeginTransformFeedback(GL_POINTS);
            expectError(GL_NO_ERROR);

            glDrawElementsInstanced(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices, 1);
            expectError(GL_INVALID_OPERATION);

            glPauseTransformFeedback();
            glDrawElementsInstanced(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices, 1);
            expectError(GL_NO_ERROR);

            glEndTransformFeedback();
            glDeleteBuffers(1, &buf);
            glDeleteTransformFeedbacks(1, &tfID);
            expectError(GL_NO_ERROR);
            m_log << tcu::TestLog::EndSection;
        }

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(draw_elements_instanced_invalid_program, "Invalid glDrawElementsInstanced() usage", {
        glUseProgram(0);
        GLuint fbo;
        GLfloat vertices[1] = {0};
        VAOHelper vao(m_context);
        glVertexAttribDivisor(0, 1);
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawElementsInstanced(-1, 1, GL_UNSIGNED_BYTE, vertices, 1);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if type is not one of the accepted values.");
        glDrawElementsInstanced(GL_POINTS, 1, -1, vertices, 1);
        expectError(GL_INVALID_ENUM);
        glDrawElementsInstanced(GL_POINTS, 1, GL_FLOAT, vertices, 1);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count or primcount are negative.");
        glDrawElementsInstanced(GL_POINTS, -1, GL_UNSIGNED_BYTE, vertices, 1);
        expectError(GL_INVALID_VALUE);
        glDrawElementsInstanced(GL_POINTS, 11, GL_UNSIGNED_BYTE, vertices, -1);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawElementsInstanced(GL_POINTS, 1, GL_UNSIGNED_BYTE, vertices, 1);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(draw_elements_instanced_incomplete_primitive, "Invalid glDrawElementsInstanced() usage", {
        const bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());
        GLuint fbo;
        GLuint buf;
        GLuint tfID;
        GLfloat vertices[1] = {0};
        VAOHelper vao(m_context);
        glVertexAttribDivisor(0, 1);
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawElementsInstanced(-1, 1, GL_UNSIGNED_BYTE, vertices, 1);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if type is not one of the accepted values.");
        glDrawElementsInstanced(GL_TRIANGLES, 1, -1, vertices, 1);
        expectError(GL_INVALID_ENUM);
        glDrawElementsInstanced(GL_TRIANGLES, 1, GL_FLOAT, vertices, 1);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count or primcount are negative.");
        glDrawElementsInstanced(GL_TRIANGLES, -1, GL_UNSIGNED_BYTE, vertices, 1);
        expectError(GL_INVALID_VALUE);
        glDrawElementsInstanced(GL_TRIANGLES, 11, GL_UNSIGNED_BYTE, vertices, -1);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawElementsInstanced(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, vertices, 1);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;

        if (isES && !m_context.getContextInfo().isExtensionSupported(
                        "GL_EXT_geometry_shader")) // GL_EXT_geometry_shader removes error
        {
            m_log << tcu::TestLog::Section(
                "", "GL_INVALID_OPERATION is generated if transform feedback is active and not paused.");
            const char *tfVarying = "gl_Position";

            glGenBuffers(1, &buf);
            glGenTransformFeedbacks(1, &tfID);

            glUseProgram(program.getProgram());
            glTransformFeedbackVaryings(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
            glLinkProgram(program.getProgram());
            glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID);
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
            glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
            glBeginTransformFeedback(GL_TRIANGLES);
            expectError(GL_NO_ERROR);

            glDrawElementsInstanced(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, vertices, 1);
            expectError(GL_INVALID_OPERATION);

            glPauseTransformFeedback();
            glDrawElementsInstanced(GL_TRIANGLES, 1, GL_UNSIGNED_BYTE, vertices, 1);
            expectError(GL_NO_ERROR);

            glEndTransformFeedback();
            glDeleteBuffers(1, &buf);
            glDeleteTransformFeedbacks(1, &tfID);
            expectError(GL_NO_ERROR);
            m_log << tcu::TestLog::EndSection;
        }

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(draw_range_elements, "Invalid glDrawRangeElements() usage", {
        const bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());
        GLuint fbo;
        GLuint buf;
        GLuint tfID;
        uint32_t vertices[1];
        vertices[0] = 0xffffffffu;
        VAOHelper vao(m_context);

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawRangeElements(-1, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if type is not one of the accepted values.");
        glDrawRangeElements(GL_POINTS, 0, 1, 1, -1, vertices);
        expectError(GL_INVALID_ENUM);
        glDrawRangeElements(GL_POINTS, 0, 1, 1, GL_FLOAT, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count is negative.");
        glDrawRangeElements(GL_POINTS, 0, 1, -1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if end < start.");
        glDrawRangeElements(GL_POINTS, 1, 0, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawRangeElements(GL_POINTS, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;

        if (isES && !m_context.getContextInfo().isExtensionSupported(
                        "GL_EXT_geometry_shader")) // GL_EXT_geometry_shader removes error
        {
            m_log << tcu::TestLog::Section(
                "", "GL_INVALID_OPERATION is generated if transform feedback is active and not paused.");
            const char *tfVarying = "gl_Position";
            uint32_t verticesInRange[1];
            verticesInRange[0] = 0;

            glGenBuffers(1, &buf);
            glGenTransformFeedbacks(1, &tfID);

            glUseProgram(program.getProgram());
            glTransformFeedbackVaryings(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
            glLinkProgram(program.getProgram());
            glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID);
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
            glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
            glBeginTransformFeedback(GL_POINTS);
            expectError(GL_NO_ERROR);

            glDrawRangeElements(GL_POINTS, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
            expectError(GL_INVALID_OPERATION);

            glPauseTransformFeedback();
            glDrawRangeElements(GL_POINTS, 0, 1, 1, GL_UNSIGNED_BYTE, verticesInRange);
            expectError(GL_NO_ERROR);

            glEndTransformFeedback();
            glDeleteBuffers(1, &buf);
            glDeleteTransformFeedbacks(1, &tfID);
            expectError(GL_NO_ERROR);
            m_log << tcu::TestLog::EndSection;
        }

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(draw_range_elements_invalid_program, "Invalid glDrawRangeElements() usage", {
        glUseProgram(0);
        GLuint fbo;
        uint32_t vertices[1];
        VAOHelper vao(m_context);
        vertices[0] = 0xffffffffu;

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawRangeElements(-1, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if type is not one of the accepted values.");
        glDrawRangeElements(GL_POINTS, 0, 1, 1, -1, vertices);
        expectError(GL_INVALID_ENUM);
        glDrawRangeElements(GL_POINTS, 0, 1, 1, GL_FLOAT, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count is negative.");
        glDrawRangeElements(GL_POINTS, 0, 1, -1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if end < start.");
        glDrawRangeElements(GL_POINTS, 1, 0, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawRangeElements(GL_POINTS, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(draw_range_elements_incomplete_primitive, "Invalid glDrawRangeElements() usage", {
        const bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glUseProgram(program.getProgram());
        GLuint fbo;
        GLuint buf;
        GLuint tfID;
        uint32_t vertices[1];
        VAOHelper vao(m_context);
        vertices[0] = 0xffffffffu;

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if mode is not an accepted value.");
        glDrawRangeElements(-1, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if type is not one of the accepted values.");
        glDrawRangeElements(GL_TRIANGLES, 0, 1, 1, -1, vertices);
        expectError(GL_INVALID_ENUM);
        glDrawRangeElements(GL_TRIANGLES, 0, 1, 1, GL_FLOAT, vertices);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if count is negative.");
        glDrawRangeElements(GL_TRIANGLES, 0, 1, -1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if end < start.");
        glDrawRangeElements(GL_TRIANGLES, 1, 0, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glDrawRangeElements(GL_TRIANGLES, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;

        if (isES && !m_context.getContextInfo().isExtensionSupported(
                        "GL_EXT_geometry_shader")) // GL_EXT_geometry_shader removes error
        {
            m_log << tcu::TestLog::Section(
                "", "GL_INVALID_OPERATION is generated if transform feedback is active and not paused.");
            const char *tfVarying = "gl_Position";
            uint32_t verticesInRange[1];
            verticesInRange[0] = 0;

            glGenBuffers(1, &buf);
            glGenTransformFeedbacks(1, &tfID);

            glUseProgram(program.getProgram());
            glTransformFeedbackVaryings(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
            glLinkProgram(program.getProgram());
            glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID);
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
            glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
            glBeginTransformFeedback(GL_TRIANGLES);
            expectError(GL_NO_ERROR);

            glDrawRangeElements(GL_TRIANGLES, 0, 1, 1, GL_UNSIGNED_BYTE, vertices);
            expectError(GL_INVALID_OPERATION);

            glPauseTransformFeedback();
            glDrawRangeElements(GL_TRIANGLES, 0, 1, 1, GL_UNSIGNED_BYTE, verticesInRange);
            expectError(GL_NO_ERROR);

            glEndTransformFeedback();
            glDeleteBuffers(1, &buf);
            glDeleteTransformFeedbacks(1, &tfID);
            expectError(GL_NO_ERROR);
            m_log << tcu::TestLog::EndSection;
        }

        glUseProgram(0);
    });
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
