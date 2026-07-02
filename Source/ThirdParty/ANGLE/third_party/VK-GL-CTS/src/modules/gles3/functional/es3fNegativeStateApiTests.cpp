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
 * \brief Negative GL State API tests.
 *//*--------------------------------------------------------------------*/

#include "es3fNegativeStateApiTests.hpp"
#include "es3fApiCase.hpp"
#include "gluShaderProgram.hpp"
#include "gluContextInfo.hpp"
#include "deMemory.h"

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

static const char *uniformTestVertSource = "#version 300 es\n"
                                           "uniform mediump vec4 vUnif_vec4;\n"
                                           "in mediump vec4 attr;"
                                           "layout(shared) uniform Block { mediump vec4 blockVar; };\n"
                                           "void main (void)\n"
                                           "{\n"
                                           "    gl_Position = vUnif_vec4 + blockVar + attr;\n"
                                           "}\n\0";
static const char *uniformTestFragSource = "#version 300 es\n"
                                           "uniform mediump ivec4 fUnif_ivec4;\n"
                                           "uniform mediump uvec4 fUnif_uvec4;\n"
                                           "layout(location = 0) out mediump vec4 fragColor;"
                                           "void main (void)\n"
                                           "{\n"
                                           "    fragColor = vec4(vec4(fUnif_ivec4) + vec4(fUnif_uvec4));\n"
                                           "}\n\0";

NegativeStateApiTests::NegativeStateApiTests(Context &context)
    : TestCaseGroup(context, "state", "Negative GL State API Cases")
{
}

NegativeStateApiTests::~NegativeStateApiTests(void)
{
}

void NegativeStateApiTests::init(void)
{
    // Enabling & disabling states

    ES3F_ADD_API_CASE(enable, "Invalid glEnable() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if cap is not one of the allowed values.");
        glEnable(-1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(disable, "Invalid glDisable() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if cap is not one of the allowed values.");
        glDisable(-1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });

    // Simple state queries

    ES3F_ADD_API_CASE(get_booleanv, "Invalid glGetBooleanv() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not one of the allowed values.");
        GLboolean params = GL_FALSE;
        glGetBooleanv(-1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_floatv, "Invalid glGetFloatv() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not one of the allowed values.");
        GLfloat params = 0.0f;
        glGetFloatv(-1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_integerv, "Invalid glGetIntegerv() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not one of the allowed values.");
        GLint params = -1;
        glGetIntegerv(-1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_integer64v, "Invalid glGetInteger64v() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not one of the allowed values.");
        GLint64 params = -1;
        glGetInteger64v(-1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_integeri_v, "Invalid glGetIntegeri_v() usage", {
        GLint data = -1;
        GLint maxUniformBufferBindings;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if name is not an accepted value.");
        glGetIntegeri_v(-1, 0, &data);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is outside of the valid range for the indexed state target.");
        glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxUniformBufferBindings);
        expectError(GL_NO_ERROR);
        glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, maxUniformBufferBindings, &data);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_integer64i_v, "Invalid glGetInteger64i_v() usage", {
        GLint64 data = (GLint64)-1;
        ;
        GLint maxUniformBufferBindings;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if name is not an accepted value.");
        glGetInteger64i_v(-1, 0, &data);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is outside of the valid range for the indexed state target.");
        glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxUniformBufferBindings);
        expectError(GL_NO_ERROR);
        glGetInteger64i_v(GL_UNIFORM_BUFFER_START, maxUniformBufferBindings, &data);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_string, "Invalid glGetString() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if name is not an accepted value.");
        glGetString(-1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_stringi, "Invalid glGetStringi() usage", {
        GLint numExtensions;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if name is not an accepted value.");
        glGetStringi(-1, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is outside the valid range for indexed state name.");
        glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
        glGetStringi(GL_EXTENSIONS, numExtensions);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });

    // Enumerated state queries: Shaders

    ES3F_ADD_API_CASE(get_attached_shaders, "Invalid glGetAttachedShaders() usage", {
        GLuint shaders[1];
        GLuint shaderObject = glCreateShader(GL_VERTEX_SHADER);
        GLuint program      = glCreateProgram();
        GLsizei count[1];

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glGetAttachedShaders(-1, 1, &count[0], &shaders[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glGetAttachedShaders(shaderObject, 1, &count[0], &shaders[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if maxCount is less than 0.");
        glGetAttachedShaders(program, -1, &count[0], &shaders[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteShader(shaderObject);
        glDeleteProgram(program);
    });
    ES3F_ADD_API_CASE(get_shaderiv, "Invalid glGetShaderiv() usage", {
        GLboolean shaderCompilerSupported;
        glGetBooleanv(GL_SHADER_COMPILER, &shaderCompilerSupported);
        m_log << TestLog::Message << "// GL_SHADER_COMPILER = " << (shaderCompilerSupported ? "GL_TRUE" : "GL_FALSE")
              << TestLog::EndMessage;

        GLuint shader  = glCreateShader(GL_VERTEX_SHADER);
        GLuint program = glCreateProgram();
        GLint param[1] = {-1};

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not an accepted value.");
        glGetShaderiv(shader, -1, &param[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if shader is not a value generated by OpenGL.");
        glGetShaderiv(-1, GL_SHADER_TYPE, &param[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if shader does not refer to a shader object.");
        glGetShaderiv(program, GL_SHADER_TYPE, &param[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteShader(shader);
        glDeleteProgram(program);
    });
    ES3F_ADD_API_CASE(get_shader_info_log, "Invalid glGetShaderInfoLog() usage", {
        GLuint shader     = glCreateShader(GL_VERTEX_SHADER);
        GLuint program    = glCreateProgram();
        GLsizei length[1] = {0};
        char infoLog[128] = {0};

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if shader is not a value generated by OpenGL.");
        glGetShaderInfoLog(-1, 128, &length[0], &infoLog[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if shader is not a shader object.");
        glGetShaderInfoLog(program, 128, &length[0], &infoLog[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if maxLength is less than 0.");
        glGetShaderInfoLog(shader, -1, &length[0], &infoLog[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteShader(shader);
        glDeleteProgram(program);
    });
    ES3F_ADD_API_CASE(get_shader_precision_format, "Invalid glGetShaderPrecisionFormat() usage", {
        GLboolean shaderCompilerSupported;
        glGetBooleanv(GL_SHADER_COMPILER, &shaderCompilerSupported);
        m_log << TestLog::Message << "// GL_SHADER_COMPILER = " << (shaderCompilerSupported ? "GL_TRUE" : "GL_FALSE")
              << TestLog::EndMessage;

        GLint range[2];
        GLint precision[1];

        deMemset(&range[0], 0xcd, sizeof(range));
        deMemset(&precision[0], 0xcd, sizeof(precision));

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if shaderType or precisionType is not an accepted value.");
        glGetShaderPrecisionFormat(-1, GL_MEDIUM_FLOAT, &range[0], &precision[0]);
        expectError(GL_INVALID_ENUM);
        glGetShaderPrecisionFormat(GL_VERTEX_SHADER, -1, &range[0], &precision[0]);
        expectError(GL_INVALID_ENUM);
        glGetShaderPrecisionFormat(-1, -1, &range[0], &precision[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_shader_source, "Invalid glGetShaderSource() usage", {
        GLsizei length[1] = {0};
        char source[1]    = {0};
        GLuint program    = glCreateProgram();
        GLuint shader     = glCreateShader(GL_VERTEX_SHADER);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if shader is not a value generated by OpenGL.");
        glGetShaderSource(-1, 1, &length[0], &source[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if shader is not a shader object.");
        glGetShaderSource(program, 1, &length[0], &source[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if bufSize is less than 0.");
        glGetShaderSource(shader, -1, &length[0], &source[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteProgram(program);
        glDeleteShader(shader);
    });

    // Enumerated state queries: Programs

    ES3F_ADD_API_CASE(get_programiv, "Invalid glGetProgramiv() usage", {
        GLuint program  = glCreateProgram();
        GLuint shader   = glCreateShader(GL_VERTEX_SHADER);
        GLint params[1] = {-1};

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not an accepted value.");
        glGetProgramiv(program, -1, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glGetProgramiv(-1, GL_LINK_STATUS, &params[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_OPERATION is generated if program does not refer to a program object.");
        glGetProgramiv(shader, GL_LINK_STATUS, &params[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteProgram(program);
        glDeleteShader(shader);
    });
    ES3F_ADD_API_CASE(get_program_info_log, "Invalid glGetProgramInfoLog() usage", {
        GLuint program    = glCreateProgram();
        GLuint shader     = glCreateShader(GL_VERTEX_SHADER);
        GLsizei length[1] = {0};
        char infoLog[1]   = {0};

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glGetProgramInfoLog(-1, 1, &length[0], &infoLog[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glGetProgramInfoLog(shader, 1, &length[0], &infoLog[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if maxLength is less than 0.");
        glGetProgramInfoLog(program, -1, &length[0], &infoLog[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteProgram(program);
        glDeleteShader(shader);
    });

    // Enumerated state queries: Shader variables

    ES3F_ADD_API_CASE(get_tex_parameterfv, "Invalid glGetTexParameterfv() usage", {
        GLfloat params[1] = {0.0f};

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target or pname is not an accepted value.");
        glGetTexParameterfv(-1, GL_TEXTURE_MAG_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        glGetTexParameterfv(GL_TEXTURE_2D, -1, &params[0]);
        expectError(GL_INVALID_ENUM);
        glGetTexParameterfv(-1, -1, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_tex_parameteriv, "Invalid glGetTexParameteriv() usage", {
        GLint params[1] = {0};

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target or pname is not an accepted value.");
        glGetTexParameteriv(-1, GL_TEXTURE_MAG_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        glGetTexParameteriv(GL_TEXTURE_2D, -1, &params[0]);
        expectError(GL_INVALID_ENUM);
        glGetTexParameteriv(-1, -1, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_uniformfv, "Invalid glGetUniformfv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));
        glUseProgram(program.getProgram());

        GLint unif = glGetUniformLocation(program.getProgram(), "vUnif_vec4"); // vec4
        if (unif == -1)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");

        GLuint shader       = glCreateShader(GL_VERTEX_SHADER);
        GLuint programEmpty = glCreateProgram();
        GLfloat params[4]   = {0.0f};

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glGetUniformfv(-1, unif, &params[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glGetUniformfv(shader, unif, &params[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program has not been successfully linked.");
        glGetUniformfv(programEmpty, unif, &params[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if location does not correspond to a valid "
                                      "uniform variable location for the specified program object.");
        glGetUniformfv(program.getProgram(), -1, &params[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteShader(shader);
        glDeleteProgram(programEmpty);
    });
    ES3F_ADD_API_CASE(get_uniformiv, "Invalid glGetUniformiv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));
        glUseProgram(program.getProgram());

        GLint unif = glGetUniformLocation(program.getProgram(), "fUnif_ivec4"); // ivec4
        if (unif == -1)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");

        GLuint shader       = glCreateShader(GL_VERTEX_SHADER);
        GLuint programEmpty = glCreateProgram();
        GLint params[4]     = {0};

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glGetUniformiv(-1, unif, &params[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glGetUniformiv(shader, unif, &params[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program has not been successfully linked.");
        glGetUniformiv(programEmpty, unif, &params[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if location does not correspond to a valid "
                                      "uniform variable location for the specified program object.");
        glGetUniformiv(program.getProgram(), -1, &params[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteShader(shader);
        glDeleteProgram(programEmpty);
    });
    ES3F_ADD_API_CASE(get_uniformuiv, "Invalid glGetUniformuiv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));
        glUseProgram(program.getProgram());

        GLint unif = glGetUniformLocation(program.getProgram(), "fUnif_uvec4"); // uvec4
        if (unif == -1)
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");

        GLuint shader       = glCreateShader(GL_VERTEX_SHADER);
        GLuint programEmpty = glCreateProgram();
        GLuint params[4]    = {0};

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glGetUniformuiv(-1, unif, &params[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glGetUniformuiv(shader, unif, &params[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program has not been successfully linked.");
        glGetUniformuiv(programEmpty, unif, &params[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if location does not correspond to a valid "
                                      "uniform variable location for the specified program object.");
        glGetUniformuiv(program.getProgram(), -1, &params[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteShader(shader);
        glDeleteProgram(programEmpty);
    });
    ES3F_ADD_API_CASE(get_active_uniform, "Invalid glGetActiveUniform() usage", {
        GLuint shader = glCreateShader(GL_VERTEX_SHADER);
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));
        GLint numActiveUniforms = -1;

        glGetProgramiv(program.getProgram(), GL_ACTIVE_UNIFORMS, &numActiveUniforms);
        m_log << TestLog::Message << "// GL_ACTIVE_UNIFORMS = " << numActiveUniforms << " (expected 4)."
              << TestLog::EndMessage;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glGetActiveUniform(-1, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glGetActiveUniform(shader, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if index is greater than or equal to the number "
                                      "of active uniform variables in program.");
        glUseProgram(program.getProgram());
        glGetActiveUniform(program.getProgram(), numActiveUniforms, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if bufSize is less than 0.");
        glGetActiveUniform(program.getProgram(), 0, -1, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glUseProgram(0);
        glDeleteShader(shader);
    });
    ES3F_ADD_API_CASE(get_active_uniformsiv, "Invalid glGetActiveUniformsiv() usage", {
        GLuint shader = glCreateShader(GL_VERTEX_SHADER);
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));
        GLuint unusedUniformIndex = 1;
        GLint unusedParamDst      = -1;
        GLint numActiveUniforms   = -1;

        glUseProgram(program.getProgram());

        glGetProgramiv(program.getProgram(), GL_ACTIVE_UNIFORMS, &numActiveUniforms);
        m_log << TestLog::Message << "// GL_ACTIVE_UNIFORMS = " << numActiveUniforms << " (expected 4)."
              << TestLog::EndMessage;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glGetActiveUniformsiv(-1, 1, &unusedUniformIndex, GL_UNIFORM_TYPE, &unusedParamDst);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glGetActiveUniformsiv(shader, 1, &unusedUniformIndex, GL_UNIFORM_TYPE, &unusedParamDst);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if any value in uniformIndices is greater than or "
                                      "equal to the value of GL_ACTIVE_UNIFORMS for program.");
        for (int excess = 0; excess <= 2; excess++)
        {
            std::vector<GLuint> invalidUniformIndices;
            invalidUniformIndices.push_back(1);
            invalidUniformIndices.push_back(numActiveUniforms - 1 + excess);
            invalidUniformIndices.push_back(1);

            std::vector<GLint> unusedParamsDst(invalidUniformIndices.size());
            glGetActiveUniformsiv(program.getProgram(), (GLsizei)invalidUniformIndices.size(),
                                  &invalidUniformIndices[0], GL_UNIFORM_TYPE, &unusedParamsDst[0]);
            expectError(excess == 0 ? GL_NO_ERROR : GL_INVALID_VALUE);
        }
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not an accepted token.");
        glGetActiveUniformsiv(program.getProgram(), 1, &unusedUniformIndex, -1, &unusedParamDst);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glUseProgram(0);
        glDeleteShader(shader);
    });
    ES3F_ADD_API_CASE(get_active_uniform_blockiv, "Invalid glGetActiveUniformBlockiv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));
        GLint params          = -1;
        GLint numActiveBlocks = -1;

        glGetProgramiv(program.getProgram(), GL_ACTIVE_UNIFORM_BLOCKS, &numActiveBlocks);
        m_log << TestLog::Message << "// GL_ACTIVE_UNIFORM_BLOCKS = " << numActiveBlocks << " (expected 1)."
              << TestLog::EndMessage;
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if uniformBlockIndex is greater than or equal to the value of "
                "GL_ACTIVE_UNIFORM_BLOCKS or is not the index of an active uniform block in program.");
        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);
        glGetActiveUniformBlockiv(program.getProgram(), numActiveBlocks, GL_UNIFORM_BLOCK_BINDING, &params);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not one of the accepted tokens.");
        glGetActiveUniformBlockiv(program.getProgram(), 0, -1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(get_active_uniform_block_name, "Invalid glGetActiveUniformBlockName() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));
        GLsizei length        = -1;
        GLint numActiveBlocks = -1;
        GLchar uniformBlockName[128];

        deMemset(&uniformBlockName[0], 0, sizeof(uniformBlockName));

        glGetProgramiv(program.getProgram(), GL_ACTIVE_UNIFORM_BLOCKS, &numActiveBlocks);
        m_log << TestLog::Message << "// GL_ACTIVE_UNIFORM_BLOCKS = " << numActiveBlocks << " (expected 1)."
              << TestLog::EndMessage;
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if uniformBlockIndex is greater than or equal to the value of "
                "GL_ACTIVE_UNIFORM_BLOCKS or is not the index of an active uniform block in program.");
        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);
        glGetActiveUniformBlockName(program.getProgram(), numActiveBlocks, (int)sizeof(uniformBlockName), &length,
                                    &uniformBlockName[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(get_active_attrib, "Invalid glGetActiveAttrib() usage", {
        GLuint shader = glCreateShader(GL_VERTEX_SHADER);
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));
        GLint numActiveAttributes = -1;

        GLsizei length = -1;
        GLint size     = -1;
        GLenum type    = -1;
        GLchar name[32];

        deMemset(&name[0], 0, sizeof(name));

        glGetProgramiv(program.getProgram(), GL_ACTIVE_ATTRIBUTES, &numActiveAttributes);
        m_log << TestLog::Message << "// GL_ACTIVE_ATTRIBUTES = " << numActiveAttributes << " (expected 1)."
              << TestLog::EndMessage;

        glUseProgram(program.getProgram());

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glGetActiveAttrib(-1, 0, 32, &length, &size, &type, &name[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glGetActiveAttrib(shader, 0, 32, &length, &size, &type, &name[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_ACTIVE_ATTRIBUTES.");
        glGetActiveAttrib(program.getProgram(), numActiveAttributes, (int)sizeof(name), &length, &size, &type,
                          &name[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if bufSize is less than 0.");
        glGetActiveAttrib(program.getProgram(), 0, -1, &length, &size, &type, &name[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glUseProgram(0);
        glDeleteShader(shader);
    });
    ES3F_ADD_API_CASE(get_uniform_indices, "Invalid glGetUniformIndices() usage", {
        GLuint shader = glCreateShader(GL_VERTEX_SHADER);
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));
        GLint numActiveBlocks     = -1;
        const GLchar *uniformName = "Block.blockVar";
        GLuint uniformIndices     = -1;

        glGetProgramiv(program.getProgram(), GL_ACTIVE_UNIFORM_BLOCKS, &numActiveBlocks);
        m_log << TestLog::Message << "// GL_ACTIVE_UNIFORM_BLOCKS = " << numActiveBlocks << TestLog::EndMessage;
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is a name of shader object.");
        glGetUniformIndices(shader, 1, &uniformName, &uniformIndices);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if program is not name of program or shader object.");
        GLuint invalid = -1;
        glGetUniformIndices(invalid, 1, &uniformName, &uniformIndices);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glUseProgram(0);
        glDeleteShader(shader);
    });
    ES3F_ADD_API_CASE(get_vertex_attribfv, "Invalid glGetVertexAttribfv() usage", {
        GLfloat params = 0.0f;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not an accepted value.");
        glGetVertexAttribfv(0, -1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        GLint maxVertexAttribs;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
        glGetVertexAttribfv(maxVertexAttribs, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &params);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_vertex_attribiv, "Invalid glGetVertexAttribiv() usage", {
        GLint params = -1;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not an accepted value.");
        glGetVertexAttribiv(0, -1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        GLint maxVertexAttribs;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
        glGetVertexAttribiv(maxVertexAttribs, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &params);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_vertex_attribi_iv, "Invalid glGetVertexAttribIiv() usage", {
        GLint params = -1;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not an accepted value.");
        glGetVertexAttribIiv(0, -1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        GLint maxVertexAttribs;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
        glGetVertexAttribIiv(maxVertexAttribs, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &params);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_vertex_attribi_uiv, "Invalid glGetVertexAttribIuiv() usage", {
        GLuint params = (GLuint)-1;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not an accepted value.");
        glGetVertexAttribIuiv(0, -1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        GLint maxVertexAttribs;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
        glGetVertexAttribIuiv(maxVertexAttribs, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &params);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_vertex_attrib_pointerv, "Invalid glGetVertexAttribPointerv() usage", {
        GLvoid *ptr[1] = {nullptr};

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not an accepted value.");
        glGetVertexAttribPointerv(0, -1, &ptr[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        GLint maxVertexAttribs;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
        glGetVertexAttribPointerv(maxVertexAttribs, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_frag_data_location, "Invalid glGetFragDataLocation() usage", {
        GLuint shader  = glCreateShader(GL_VERTEX_SHADER);
        GLuint program = glCreateProgram();

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is the name of a shader object.");
        glGetFragDataLocation(shader, "gl_FragColor");
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program has not been linked.");
        glGetFragDataLocation(program, "gl_FragColor");
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteProgram(program);
        glDeleteShader(shader);
    });

    // Enumerated state queries: Buffers

    ES3F_ADD_API_CASE(get_buffer_parameteriv, "Invalid glGetBufferParameteriv() usage", {
        GLint params = -1;
        GLuint buf;
        glGenBuffers(1, &buf);
        glBindBuffer(GL_ARRAY_BUFFER, buf);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target or value is not an accepted value.");
        glGetBufferParameteriv(-1, GL_BUFFER_SIZE, &params);
        expectError(GL_INVALID_ENUM);
        glGetBufferParameteriv(GL_ARRAY_BUFFER, -1, &params);
        expectError(GL_INVALID_ENUM);
        glGetBufferParameteriv(-1, -1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the reserved buffer object name 0 is bound to target.");
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &params);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buf);
    });
    ES3F_ADD_API_CASE(get_buffer_parameteri64v, "Invalid glGetBufferParameteri64v() usage", {
        GLint64 params = -1;
        GLuint buf;
        glGenBuffers(1, &buf);
        glBindBuffer(GL_ARRAY_BUFFER, buf);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target or value is not an accepted value.");
        glGetBufferParameteri64v(-1, GL_BUFFER_SIZE, &params);
        expectError(GL_INVALID_ENUM);
        glGetBufferParameteri64v(GL_ARRAY_BUFFER, -1, &params);
        expectError(GL_INVALID_ENUM);
        glGetBufferParameteri64v(-1, -1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the reserved buffer object name 0 is bound to target.");
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glGetBufferParameteri64v(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &params);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buf);
    });
    ES3F_ADD_API_CASE(get_buffer_pointerv, "Invalid glGetBufferPointerv() usage", {
        GLvoid *params = nullptr;
        GLuint buf;
        glGenBuffers(1, &buf);
        glBindBuffer(GL_ARRAY_BUFFER, buf);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target or pname is not an accepted value.");
        glGetBufferPointerv(GL_ARRAY_BUFFER, -1, &params);
        expectError(GL_INVALID_ENUM);
        glGetBufferPointerv(-1, GL_BUFFER_MAP_POINTER, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the reserved buffer object name 0 is bound to target.");
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glGetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER, &params);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buf);
    });
    ES3F_ADD_API_CASE(get_framebuffer_attachment_parameteriv, "Invalid glGetFramebufferAttachmentParameteriv() usage", {
        GLint params[1] = {-1};
        GLuint fbo;
        GLuint rbo[2];

        glGenFramebuffers(1, &fbo);
        glGenRenderbuffers(2, rbo);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 16, 16);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo[0]);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo[1]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, 16, 16);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo[1]);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not one of the accepted tokens.");
        glGetFramebufferAttachmentParameteriv(-1, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                              &params[0]); // TYPE is GL_RENDERBUFFER
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not valid for the value of "
                                      "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE.");
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                              GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL,
                                              &params[0]); // TYPE is GL_RENDERBUFFER
        expectError(GL_INVALID_ENUM);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                              &params[0]); // TYPE is GL_FRAMEBUFFER_DEFAULT
        expectError(GL_INVALID_ENUM);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_OPERATION is generated if attachment is GL_DEPTH_STENCIL_ATTACHMENT and "
                                  "different objects are bound to the depth and stencil attachment points of target.");
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &params[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE "
                "and pname is not GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME.");
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &params[0]); // TYPE is GL_NONE
        expectError(GL_NO_ERROR);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                              GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &params[0]); // TYPE is GL_NONE
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION or GL_INVALID_ENUM is generated if attachment is not one "
                                      "of the accepted values for the current binding of target.");
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                                              &params[0]); // A FBO is bound so GL_BACK is invalid
        expectError(GL_INVALID_OPERATION, GL_INVALID_ENUM);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
            &params[0]); // Default framebuffer is bound so GL_COLOR_ATTACHMENT0 is invalid
        expectError(GL_INVALID_OPERATION, GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteFramebuffers(1, &fbo);
    });
    ES3F_ADD_API_CASE(get_renderbuffer_parameteriv, "Invalid glGetRenderbufferParameteriv() usage", {
        GLint params[1] = {-1};
        GLuint rbo;
        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_RENDERBUFFER.");
        glGetRenderbufferParameteriv(-1, GL_RENDERBUFFER_WIDTH, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not one of the accepted tokens.");
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, -1, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    });
    ES3F_ADD_API_CASE(get_internalformativ, "Invalid glGetInternalformativ() usage", {
        const bool isES = glu::isContextTypeES(m_context.getRenderContext().getType());
        GLint params[16];

        deMemset(&params[0], 0xcd, sizeof(params));

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if bufSize is negative.");
        glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, -1, &params[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_ENUM is generated if pname is not GL_SAMPLES or GL_NUM_SAMPLE_COUNTS.");
        glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, -1, 16, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        if (isES)
        {
            m_log << TestLog::Section(
                "", "GL_INVALID_ENUM is generated if internalformat is not color-, depth-, or stencil-renderable.");
            if (!m_context.getContextInfo().isExtensionSupported("GL_EXT_render_snorm"))
            {
                glGetInternalformativ(GL_RENDERBUFFER, GL_RG8_SNORM, GL_NUM_SAMPLE_COUNTS, 16, &params[0]);
                expectError(GL_INVALID_ENUM);
            }

            glGetInternalformativ(GL_RENDERBUFFER, GL_COMPRESSED_RGB8_ETC2, GL_NUM_SAMPLE_COUNTS, 16, &params[0]);
            expectError(GL_INVALID_ENUM);
            m_log << TestLog::EndSection;
        }

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_RENDERBUFFER.");
        glGetInternalformativ(-1, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 16, &params[0]);
        expectError(GL_INVALID_ENUM);
        glGetInternalformativ(GL_FRAMEBUFFER, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 16, &params[0]);
        expectError(GL_INVALID_ENUM);

        if (isES && !m_context.getContextInfo().isExtensionSupported("GL_EXT_sparse_texture"))
        {
            glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, 16, &params[0]);
            expectError(GL_INVALID_ENUM);
        }
        m_log << TestLog::EndSection;
    });

    // Query object queries

    ES3F_ADD_API_CASE(get_queryiv, "Invalid glGetQueryiv() usage", {
        GLint params = -1;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target or pname is not an accepted value.");
        glGetQueryiv(GL_ANY_SAMPLES_PASSED, -1, &params);
        expectError(GL_INVALID_ENUM);
        glGetQueryiv(-1, GL_CURRENT_QUERY, &params);
        expectError(GL_INVALID_ENUM);
        glGetQueryiv(-1, -1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_query_objectuiv, "Invalid glGetQueryObjectuiv() usage", {
        GLuint params = -1;
        GLuint id;
        glGenQueries(1, &id);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if id is not the name of a query object.");
        glGetQueryObjectuiv(-1, GL_QUERY_RESULT_AVAILABLE, &params);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::Message << "// Note: " << id
              << " is not a query object yet, since it hasn't been used by glBeginQuery" << TestLog::EndMessage;
        glGetQueryObjectuiv(id, GL_QUERY_RESULT_AVAILABLE, &params);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glBeginQuery(GL_ANY_SAMPLES_PASSED, id);
        glEndQuery(GL_ANY_SAMPLES_PASSED);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not an accepted value.");
        glGetQueryObjectuiv(id, -1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if id is the name of a currently active query object.");
        glBeginQuery(GL_ANY_SAMPLES_PASSED, id);
        expectError(GL_NO_ERROR);
        glGetQueryObjectuiv(id, GL_QUERY_RESULT_AVAILABLE, &params);
        expectError(GL_INVALID_OPERATION);
        glEndQuery(GL_ANY_SAMPLES_PASSED);
        expectError(GL_NO_ERROR);
        m_log << TestLog::EndSection;

        glDeleteQueries(1, &id);
    });

    // Sync object queries

    ES3F_ADD_API_CASE(get_synciv, "Invalid glGetSynciv() usage", {
        GLsizei length = -1;
        GLint values[32];
        GLsync sync;

        deMemset(&values[0], 0xcd, sizeof(values));

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if sync is not the name of a sync object.");
        glGetSynciv(0, GL_OBJECT_TYPE, 32, &length, &values[0]);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not one of the accepted tokens.");
        sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        expectError(GL_NO_ERROR);
        glGetSynciv(sync, -1, 32, &length, &values[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });

    // Enumerated boolean state queries

    ES3F_ADD_API_CASE(is_enabled, "Invalid glIsEnabled() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if cap is not an accepted value.");
        glIsEnabled(-1);
        expectError(GL_INVALID_ENUM);
        glIsEnabled(GL_TRIANGLES);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });

    // Hints

    ES3F_ADD_API_CASE(hint, "Invalid glHint() usage", {
        m_log << TestLog::Section("",
                                  "GL_INVALID_ENUM is generated if either target or mode is not an accepted value.");
        glHint(GL_GENERATE_MIPMAP_HINT, -1);
        expectError(GL_INVALID_ENUM);
        glHint(-1, GL_FASTEST);
        expectError(GL_INVALID_ENUM);
        glHint(-1, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });

    // Named Object Usage

    ES3F_ADD_API_CASE(is_buffer, "Invalid glIsBuffer() usage", {
        GLuint buffer = 0;
        GLboolean isBuffer;

        m_log << TestLog::Section("", "A name returned by glGenBuffers, but not yet associated with a buffer object by "
                                      "calling glBindBuffer, is not the name of a buffer object.");
        isBuffer = glIsBuffer(buffer);
        checkBooleans(isBuffer, GL_FALSE);

        glGenBuffers(1, &buffer);
        isBuffer = glIsBuffer(buffer);
        checkBooleans(isBuffer, GL_FALSE);

        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        isBuffer = glIsBuffer(buffer);
        checkBooleans(isBuffer, GL_TRUE);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &buffer);
        isBuffer = glIsBuffer(buffer);
        checkBooleans(isBuffer, GL_FALSE);
        m_log << TestLog::EndSection;

        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(is_framebuffer, "Invalid glIsFramebuffer() usage", {
        GLuint fbo = 0;
        GLboolean isFbo;

        m_log << TestLog::Section("", "A name returned by glGenFramebuffers, but not yet bound through a call to "
                                      "glBindFramebuffer is not the name of a framebuffer object.");
        isFbo = glIsFramebuffer(fbo);
        checkBooleans(isFbo, GL_FALSE);

        glGenFramebuffers(1, &fbo);
        isFbo = glIsFramebuffer(fbo);
        checkBooleans(isFbo, GL_FALSE);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        isFbo = glIsFramebuffer(fbo);
        checkBooleans(isFbo, GL_TRUE);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        isFbo = glIsFramebuffer(fbo);
        checkBooleans(isFbo, GL_FALSE);
        m_log << TestLog::EndSection;

        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(is_program, "Invalid glIsProgram() usage", {
        GLuint program = 0;
        GLboolean isProgram;

        m_log << TestLog::Section("", "A name created with glCreateProgram, and not yet deleted with glDeleteProgram "
                                      "is a name of a program object.");
        isProgram = glIsProgram(program);
        checkBooleans(isProgram, GL_FALSE);

        program   = glCreateProgram();
        isProgram = glIsProgram(program);
        checkBooleans(isProgram, GL_TRUE);

        glDeleteProgram(program);
        isProgram = glIsProgram(program);
        checkBooleans(isProgram, GL_FALSE);
        m_log << TestLog::EndSection;

        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(is_renderbuffer, "Invalid glIsRenderbuffer() usage", {
        GLuint rbo = 0;
        GLboolean isRbo;

        m_log << TestLog::Section(
            "", "A name returned by glGenRenderbuffers, but not yet bound through a call to glBindRenderbuffer or "
                "glFramebufferRenderbuffer is not the name of a renderbuffer object.");
        isRbo = glIsRenderbuffer(rbo);
        checkBooleans(isRbo, GL_FALSE);

        glGenRenderbuffers(1, &rbo);
        isRbo = glIsRenderbuffer(rbo);
        checkBooleans(isRbo, GL_FALSE);

        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        isRbo = glIsRenderbuffer(rbo);
        checkBooleans(isRbo, GL_TRUE);

        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glDeleteRenderbuffers(1, &rbo);
        isRbo = glIsRenderbuffer(rbo);
        checkBooleans(isRbo, GL_FALSE);
        m_log << TestLog::EndSection;

        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(is_shader, "Invalid glIsShader() usage", {
        GLuint shader = 0;
        GLboolean isShader;

        m_log << TestLog::Section("", "A name created with glCreateShader, and not yet deleted with glDeleteShader is "
                                      "a name of a shader object.");
        isShader = glIsProgram(shader);
        checkBooleans(isShader, GL_FALSE);

        shader   = glCreateShader(GL_VERTEX_SHADER);
        isShader = glIsShader(shader);
        checkBooleans(isShader, GL_TRUE);

        glDeleteShader(shader);
        isShader = glIsShader(shader);
        checkBooleans(isShader, GL_FALSE);
        m_log << TestLog::EndSection;

        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(is_texture, "Invalid glIsTexture() usage", {
        GLuint texture = 0;
        GLboolean isTexture;

        m_log << TestLog::Section("", "A name returned by glGenTextures, but not yet bound through a call to "
                                      "glBindTexture is not the name of a texture.");
        isTexture = glIsTexture(texture);
        checkBooleans(isTexture, GL_FALSE);

        glGenTextures(1, &texture);
        isTexture = glIsTexture(texture);
        checkBooleans(isTexture, GL_FALSE);

        glBindTexture(GL_TEXTURE_2D, texture);
        isTexture = glIsTexture(texture);
        checkBooleans(isTexture, GL_TRUE);

        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &texture);
        isTexture = glIsTexture(texture);
        checkBooleans(isTexture, GL_FALSE);
        m_log << TestLog::EndSection;

        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(is_query, "Invalid glIsQuery() usage", {
        GLuint query = 0;
        GLboolean isQuery;

        m_log << TestLog::Section("", "A name returned by glGenQueries, but not yet associated with a query object by "
                                      "calling glBeginQuery, is not the name of a query object.");
        isQuery = glIsQuery(query);
        checkBooleans(isQuery, GL_FALSE);

        glGenQueries(1, &query);
        isQuery = glIsQuery(query);
        checkBooleans(isQuery, GL_FALSE);

        glBeginQuery(GL_ANY_SAMPLES_PASSED, query);
        isQuery = glIsQuery(query);
        checkBooleans(isQuery, GL_TRUE);

        glEndQuery(GL_ANY_SAMPLES_PASSED);
        glDeleteQueries(1, &query);
        isQuery = glIsQuery(query);
        checkBooleans(isQuery, GL_FALSE);
        m_log << TestLog::EndSection;

        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(is_sampler, "Invalid glIsSampler() usage", {
        GLuint sampler = 0;
        GLboolean isSampler;

        m_log << TestLog::Section("", "A name returned by glGenSamplers is the name of a sampler object.");
        isSampler = glIsSampler(sampler);
        checkBooleans(isSampler, GL_FALSE);

        glGenSamplers(1, &sampler);
        isSampler = glIsSampler(sampler);
        checkBooleans(isSampler, GL_TRUE);

        glBindSampler(0, sampler);
        isSampler = glIsSampler(sampler);
        checkBooleans(isSampler, GL_TRUE);

        glDeleteSamplers(1, &sampler);
        isSampler = glIsSampler(sampler);
        checkBooleans(isSampler, GL_FALSE);
        m_log << TestLog::EndSection;

        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(is_sync, "Invalid glIsSync() usage", {
        GLsync sync = 0;
        GLboolean isSync;

        m_log << TestLog::Section("", "A name returned by glFenceSync is the name of a sync object.");
        isSync = glIsSync(sync);
        checkBooleans(isSync, GL_FALSE);

        sync   = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        isSync = glIsSync(sync);
        checkBooleans(isSync, GL_TRUE);

        glDeleteSync(sync);
        isSync = glIsSync(sync);
        checkBooleans(isSync, GL_FALSE);
        m_log << TestLog::EndSection;

        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(is_transform_feedback, "Invalid glIsTransformFeedback() usage", {
        GLuint tf = 0;
        GLboolean isTF;

        m_log << TestLog::Section("", "A name returned by glGenTransformFeedbacks, but not yet bound using "
                                      "glBindTransformFeedback, is not the name of a transform feedback object.");
        isTF = glIsTransformFeedback(tf);
        checkBooleans(isTF, GL_FALSE);

        glGenTransformFeedbacks(1, &tf);
        isTF = glIsTransformFeedback(tf);
        checkBooleans(isTF, GL_FALSE);

        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf);
        isTF = glIsTransformFeedback(tf);
        checkBooleans(isTF, GL_TRUE);

        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
        glDeleteTransformFeedbacks(1, &tf);
        isTF = glIsTransformFeedback(tf);
        checkBooleans(isTF, GL_FALSE);
        m_log << TestLog::EndSection;

        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(is_vertex_array, "Invalid glIsVertexArray() usage", {
        GLuint vao = 0;
        GLboolean isVao;

        m_log << TestLog::Section("", "A name returned by glGenVertexArrays, but not yet bound using "
                                      "glBindVertexArray, is not the name of a vertex array object.");
        isVao = glIsVertexArray(vao);
        checkBooleans(isVao, GL_FALSE);

        glGenVertexArrays(1, &vao);
        isVao = glIsVertexArray(vao);
        checkBooleans(isVao, GL_FALSE);

        glBindVertexArray(vao);
        isVao = glIsVertexArray(vao);
        checkBooleans(isVao, GL_TRUE);

        glBindVertexArray(0);
        glDeleteVertexArrays(1, &vao);
        isVao = glIsVertexArray(vao);
        checkBooleans(isVao, GL_FALSE);
        m_log << TestLog::EndSection;

        expectError(GL_NO_ERROR);
    });
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
