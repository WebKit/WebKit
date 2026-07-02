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
 * \brief Negative Shader API tests.
 *//*--------------------------------------------------------------------*/

#include "es3fNegativeShaderApiTests.hpp"
#include "gluShaderProgram.hpp"
#include "gluContextInfo.hpp"
#include "deUniquePtr.hpp"

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

static const char *glFragDepthRedeclarationFragSource = "#version 300 es\n"
                                                        "#extension GL_EXT_conservative_depth: enable\n"
                                                        "out highp float gl_FragDepth;\n"
                                                        "void main (void)\n"
                                                        "{\n"
                                                        "}\n\0";

static const char *glFragDepthRedeclarationFragExtNotEnabledSource = "#version 300 es\n"
                                                                     "out highp float gl_FragDepth;\n"
                                                                     "void main (void)\n"
                                                                     "{\n"
                                                                     "}\n\0";

static const char *uniformTestVertSource = "#version 300 es\n"
                                           "uniform mediump vec4 vec4_v;\n"
                                           "uniform mediump mat4 mat4_v;\n"
                                           "void main (void)\n"
                                           "{\n"
                                           "    gl_Position = mat4_v * vec4_v;\n"
                                           "}\n\0";

static const char *uniformTestFragSource = "#version 300 es\n"
                                           "uniform mediump ivec4 ivec4_f;\n"
                                           "uniform mediump uvec4 uvec4_f;\n"
                                           "uniform sampler2D sampler_f;\n"
                                           "layout(location = 0) out mediump vec4 fragColor;"
                                           "void main (void)\n"
                                           "{\n"
                                           "    fragColor.xy = (vec4(uvec4_f) + vec4(ivec4_f)).xy;\n"
                                           "    fragColor.zw = texture(sampler_f, vec2(0.0, 0.0)).zw;\n"
                                           "}\n\0";

static const char *uniformBlockVertSource = "#version 300 es\n"
                                            "layout(shared) uniform Block { lowp float var; };\n"
                                            "void main (void)\n"
                                            "{\n"
                                            "    gl_Position = vec4(var);\n"
                                            "}\n\0";

NegativeShaderApiTests::NegativeShaderApiTests(Context &context)
    : TestCaseGroup(context, "shader", "Negative Shader API Cases")
{
}

NegativeShaderApiTests::~NegativeShaderApiTests(void)
{
}

void NegativeShaderApiTests::init(void)
{
    glu::RenderContext &renderContext = m_context.getRenderContext();
    glu::ContextType contextType      = renderContext.getType();
    if (!glu::isContextTypeGLCore(contextType) && glu::isContextTypeES(contextType) &&
        glu::contextSupports(m_context.getRenderContext().getType(), glu::ApiType::es(3, 0)))
    {
        addChild(
            new ApiCaseRedeclaringGlFragDepth(m_context, "redeclaring_gl_frag_depth", "gl_FragDepth redeclaration"));
        addChild(new ApiCaseRedeclaringGlFragDepthExtensionNotEnabled(
            m_context, "redeclaring_gl_frag_depth_extension_not_enabled",
            "gl_FragDepth redeclaration without extension enabled in shader"));
    }
    // Shader control commands
    ES3F_ADD_API_CASE(create_shader, "Invalid glCreateShader() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if shaderType is not an accepted value.");
        glCreateShader(-1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(shader_source, "Invalid glShaderSource() usage", {
        // \note Shader compilation must be supported.

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if shader is not a value generated by OpenGL.");
        glShaderSource(123456, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if count is less than 0.");
        GLuint shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(shader, -1, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if shader is not a shader object.");
        GLuint program = glCreateProgram();
        glShaderSource(program, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteProgram(program);
        glDeleteShader(shader);
    });
    ES3F_ADD_API_CASE(compile_shader, "Invalid glCompileShader() usage", {
        // \note Shader compilation must be supported.

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if shader is not a value generated by OpenGL.");
        glCompileShader(9);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if shader is not a shader object.");
        GLuint program = glCreateProgram();
        glCompileShader(program);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteProgram(program);
    });
    ES3F_ADD_API_CASE(delete_shader, "Invalid glDeleteShader() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if shader is not a value generated by OpenGL.");
        glDeleteShader(9);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(shader_binary, "Invalid glShaderBinary() usage", {
        std::vector<int32_t> binaryFormats;
        getSupportedExtensions(GL_NUM_SHADER_BINARY_FORMATS, GL_SHADER_BINARY_FORMATS, binaryFormats);
        bool shaderBinarySupported = !binaryFormats.empty();
        if (!shaderBinarySupported)
            m_log << TestLog::Message << "// Shader binaries not supported." << TestLog::EndMessage;
        else
            m_log << TestLog::Message << "// Shader binaries supported" << TestLog::EndMessage;

        GLuint shaders[2];
        shaders[0] = glCreateShader(GL_VERTEX_SHADER);
        shaders[1] = glCreateShader(GL_VERTEX_SHADER);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if binaryFormat is not an accepted value.");
        glShaderBinary(1, &shaders[0], -1, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        if (shaderBinarySupported)
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if the data pointed to by binary does not "
                                          "match the format specified by binaryFormat.");
            const GLbyte data = 0x005F;
            glShaderBinary(1, &shaders[0], binaryFormats[0], &data, 1);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;

            // Error handling is different in case of SPIRV.
            const bool spirvBinary = binaryFormats[0] == GL_SHADER_BINARY_FORMAT_SPIR_V;
            if (spirvBinary)
                m_log << TestLog::Section("", "GL_INVALID_VALUE due to invalid data pointer.");
            else
                m_log << TestLog::Section(
                    "", "GL_INVALID_OPERATION is generated if more than one of the handles in shaders refers to the "
                        "same type of shader, or GL_INVALID_VALUE due to invalid data pointer.");

            glShaderBinary(2, &shaders[0], binaryFormats[0], 0, 0);

            if (spirvBinary)
                expectError(GL_INVALID_VALUE);
            else
                expectError(GL_INVALID_OPERATION, GL_INVALID_VALUE);

            m_log << TestLog::EndSection;
        }

        glDeleteShader(shaders[0]);
        glDeleteShader(shaders[1]);
    });
    ES3F_ADD_API_CASE(attach_shader, "Invalid glAttachShader() usage", {
        GLuint shader1 = glCreateShader(GL_VERTEX_SHADER);
        GLuint shader2 = glCreateShader(GL_VERTEX_SHADER);
        GLuint program = glCreateProgram();

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glAttachShader(shader1, shader1);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if shader is not a shader object.");
        glAttachShader(program, program);
        expectError(GL_INVALID_OPERATION);
        glAttachShader(shader1, program);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if either program or shader is not a value generated by OpenGL.");
        glAttachShader(program, -1);
        expectError(GL_INVALID_VALUE);
        glAttachShader(-1, shader1);
        expectError(GL_INVALID_VALUE);
        glAttachShader(-1, -1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if shader is already attached to program.");
        glAttachShader(program, shader1);
        expectError(GL_NO_ERROR);
        glAttachShader(program, shader1);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        if (glu::isContextTypeES(m_context.getRenderContext().getType()))
        {
            m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if a shader of the same type as shader is "
                                          "already attached to program.");
            glAttachShader(program, shader2);
            expectError(GL_INVALID_OPERATION);
            m_log << TestLog::EndSection;
        }

        glDeleteProgram(program);
        glDeleteShader(shader1);
        glDeleteShader(shader2);
    });
    ES3F_ADD_API_CASE(detach_shader, "Invalid glDetachShader() usage", {
        GLuint shader  = glCreateShader(GL_VERTEX_SHADER);
        GLuint program = glCreateProgram();

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if either program or shader is not a value generated by OpenGL.");
        glDetachShader(-1, shader);
        expectError(GL_INVALID_VALUE);
        glDetachShader(program, -1);
        expectError(GL_INVALID_VALUE);
        glDetachShader(-1, -1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glDetachShader(shader, shader);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if shader is not a shader object.");
        glDetachShader(program, program);
        expectError(GL_INVALID_OPERATION);
        glDetachShader(shader, program);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if shader is not attached to program.");
        glDetachShader(program, shader);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteProgram(program);
        glDeleteShader(shader);
    });
    ES3F_ADD_API_CASE(link_program, "Invalid glLinkProgram() usage", {
        GLuint shader = glCreateShader(GL_VERTEX_SHADER);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glLinkProgram(-1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glLinkProgram(shader);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteShader(shader);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is the currently active program "
                                      "object and transform feedback mode is active.");
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        uint32_t buf;
        uint32_t tfID;
        const char *tfVarying = "gl_Position";

        glGenTransformFeedbacks(1, &tfID);
        glGenBuffers(1, &buf);

        glUseProgram(program.getProgram());
        glTransformFeedbackVaryings(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(program.getProgram());
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
        glBeginTransformFeedback(GL_TRIANGLES);
        expectError(GL_NO_ERROR);

        glLinkProgram(program.getProgram());
        expectError(GL_INVALID_OPERATION);

        glEndTransformFeedback();
        glDeleteTransformFeedbacks(1, &tfID);
        glDeleteBuffers(1, &buf);
        expectError(GL_NO_ERROR);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(use_program, "Invalid glUseProgram() usage", {
        GLuint shader = glCreateShader(GL_VERTEX_SHADER);

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if program is neither 0 nor a value generated by OpenGL.");
        glUseProgram(-1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glUseProgram(shader);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if transform feedback mode is active and not paused.");
        glu::ShaderProgram program1(m_context.getRenderContext(),
                                    glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glu::ShaderProgram program2(m_context.getRenderContext(),
                                    glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        uint32_t buf;
        uint32_t tfID;
        const char *tfVarying = "gl_Position";

        glGenTransformFeedbacks(1, &tfID);
        glGenBuffers(1, &buf);

        glUseProgram(program1.getProgram());
        glTransformFeedbackVaryings(program1.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(program1.getProgram());
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
        glBeginTransformFeedback(GL_TRIANGLES);
        expectError(GL_NO_ERROR);

        glUseProgram(program2.getProgram());
        expectError(GL_INVALID_OPERATION);

        glPauseTransformFeedback();
        glUseProgram(program2.getProgram());
        expectError(GL_NO_ERROR);

        glEndTransformFeedback();
        glDeleteTransformFeedbacks(1, &tfID);
        glDeleteBuffers(1, &buf);
        expectError(GL_NO_ERROR);
        m_log << TestLog::EndSection;

        glUseProgram(0);
        glDeleteShader(shader);
    });
    ES3F_ADD_API_CASE(delete_program, "Invalid glDeleteProgram() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glDeleteProgram(-1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(validate_program, "Invalid glValidateProgram() usage", {
        GLuint shader = glCreateShader(GL_VERTEX_SHADER);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glValidateProgram(-1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glValidateProgram(shader);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteShader(shader);
    });
    if (glu::isContextTypeES(m_context.getRenderContext().getType()))
    {
        ES3F_ADD_API_CASE(get_program_binary, "Invalid glGetProgramBinary() usage", {
            glu::ShaderProgram program(m_context.getRenderContext(),
                                       glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
            glu::ShaderProgram programInvalid(m_context.getRenderContext(),
                                              glu::makeVtxFragSources(vertexShaderSource, ""));
            GLenum binaryFormat  = -1;
            GLsizei binaryLength = -1;
            GLint binaryPtr      = -1;
            GLint bufSize        = -1;
            GLint linkStatus     = -1;

            m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if bufSize is less than the size of "
                                          "GL_PROGRAM_BINARY_LENGTH for program.");
            glGetProgramiv(program.getProgram(), GL_PROGRAM_BINARY_LENGTH, &bufSize);
            expectError(GL_NO_ERROR);
            glGetProgramiv(program.getProgram(), GL_LINK_STATUS, &linkStatus);
            m_log << TestLog::Message << "// GL_PROGRAM_BINARY_LENGTH = " << bufSize << TestLog::EndMessage;
            m_log << TestLog::Message << "// GL_LINK_STATUS = " << linkStatus << TestLog::EndMessage;
            expectError(GL_NO_ERROR);

            glGetProgramBinary(program.getProgram(), 0, &binaryLength, &binaryFormat, &binaryPtr);
            expectError(GL_INVALID_OPERATION);
            if (bufSize > 0)
            {
                glGetProgramBinary(program.getProgram(), bufSize - 1, &binaryLength, &binaryFormat, &binaryPtr);
                expectError(GL_INVALID_OPERATION);
            }
            m_log << TestLog::EndSection;

            m_log << TestLog::Section(
                "", "GL_INVALID_OPERATION is generated if GL_LINK_STATUS for the program object is false.");
            glGetProgramiv(programInvalid.getProgram(), GL_PROGRAM_BINARY_LENGTH, &bufSize);
            expectError(GL_NO_ERROR);
            glGetProgramiv(programInvalid.getProgram(), GL_LINK_STATUS, &linkStatus);
            m_log << TestLog::Message << "// GL_PROGRAM_BINARY_LENGTH = " << bufSize << TestLog::EndMessage;
            m_log << TestLog::Message << "// GL_LINK_STATUS = " << linkStatus << TestLog::EndMessage;
            expectError(GL_NO_ERROR);

            glGetProgramBinary(programInvalid.getProgram(), bufSize, &binaryLength, &binaryFormat, &binaryPtr);
            expectError(GL_INVALID_OPERATION);
            m_log << TestLog::EndSection;
        });
    }
    ES3F_ADD_API_CASE(program_binary, "Invalid glProgramBinary() usage", {
        glu::ShaderProgram srcProgram(m_context.getRenderContext(),
                                      glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        GLuint dstProgram    = glCreateProgram();
        GLuint unusedShader  = glCreateShader(GL_VERTEX_SHADER);
        GLenum binaryFormat  = -1;
        GLsizei binaryLength = -1;
        std::vector<uint8_t> binaryBuf;
        GLint bufSize    = -1;
        GLint linkStatus = -1;

        glGetProgramiv(srcProgram.getProgram(), GL_PROGRAM_BINARY_LENGTH, &bufSize);
        glGetProgramiv(srcProgram.getProgram(), GL_LINK_STATUS, &linkStatus);
        m_log << TestLog::Message << "// GL_PROGRAM_BINARY_LENGTH = " << bufSize << TestLog::EndMessage;
        m_log << TestLog::Message << "// GL_LINK_STATUS = " << linkStatus << TestLog::EndMessage;

        TCU_CHECK(bufSize >= 0);

        if (bufSize > 0)
        {
            binaryBuf.resize(bufSize);
            glGetProgramBinary(srcProgram.getProgram(), bufSize, &binaryLength, &binaryFormat, &binaryBuf[0]);
            expectError(GL_NO_ERROR);

            m_log << TestLog::Section(
                "", "GL_INVALID_OPERATION is generated if program is not the name of an existing program object.");
            glProgramBinary(unusedShader, binaryFormat, &binaryBuf[0], binaryLength);
            expectError(GL_INVALID_OPERATION);
            m_log << TestLog::EndSection;

            m_log << TestLog::Section(
                "", "GL_INVALID_ENUM is generated if binaryFormat is not a value recognized by the implementation.");
            glProgramBinary(dstProgram, -1, &binaryBuf[0], binaryLength);
            expectError(GL_INVALID_ENUM);
            m_log << TestLog::EndSection;
        }

        glDeleteShader(unusedShader);
        glDeleteProgram(dstProgram);
    });
    ES3F_ADD_API_CASE(program_parameteri, "Invalid glProgramParameteri() usage", {
        GLuint program = glCreateProgram();

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if program is not the name of an existing program object.");
        glProgramParameteri(0, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_ENUM is generated if pname is not GL_PROGRAM_BINARY_RETRIEVABLE_HINT.");
        glProgramParameteri(program, -1, GL_TRUE);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if value is not GL_FALSE or GL_TRUE.");
        glProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, 2);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteProgram(program);
    });
    ES3F_ADD_API_CASE(gen_samplers, "Invalid glGenSamplers() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        GLuint sampler;
        glGenSamplers(-1, &sampler);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(bind_sampler, "Invalid glBindSampler() usage", {
        int maxTexImageUnits;
        GLuint sampler;
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTexImageUnits);
        glGenSamplers(1, &sampler);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if unit is greater than or equal to the value of "
                                      "GL_MAX_COMBIED_TEXTURE_IMAGE_UNITS.");
        glBindSampler(maxTexImageUnits, sampler);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if sampler is not zero or a name previously "
                                      "returned from a call to glGenSamplers.");
        glBindSampler(1, -1);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if sampler has been deleted by a call to glDeleteSamplers.");
        glDeleteSamplers(1, &sampler);
        glBindSampler(1, sampler);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(delete_samplers, "Invalid glDeleteSamplers() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glDeleteSamplers(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(get_sampler_parameteriv, "Invalid glGetSamplerParameteriv() usage", {
        int params = -1;
        GLuint sampler;
        glGenSamplers(1, &sampler);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if sampler is not the name of a sampler "
                                      "object returned from a previous call to glGenSamplers.");
        glGetSamplerParameteriv(-1, GL_TEXTURE_MAG_FILTER, &params);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not an accepted value.");
        glGetSamplerParameteriv(sampler, -1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteSamplers(1, &sampler);
    });
    ES3F_ADD_API_CASE(get_sampler_parameterfv, "Invalid glGetSamplerParameterfv() usage", {
        float params = 0.0f;
        GLuint sampler;
        glGenSamplers(1, &sampler);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if sampler is not the name of a sampler "
                                      "object returned from a previous call to glGenSamplers.");
        glGetSamplerParameterfv(-1, GL_TEXTURE_MAG_FILTER, &params);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not an accepted value.");
        glGetSamplerParameterfv(sampler, -1, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteSamplers(1, &sampler);
    });
    ES3F_ADD_API_CASE(sampler_parameteri, "Invalid glSamplerParameteri() usage", {
        GLuint sampler;
        glGenSamplers(1, &sampler);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if sampler is not the name of a sampler "
                                      "object previously returned from a call to glGenSamplers.");
        glSamplerParameteri(-1, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if params should have a defined constant value "
                                      "(based on the value of pname) and does not.");
        glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, -1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteSamplers(1, &sampler);
    });
    ES3F_ADD_API_CASE(sampler_parameteriv, "Invalid glSamplerParameteriv() usage", {
        int params = -1;
        GLuint sampler;
        glGenSamplers(1, &sampler);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if sampler is not the name of a sampler "
                                      "object previously returned from a call to glGenSamplers.");
        params = GL_CLAMP_TO_EDGE;
        glSamplerParameteriv(-1, GL_TEXTURE_WRAP_S, &params);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if params should have a defined constant value "
                                      "(based on the value of pname) and does not.");
        params = -1;
        glSamplerParameteriv(sampler, GL_TEXTURE_WRAP_S, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteSamplers(1, &sampler);
    });
    ES3F_ADD_API_CASE(sampler_parameterf, "Invalid glSamplerParameterf() usage", {
        GLuint sampler;
        glGenSamplers(1, &sampler);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if sampler is not the name of a sampler "
                                      "object previously returned from a call to glGenSamplers.");
        glSamplerParameterf(-1, GL_TEXTURE_MIN_LOD, -1000.0f);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if params should have a defined constant value "
                                      "(based on the value of pname) and does not.");
        glSamplerParameterf(sampler, GL_TEXTURE_WRAP_S, -1.0f);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteSamplers(1, &sampler);
    });
    ES3F_ADD_API_CASE(sampler_parameterfv, "Invalid glSamplerParameterfv() usage", {
        float params = 0.0f;
        GLuint sampler;
        glGenSamplers(1, &sampler);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if sampler is not the name of a sampler "
                                      "object previously returned from a call to glGenSamplers.");
        params = -1000.0f;
        glSamplerParameterfv(-1, GL_TEXTURE_WRAP_S, &params);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if params should have a defined constant value "
                                      "(based on the value of pname) and does not.");
        params = -1.0f;
        glSamplerParameterfv(sampler, GL_TEXTURE_WRAP_S, &params);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteSamplers(1, &sampler);
    });

    // Shader data commands

    ES3F_ADD_API_CASE(get_attrib_location, "Invalid glGetAttribLocation() usage", {
        GLuint programEmpty = glCreateProgram();
        GLuint shader       = glCreateShader(GL_VERTEX_SHADER);

        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program has not been successfully linked.");
        glBindAttribLocation(programEmpty, 0, "test");
        glGetAttribLocation(programEmpty, "test");
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a program or shader object.");
        glUseProgram(program.getProgram());
        glBindAttribLocation(program.getProgram(), 0, "test");
        expectError(GL_NO_ERROR);
        glGetAttribLocation(program.getProgram(), "test");
        expectError(GL_NO_ERROR);
        glGetAttribLocation(-2, "test");
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glGetAttribLocation(shader, "test");
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glUseProgram(0);
        glDeleteShader(shader);
        glDeleteProgram(programEmpty);
    });
    ES3F_ADD_API_CASE(get_uniform_location, "Invalid glGetUniformLocation() usage", {
        GLuint programEmpty = glCreateProgram();
        GLuint shader       = glCreateShader(GL_VERTEX_SHADER);

        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program has not been successfully linked.");
        glGetUniformLocation(programEmpty, "test");
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glUseProgram(program.getProgram());
        glGetUniformLocation(-2, "test");
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glGetAttribLocation(shader, "test");
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glUseProgram(0);
        glDeleteProgram(programEmpty);
        glDeleteShader(shader);
    });
    ES3F_ADD_API_CASE(bind_attrib_location, "Invalid glBindAttribLocation() usage", {
        GLuint program  = glCreateProgram();
        GLuint maxIndex = m_context.getContextInfo().getInt(GL_MAX_VERTEX_ATTRIBS);
        GLuint shader   = glCreateShader(GL_VERTEX_SHADER);

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if index is greater than or equal to GL_MAX_VERTEX_ATTRIBS.");
        glBindAttribLocation(program, maxIndex, "test");
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_OPERATION is generated if name starts with the reserved prefix \"gl_\".");
        glBindAttribLocation(program, maxIndex - 1, "gl_test");
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if program is not a value generated by OpenGL.");
        glBindAttribLocation(-1, maxIndex - 1, "test");
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if program is not a program object.");
        glBindAttribLocation(shader, maxIndex - 1, "test");
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteProgram(program);
        glDeleteShader(shader);
    });
    ES3F_ADD_API_CASE(uniform_block_binding, "Invalid glUniformBlockBinding() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformBlockVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());

        GLint maxUniformBufferBindings;
        GLint numActiveUniforms = -1;
        GLint numActiveBlocks   = -1;
        glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &maxUniformBufferBindings);
        glGetProgramiv(program.getProgram(), GL_ACTIVE_UNIFORMS, &numActiveUniforms);
        glGetProgramiv(program.getProgram(), GL_ACTIVE_UNIFORM_BLOCKS, &numActiveBlocks);
        m_log << TestLog::Message << "// GL_MAX_UNIFORM_BUFFER_BINDINGS = " << maxUniformBufferBindings
              << TestLog::EndMessage;
        m_log << TestLog::Message << "// GL_ACTIVE_UNIFORMS = " << numActiveUniforms << TestLog::EndMessage;
        m_log << TestLog::Message << "// GL_ACTIVE_UNIFORM_BLOCKS = " << numActiveBlocks << TestLog::EndMessage;
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_VALUE is generated if uniformBlockIndex is not an active uniform block index of program.");
        glUniformBlockBinding(program.getProgram(), -1, 0);
        expectError(GL_INVALID_VALUE);
        glUniformBlockBinding(program.getProgram(), 5, 0);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if uniformBlockBinding is greater than or "
                                           "equal to the value of GL_MAX_UNIFORM_BUFFER_BINDINGS.");
        glUniformBlockBinding(program.getProgram(), maxUniformBufferBindings, 0);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_VALUE is generated if program is not the name of a program object generated by the GL.");
        glUniformBlockBinding(-1, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });

    // glUniform*f

    ES3F_ADD_API_CASE(uniformf_invalid_program, "Invalid glUniform{1234}f() usage", {
        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if there is no current program object.");
        glUseProgram(0);
        glUniform1f(-1, 0.0f);
        expectError(GL_INVALID_OPERATION);
        glUniform2f(-1, 0.0f, 0.0f);
        expectError(GL_INVALID_OPERATION);
        glUniform3f(-1, 0.0f, 0.0f, 0.0f);
        expectError(GL_INVALID_OPERATION);
        glUniform4f(-1, 0.0f, 0.0f, 0.0f, 0.0f);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(uniformf_incompatible_type, "Invalid glUniform{1234}f() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        GLint vec4_v    = glGetUniformLocation(program.getProgram(), "vec4_v");    // vec4
        GLint ivec4_f   = glGetUniformLocation(program.getProgram(), "ivec4_f");   // ivec4
        GLint uvec4_f   = glGetUniformLocation(program.getProgram(), "uvec4_f");   // uvec4
        GLint sampler_f = glGetUniformLocation(program.getProgram(), "sampler_f"); // sampler2D
        expectError(GL_NO_ERROR);

        if (vec4_v == -1 || ivec4_f == -1 || uvec4_f == -1 || sampler_f == -1)
        {
            m_log << TestLog::Message << "// ERROR: Failed to retrieve uniform location" << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");
        }

        m_log << tcu::TestLog::Section("",
                                       "GL_INVALID_OPERATION is generated if the size of the uniform variable declared "
                                       "in the shader does not match the size indicated by the glUniform command.");
        glUseProgram(program.getProgram());
        glUniform1f(vec4_v, 0.0f);
        expectError(GL_INVALID_OPERATION);
        glUniform2f(vec4_v, 0.0f, 0.0f);
        expectError(GL_INVALID_OPERATION);
        glUniform3f(vec4_v, 0.0f, 0.0f, 0.0f);
        expectError(GL_INVALID_OPERATION);
        glUniform4f(vec4_v, 0.0f, 0.0f, 0.0f, 0.0f);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if glUniform{1234}f is used to load a uniform variable of type int, "
                "ivec2, ivec3, ivec4, unsigned int, uvec2, uvec3, uvec4.");
        glUseProgram(program.getProgram());
        glUniform4f(ivec4_f, 0.0f, 0.0f, 0.0f, 0.0f);
        expectError(GL_INVALID_OPERATION);
        glUniform4f(uvec4_f, 0.0f, 0.0f, 0.0f, 0.0f);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if a sampler is loaded using a command "
                                           "other than glUniform1i and glUniform1iv.");
        glUseProgram(program.getProgram());
        glUniform1f(sampler_f, 0.0f);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(uniformf_invalid_location, "Invalid glUniform{1234}f() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if location is an invalid uniform "
                                           "location for the current program object and location is not equal to -1.");
        glUseProgram(program.getProgram());
        glUniform1f(-2, 0.0f);
        expectError(GL_INVALID_OPERATION);
        glUniform2f(-2, 0.0f, 0.0f);
        expectError(GL_INVALID_OPERATION);
        glUniform3f(-2, 0.0f, 0.0f, 0.0f);
        expectError(GL_INVALID_OPERATION);
        glUniform4f(-2, 0.0f, 0.0f, 0.0f, 0.0f);
        expectError(GL_INVALID_OPERATION);

        glUseProgram(program.getProgram());
        glUniform1f(-1, 0.0f);
        expectError(GL_NO_ERROR);
        glUniform2f(-1, 0.0f, 0.0f);
        expectError(GL_NO_ERROR);
        glUniform3f(-1, 0.0f, 0.0f, 0.0f);
        expectError(GL_NO_ERROR);
        glUniform4f(-1, 0.0f, 0.0f, 0.0f, 0.0f);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });

    // glUniform*fv

    ES3F_ADD_API_CASE(uniformfv_invalid_program, "Invalid glUniform{1234}fv() usage", {
        std::vector<GLfloat> data(4);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if there is no current program object.");
        glUseProgram(0);
        glUniform1fv(-1, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2fv(-1, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3fv(-1, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4fv(-1, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(uniformfv_incompatible_type, "Invalid glUniform{1234}fv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        GLint vec4_v    = glGetUniformLocation(program.getProgram(), "vec4_v");    // vec4
        GLint ivec4_f   = glGetUniformLocation(program.getProgram(), "ivec4_f");   // ivec4
        GLint uvec4_f   = glGetUniformLocation(program.getProgram(), "uvec4_f");   // uvec4
        GLint sampler_f = glGetUniformLocation(program.getProgram(), "sampler_f"); // sampler2D
        expectError(GL_NO_ERROR);

        if (vec4_v == -1 || ivec4_f == -1 || uvec4_f == -1 || sampler_f == -1)
        {
            m_log << TestLog::Message << "// ERROR: Failed to retrieve uniform location" << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");
        }

        std::vector<GLfloat> data(4);

        m_log << tcu::TestLog::Section("",
                                       "GL_INVALID_OPERATION is generated if the size of the uniform variable declared "
                                       "in the shader does not match the size indicated by the glUniform command.");
        glUseProgram(program.getProgram());
        glUniform1fv(vec4_v, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2fv(vec4_v, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3fv(vec4_v, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4fv(vec4_v, 1, &data[0]);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if glUniform{1234}fv is used to load a uniform variable of type "
                "int, ivec2, ivec3, ivec4, unsigned int, uvec2, uvec3, uvec4.");
        glUseProgram(program.getProgram());
        glUniform4fv(ivec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4fv(uvec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if a sampler is loaded using a command "
                                           "other than glUniform1i and glUniform1iv.");
        glUseProgram(program.getProgram());
        glUniform1fv(sampler_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(uniformfv_invalid_location, "Invalid glUniform{1234}fv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        std::vector<GLfloat> data(4);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if location is an invalid uniform "
                                           "location for the current program object and location is not equal to -1.");
        glUseProgram(program.getProgram());
        glUniform1fv(-2, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2fv(-2, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3fv(-2, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4fv(-2, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);

        glUseProgram(program.getProgram());
        glUniform1fv(-1, 1, &data[0]);
        expectError(GL_NO_ERROR);
        glUniform2fv(-1, 1, &data[0]);
        expectError(GL_NO_ERROR);
        glUniform3fv(-1, 1, &data[0]);
        expectError(GL_NO_ERROR);
        glUniform4fv(-1, 1, &data[0]);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(uniformfv_invalid_count, "Invalid glUniform{1234}fv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        GLint vec4_v = glGetUniformLocation(program.getProgram(), "vec4_v"); // vec4
        expectError(GL_NO_ERROR);

        if (vec4_v == -1)
        {
            m_log << TestLog::Message << "// ERROR: Failed to retrieve uniform location" << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");
        }

        std::vector<GLfloat> data(8);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if count is greater than 1 and the "
                                           "indicated uniform variable is not an array variable.");
        glUseProgram(program.getProgram());
        glUniform1fv(vec4_v, 2, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2fv(vec4_v, 2, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3fv(vec4_v, 2, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4fv(vec4_v, 2, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });

    // glUniform*i

    ES3F_ADD_API_CASE(uniformi_invalid_program, "Invalid glUniform{1234}i() usage", {
        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if there is no current program object.");
        glUseProgram(0);
        glUniform1i(-1, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform2i(-1, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform3i(-1, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform4i(-1, 0, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(uniformi_incompatible_type, "Invalid glUniform{1234}i() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        GLint vec4_v    = glGetUniformLocation(program.getProgram(), "vec4_v");    // vec4
        GLint ivec4_f   = glGetUniformLocation(program.getProgram(), "ivec4_f");   // ivec4
        GLint uvec4_f   = glGetUniformLocation(program.getProgram(), "uvec4_f");   // uvec4
        GLint sampler_f = glGetUniformLocation(program.getProgram(), "sampler_f"); // sampler2D
        expectError(GL_NO_ERROR);

        if (vec4_v == -1 || ivec4_f == -1 || uvec4_f == -1 || sampler_f == -1)
        {
            m_log << TestLog::Message << "// ERROR: Failed to retrieve uniform location" << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");
        }

        m_log << tcu::TestLog::Section("",
                                       "GL_INVALID_OPERATION is generated if the size of the uniform variable declared "
                                       "in the shader does not match the size indicated by the glUniform command.");
        glUseProgram(program.getProgram());
        glUniform1i(ivec4_f, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform2i(ivec4_f, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform3i(ivec4_f, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform4i(ivec4_f, 0, 0, 0, 0);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if glUniform{1234}i is used to load a uniform variable of type "
                "unsigned int, uvec2, uvec3, uvec4, or an array of these.");
        glUseProgram(program.getProgram());
        glUniform1i(uvec4_f, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform2i(uvec4_f, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform3i(uvec4_f, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform4i(uvec4_f, 0, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if glUniform{1234}i is used to load a "
                                           "uniform variable of type float, vec2, vec3, or vec4.");
        glUseProgram(program.getProgram());
        glUniform1i(vec4_v, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform2i(vec4_v, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform3i(vec4_v, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform4i(vec4_v, 0, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(uniformi_invalid_location, "Invalid glUniform{1234}i() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if location is an invalid uniform "
                                           "location for the current program object and location is not equal to -1.");
        glUseProgram(program.getProgram());
        glUniform1i(-2, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform2i(-2, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform3i(-2, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform4i(-2, 0, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);

        glUseProgram(program.getProgram());
        glUniform1i(-1, 0);
        expectError(GL_NO_ERROR);
        glUniform2i(-1, 0, 0);
        expectError(GL_NO_ERROR);
        glUniform3i(-1, 0, 0, 0);
        expectError(GL_NO_ERROR);
        glUniform4i(-1, 0, 0, 0, 0);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });

    // glUniform*iv

    ES3F_ADD_API_CASE(uniformiv_invalid_program, "Invalid glUniform{1234}iv() usage", {
        std::vector<GLint> data(4);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if there is no current program object.");
        glUseProgram(0);
        glUniform1iv(-1, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2iv(-1, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3iv(-1, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4iv(-1, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(uniformiv_incompatible_type, "Invalid glUniform{1234}iv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        GLint vec4_v    = glGetUniformLocation(program.getProgram(), "vec4_v");    // vec4
        GLint ivec4_f   = glGetUniformLocation(program.getProgram(), "ivec4_f");   // ivec4
        GLint uvec4_f   = glGetUniformLocation(program.getProgram(), "uvec4_f");   // uvec4
        GLint sampler_f = glGetUniformLocation(program.getProgram(), "sampler_f"); // sampler2D
        expectError(GL_NO_ERROR);

        if (vec4_v == -1 || ivec4_f == -1 || uvec4_f == -1 || sampler_f == -1)
        {
            m_log << TestLog::Message << "// ERROR: Failed to retrieve uniform location" << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");
        }

        std::vector<GLint> data(4);

        m_log << tcu::TestLog::Section("",
                                       "GL_INVALID_OPERATION is generated if the size of the uniform variable declared "
                                       "in the shader does not match the size indicated by the glUniform command.");
        glUseProgram(program.getProgram());
        glUniform1iv(ivec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2iv(ivec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3iv(ivec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4iv(ivec4_f, 1, &data[0]);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if glUniform{1234}iv is used to load a "
                                           "uniform variable of type float, vec2, vec3, or vec4.");
        glUseProgram(program.getProgram());
        glUniform1iv(vec4_v, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2iv(vec4_v, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3iv(vec4_v, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4iv(vec4_v, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if glUniform{1234}iv is used to load a "
                                           "uniform variable of type unsigned int, uvec2, uvec3 or uvec4.");
        glUseProgram(program.getProgram());
        glUniform1iv(uvec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2iv(uvec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3iv(uvec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4iv(uvec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(uniformiv_invalid_location, "Invalid glUniform{1234}iv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        std::vector<GLint> data(4);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if location is an invalid uniform "
                                           "location for the current program object and location is not equal to -1.");
        glUseProgram(program.getProgram());
        glUniform1iv(-2, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2iv(-2, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3iv(-2, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4iv(-2, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);

        glUseProgram(program.getProgram());
        glUniform1iv(-1, 1, &data[0]);
        expectError(GL_NO_ERROR);
        glUniform2iv(-1, 1, &data[0]);
        expectError(GL_NO_ERROR);
        glUniform3iv(-1, 1, &data[0]);
        expectError(GL_NO_ERROR);
        glUniform4iv(-1, 1, &data[0]);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(uniformiv_invalid_count, "Invalid glUniform{1234}iv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        GLint ivec4_f = glGetUniformLocation(program.getProgram(), "ivec4_f"); // ivec4
        expectError(GL_NO_ERROR);

        if (ivec4_f == -1)
        {
            m_log << TestLog::Message << "// ERROR: Failed to retrieve uniform location" << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");
        }

        std::vector<GLint> data(8);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if count is greater than 1 and the "
                                           "indicated uniform variable is not an array variable.");
        glUseProgram(program.getProgram());
        glUniform1iv(ivec4_f, 2, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2iv(ivec4_f, 2, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3iv(ivec4_f, 2, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4iv(ivec4_f, 2, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });

    // glUniform{1234}ui

    ES3F_ADD_API_CASE(uniformui_invalid_program, "Invalid glUniform{234}ui() usage", {
        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if there is no current program object.");
        glUseProgram(0);
        glUniform1ui(-1, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform2ui(-1, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform3ui(-1, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform4ui(-1, 0, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(uniformui_incompatible_type, "Invalid glUniform{1234}ui() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        GLint vec4_v    = glGetUniformLocation(program.getProgram(), "vec4_v");    // vec4
        GLint ivec4_f   = glGetUniformLocation(program.getProgram(), "ivec4_f");   // ivec4
        GLint uvec4_f   = glGetUniformLocation(program.getProgram(), "uvec4_f");   // uvec4
        GLint sampler_f = glGetUniformLocation(program.getProgram(), "sampler_f"); // sampler2D
        expectError(GL_NO_ERROR);

        if (vec4_v == -1 || ivec4_f == -1 || uvec4_f == -1 || sampler_f == -1)
        {
            m_log << TestLog::Message << "// ERROR: Failed to retrieve uniform location" << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");
        }

        m_log << tcu::TestLog::Section("",
                                       "GL_INVALID_OPERATION is generated if the size of the uniform variable declared "
                                       "in the shader does not match the size indicated by the glUniform command.");
        glUseProgram(program.getProgram());
        glUniform1ui(uvec4_f, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform2ui(uvec4_f, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform3ui(uvec4_f, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform4ui(uvec4_f, 0, 0, 0, 0);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if glUniform{1234}i is used to load a "
                                           "uniform variable of type int, ivec2, ivec3, ivec4, or an array of these.");
        glUseProgram(program.getProgram());
        glUniform1ui(ivec4_f, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform2ui(ivec4_f, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform3ui(ivec4_f, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform4ui(ivec4_f, 0, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if glUniform{1234}i is used to load a "
                                           "uniform variable of type float, vec2, vec3, or vec4.");
        glUseProgram(program.getProgram());
        glUniform1ui(vec4_v, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform2ui(vec4_v, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform3ui(vec4_v, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform4ui(vec4_v, 0, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if a sampler is loaded using a command "
                                           "other than glUniform1i and glUniform1iv.");
        glUseProgram(program.getProgram());
        glUniform1ui(sampler_f, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(uniformui_invalid_location, "Invalid glUniform{1234}ui() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if location is an invalid uniform "
                                           "location for the current program object and location is not equal to -1.");
        glUseProgram(program.getProgram());
        glUniform1i(-2, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform2i(-2, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform3i(-2, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);
        glUniform4i(-2, 0, 0, 0, 0);
        expectError(GL_INVALID_OPERATION);

        glUseProgram(program.getProgram());
        glUniform1i(-1, 0);
        expectError(GL_NO_ERROR);
        glUniform2i(-1, 0, 0);
        expectError(GL_NO_ERROR);
        glUniform3i(-1, 0, 0, 0);
        expectError(GL_NO_ERROR);
        glUniform4i(-1, 0, 0, 0, 0);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });

    // glUniform{1234}uiv

    ES3F_ADD_API_CASE(uniformuiv_invalid_program, "Invalid glUniform{234}uiv() usage", {
        std::vector<GLuint> data(4);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if there is no current program object.");
        glUseProgram(0);
        glUniform1uiv(-1, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2uiv(-1, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3uiv(-1, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4uiv(-1, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(uniformuiv_incompatible_type, "Invalid glUniform{1234}uiv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        GLint vec4_v    = glGetUniformLocation(program.getProgram(), "vec4_v");    // vec4
        GLint ivec4_f   = glGetUniformLocation(program.getProgram(), "ivec4_f");   // ivec4
        GLint uvec4_f   = glGetUniformLocation(program.getProgram(), "uvec4_f");   // uvec4
        GLint sampler_f = glGetUniformLocation(program.getProgram(), "sampler_f"); // sampler2D
        expectError(GL_NO_ERROR);

        if (vec4_v == -1 || ivec4_f == -1 || uvec4_f == -1 || sampler_f == -1)
        {
            m_log << TestLog::Message << "// ERROR: Failed to retrieve uniform location" << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");
        }

        std::vector<GLuint> data(4);

        m_log << tcu::TestLog::Section("",
                                       "GL_INVALID_OPERATION is generated if the size of the uniform variable declared "
                                       "in the shader does not match the size indicated by the glUniform command.");
        glUseProgram(program.getProgram());
        glUniform1uiv(uvec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2uiv(uvec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3uiv(uvec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4uiv(uvec4_f, 1, &data[0]);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if glUniform{1234}uiv is used to load a "
                                           "uniform variable of type float, vec2, vec3, or vec4.");
        glUseProgram(program.getProgram());
        glUniform1uiv(vec4_v, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2uiv(vec4_v, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3uiv(vec4_v, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4uiv(vec4_v, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if glUniform{1234}uiv is used to load a "
                                           "uniform variable of type int, ivec2, ivec3 or ivec4.");
        glUseProgram(program.getProgram());
        glUniform1uiv(ivec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2uiv(ivec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3uiv(ivec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4uiv(ivec4_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if a sampler is loaded using a command "
                                           "other than glUniform1i and glUniform1iv.");
        glUseProgram(program.getProgram());
        glUniform1uiv(sampler_f, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(uniformuiv_invalid_location, "Invalid glUniform{1234}uiv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        std::vector<GLuint> data(4);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if location is an invalid uniform "
                                           "location for the current program object and location is not equal to -1.");
        glUseProgram(program.getProgram());
        glUniform1uiv(-2, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2uiv(-2, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3uiv(-2, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4uiv(-2, 1, &data[0]);
        expectError(GL_INVALID_OPERATION);

        glUseProgram(program.getProgram());
        glUniform1uiv(-1, 1, &data[0]);
        expectError(GL_NO_ERROR);
        glUniform2uiv(-1, 1, &data[0]);
        expectError(GL_NO_ERROR);
        glUniform3uiv(-1, 1, &data[0]);
        expectError(GL_NO_ERROR);
        glUniform4uiv(-1, 1, &data[0]);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(uniformuiv_invalid_count, "Invalid glUniform{1234}uiv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        int uvec4_f = glGetUniformLocation(program.getProgram(), "uvec4_f"); // uvec4
        expectError(GL_NO_ERROR);

        if (uvec4_f == -1)
        {
            m_log << TestLog::Message << "// ERROR: Failed to retrieve uniform location" << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");
        }

        std::vector<GLuint> data(8);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if count is greater than 1 and the "
                                           "indicated uniform variable is not an array variable.");
        glUseProgram(program.getProgram());
        glUniform1uiv(uvec4_f, 2, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform2uiv(uvec4_f, 2, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform3uiv(uvec4_f, 2, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniform4uiv(uvec4_f, 2, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });

    // glUniformMatrix*fv

    ES3F_ADD_API_CASE(uniform_matrixfv_invalid_program, "Invalid glUniformMatrix{234}fv() usage", {
        std::vector<GLfloat> data(16);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if there is no current program object.");
        glUseProgram(0);
        glUniformMatrix2fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);

        glUniformMatrix2x3fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3x2fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix2x4fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4x2fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3x4fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4x3fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(uniform_matrixfv_incompatible_type, "Invalid glUniformMatrix{234}fv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        GLint mat4_v    = glGetUniformLocation(program.getProgram(), "mat4_v");    // mat4
        GLint sampler_f = glGetUniformLocation(program.getProgram(), "sampler_f"); // sampler2D
        expectError(GL_NO_ERROR);

        if (mat4_v == -1 || sampler_f == -1)
        {
            m_log << TestLog::Message << "// ERROR: Failed to retrieve uniform location" << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");
        }

        std::vector<GLfloat> data(16);

        m_log << tcu::TestLog::Section("",
                                       "GL_INVALID_OPERATION is generated if the size of the uniform variable declared "
                                       "in the shader does not match the size indicated by the glUniform command.");
        glUseProgram(program.getProgram());
        glUniformMatrix2fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_NO_ERROR);

        glUniformMatrix2x3fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3x2fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix2x4fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4x2fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3x4fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4x3fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if a sampler is loaded using a command "
                                           "other than glUniform1i and glUniform1iv.");
        glUseProgram(program.getProgram());
        glUniformMatrix2fv(sampler_f, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3fv(sampler_f, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4fv(sampler_f, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);

        glUniformMatrix2x3fv(sampler_f, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3x2fv(sampler_f, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix2x4fv(sampler_f, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4x2fv(sampler_f, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3x4fv(sampler_f, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4x3fv(sampler_f, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(uniform_matrixfv_invalid_location, "Invalid glUniformMatrix{234}fv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        std::vector<GLfloat> data(16);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if location is an invalid uniform "
                                           "location for the current program object and location is not equal to -1.");
        glUseProgram(program.getProgram());
        glUniformMatrix2fv(-2, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3fv(-2, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4fv(-2, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);

        glUniformMatrix2x3fv(-2, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3x2fv(-2, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix2x4fv(-2, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4x2fv(-2, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3x4fv(-2, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4x3fv(-2, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);

        glUseProgram(program.getProgram());
        glUniformMatrix2fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_NO_ERROR);
        glUniformMatrix3fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_NO_ERROR);
        glUniformMatrix4fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_NO_ERROR);

        glUniformMatrix2x3fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_NO_ERROR);
        glUniformMatrix3x2fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_NO_ERROR);
        glUniformMatrix2x4fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_NO_ERROR);
        glUniformMatrix4x2fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_NO_ERROR);
        glUniformMatrix3x4fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_NO_ERROR);
        glUniformMatrix4x3fv(-1, 1, GL_FALSE, &data[0]);
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });
    ES3F_ADD_API_CASE(uniform_matrixfv_invalid_count, "Invalid glUniformMatrix{234}fv() usage", {
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(uniformTestVertSource, uniformTestFragSource));

        glUseProgram(program.getProgram());
        GLint mat4_v = glGetUniformLocation(program.getProgram(), "mat4_v"); // mat4
        expectError(GL_NO_ERROR);

        if (mat4_v == -1)
        {
            m_log << TestLog::Message << "// ERROR: Failed to retrieve uniform location" << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Failed to retrieve uniform location");
        }

        std::vector<GLfloat> data(32);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if count is greater than 1 and the "
                                           "indicated uniform variable is not an array variable.");
        glUseProgram(program.getProgram());
        glUniformMatrix2fv(mat4_v, 2, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3fv(mat4_v, 2, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4fv(mat4_v, 2, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);

        glUniformMatrix2x3fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3x2fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix2x4fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4x2fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix3x4fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        glUniformMatrix4x3fv(mat4_v, 1, GL_FALSE, &data[0]);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
    });

    // Transform feedback

    ES3F_ADD_API_CASE(gen_transform_feedbacks, "Invalid glGenTransformFeedbacks() usage", {
        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        GLuint id;
        glGenTransformFeedbacks(-1, &id);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(bind_transform_feedback, "Invalid glBindTransformFeedback() usage", {
        GLuint tfID[2];
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        uint32_t buf;
        const char *tfVarying = "gl_Position";

        glGenBuffers(1, &buf);
        glGenTransformFeedbacks(2, tfID);

        m_log << tcu::TestLog::Section("", "GL_INVALID_ENUM is generated if target is not GL_TRANSFORM_FEEDBACK.");
        glBindTransformFeedback(-1, tfID[0]);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("",
                                       "GL_INVALID_OPERATION is generated if the transform feedback operation is "
                                       "active on the currently bound transform feedback object, and is not paused.");
        glUseProgram(program.getProgram());
        glTransformFeedbackVaryings(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(program.getProgram());
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID[0]);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
        glBeginTransformFeedback(GL_TRIANGLES);
        expectError(GL_NO_ERROR);

        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID[1]);
        expectError(GL_INVALID_OPERATION);

        glEndTransformFeedback();
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        glUseProgram(0);
        glDeleteBuffers(1, &buf);
        glDeleteTransformFeedbacks(2, tfID);
        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(delete_transform_feedbacks, "Invalid glDeleteTransformFeedbacks() usage", {
        GLuint id;
        glGenTransformFeedbacks(1, &id);

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glDeleteTransformFeedbacks(-1, &id);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        glDeleteTransformFeedbacks(1, &id);
    });
    ES3F_ADD_API_CASE(begin_transform_feedback, "Invalid glBeginTransformFeedback() usage", {
        GLuint tfID[2];
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        uint32_t buf;
        const char *tfVarying = "gl_Position";

        glGenBuffers(1, &buf);
        glGenTransformFeedbacks(2, tfID);

        glUseProgram(program.getProgram());
        glTransformFeedbackVaryings(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(program.getProgram());
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID[0]);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_ENUM is generated if primitiveMode is not one of GL_POINTS, GL_LINES, or GL_TRIANGLES.");
        glBeginTransformFeedback(-1);
        expectError(GL_INVALID_ENUM);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("",
                                       "GL_INVALID_OPERATION is generated if transform feedback is already active.");
        glBeginTransformFeedback(GL_TRIANGLES);
        expectError(GL_NO_ERROR);
        glBeginTransformFeedback(GL_POINTS);
        expectError(GL_INVALID_OPERATION);
        glEndTransformFeedback();
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if any binding point used in transform "
                                           "feedback mode does not have a buffer object bound.");
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
        glBeginTransformFeedback(GL_TRIANGLES);
        expectError(GL_INVALID_OPERATION);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if no binding points would be used "
                                           "because no program object is active.");
        glUseProgram(0);
        glBeginTransformFeedback(GL_TRIANGLES);
        expectError(GL_INVALID_OPERATION);
        glUseProgram(program.getProgram());
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("",
                                       "GL_INVALID_OPERATION is generated if no binding points would be used because "
                                       "the active program object has specified no varying variables to record.");
        glTransformFeedbackVaryings(program.getProgram(), 0, 0, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(program.getProgram());
        expectError(GL_NO_ERROR);
        glBeginTransformFeedback(GL_TRIANGLES);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glDeleteBuffers(1, &buf);
        glDeleteTransformFeedbacks(2, tfID);
        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(pause_transform_feedback, "Invalid glPauseTransformFeedback() usage", {
        GLuint tfID[2];
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        uint32_t buf;
        const char *tfVarying = "gl_Position";

        glGenBuffers(1, &buf);
        glGenTransformFeedbacks(2, tfID);

        glUseProgram(program.getProgram());
        glTransformFeedbackVaryings(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(program.getProgram());
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID[0]);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if the currently bound transform "
                                           "feedback object is not active or is paused.");
        glPauseTransformFeedback();
        expectError(GL_INVALID_OPERATION);
        glBeginTransformFeedback(GL_TRIANGLES);
        glPauseTransformFeedback();
        expectError(GL_NO_ERROR);
        glPauseTransformFeedback();
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glEndTransformFeedback();
        glDeleteBuffers(1, &buf);
        glDeleteTransformFeedbacks(2, tfID);
        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(resume_transform_feedback, "Invalid glResumeTransformFeedback() usage", {
        GLuint tfID[2];
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        uint32_t buf;
        const char *tfVarying = "gl_Position";

        glGenBuffers(1, &buf);
        glGenTransformFeedbacks(2, tfID);

        glUseProgram(program.getProgram());
        glTransformFeedbackVaryings(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(program.getProgram());
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID[0]);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 32, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if the currently bound transform "
                                           "feedback object is not active or is not paused.");
        glResumeTransformFeedback();
        expectError(GL_INVALID_OPERATION);
        glBeginTransformFeedback(GL_TRIANGLES);
        glResumeTransformFeedback();
        expectError(GL_INVALID_OPERATION);
        glPauseTransformFeedback();
        glResumeTransformFeedback();
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        glEndTransformFeedback();
        glDeleteBuffers(1, &buf);
        glDeleteTransformFeedbacks(2, tfID);
        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(end_transform_feedback, "Invalid glEndTransformFeedback() usage", {
        GLuint tfID;
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        uint32_t buf;
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
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if transform feedback is not active.");
        glEndTransformFeedback();
        expectError(GL_INVALID_OPERATION);
        glBeginTransformFeedback(GL_TRIANGLES);
        glEndTransformFeedback();
        expectError(GL_NO_ERROR);
        m_log << tcu::TestLog::EndSection;

        glDeleteBuffers(1, &buf);
        glDeleteTransformFeedbacks(1, &tfID);
        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(get_transform_feedback_varying, "Invalid glGetTransformFeedbackVarying() usage", {
        GLuint tfID;
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        glu::ShaderProgram programInvalid(m_context.getRenderContext(),
                                          glu::makeVtxFragSources(vertexShaderSource, ""));
        const char *tfVarying            = "gl_Position";
        int maxTransformFeedbackVaryings = 0;

        GLsizei length;
        GLsizei size;
        GLenum type;
        char name[32];

        glGenTransformFeedbacks(1, &tfID);

        glTransformFeedbackVaryings(program.getProgram(), 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
        expectError(GL_NO_ERROR);
        glLinkProgram(program.getProgram());
        expectError(GL_NO_ERROR);

        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfID);
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("",
                                       "GL_INVALID_VALUE is generated if program is not the name of a program object.");
        glGetTransformFeedbackVarying(-1, 0, 32, &length, &size, &type, &name[0]);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if index is greater or equal to the value of "
                                           "GL_TRANSFORM_FEEDBACK_VARYINGS.");
        glGetProgramiv(program.getProgram(), GL_TRANSFORM_FEEDBACK_VARYINGS, &maxTransformFeedbackVaryings);
        glGetTransformFeedbackVarying(program.getProgram(), maxTransformFeedbackVaryings, 32, &length, &size, &type,
                                      &name[0]);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_OPERATION or GL_INVALID_VALUE is generated program has not been linked.");
        glGetTransformFeedbackVarying(programInvalid.getProgram(), 0, 32, &length, &size, &type, &name[0]);
        expectError(GL_INVALID_OPERATION, GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        glDeleteTransformFeedbacks(1, &tfID);
        expectError(GL_NO_ERROR);
    });
    ES3F_ADD_API_CASE(transform_feedback_varyings, "Invalid glTransformFeedbackVaryings() usage", {
        GLuint tfID;
        glu::ShaderProgram program(m_context.getRenderContext(),
                                   glu::makeVtxFragSources(vertexShaderSource, fragmentShaderSource));
        const char *tfVarying                     = "gl_Position";
        GLint maxTransformFeedbackSeparateAttribs = 0;

        glGenTransformFeedbacks(1, &tfID);
        expectError(GL_NO_ERROR);

        m_log << tcu::TestLog::Section("",
                                       "GL_INVALID_VALUE is generated if program is not the name of a program object.");
        glTransformFeedbackVaryings(0, 1, &tfVarying, GL_INTERLEAVED_ATTRIBS);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_VALUE is generated if bufferMode is GL_SEPARATE_ATTRIBS and "
                                           "count is greater than GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS.");
        glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &maxTransformFeedbackSeparateAttribs);
        glTransformFeedbackVaryings(program.getProgram(), maxTransformFeedbackSeparateAttribs + 1, &tfVarying,
                                    GL_SEPARATE_ATTRIBS);
        expectError(GL_INVALID_VALUE);
        m_log << tcu::TestLog::EndSection;

        glDeleteTransformFeedbacks(1, &tfID);
        expectError(GL_NO_ERROR);
    });
}

void ApiCaseRedeclaringGlFragDepth::test(void)
{
    m_log << TestLog::Section("", "gl_FragDepth redeclaration");
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

    glu::ShaderProgram program(m_context.getRenderContext(),
                               glu::makeVtxFragSources(vertexShaderSource, glFragDepthRedeclarationFragSource));

    auto isOkProgram                  = program.isOk();
    auto conservative_depth_supported = m_context.getContextInfo().isExtensionSupported("GL_EXT_conservative_depth");
    auto at_least_es30 = glu::contextSupports(m_context.getRenderContext().getType(), glu::ApiType::es(3, 0));

    if (at_least_es30)
    {
        if (conservative_depth_supported)
        {
            if (isOkProgram)
            {
                m_log << TestLog::Section(
                    "", "GL_EXT_conservative_depth extension is supported, gl_FragDepth redeclaration is allowed");
                m_log << TestLog::EndSection;
            }
            else
            {
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail, fragment shader compile/link error");
            }
        }
        else if (!conservative_depth_supported && !isOkProgram)
        {
            m_log << TestLog::Section(
                "", "GL_EXT_conservative_depth extension is not supported, gl_FragDepth redeclaration is not allowed");
            m_log << TestLog::EndSection;
        }
        else
        {
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL,
                                    "Fail, GL_EXT_conservative_depth extension is not supported; gl_FragDepth "
                                    "redeclaration must cause a compile failure");
        }
    }
    else
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
    }

    m_log << TestLog::EndSection;
}

void ApiCaseRedeclaringGlFragDepthExtensionNotEnabled::test(void)
{
    m_log << TestLog::Section("", "gl_FragDepth redeclaration without extension enabled in shader");
    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");

    glu::ShaderProgram program(
        m_context.getRenderContext(),
        glu::makeVtxFragSources(vertexShaderSource, glFragDepthRedeclarationFragExtNotEnabledSource));

    auto isOkProgram                  = program.isOk();
    auto conservative_depth_supported = m_context.getContextInfo().isExtensionSupported("GL_EXT_conservative_depth");
    auto at_least_es30 = glu::contextSupports(m_context.getRenderContext().getType(), glu::ApiType::es(3, 0));

    if (at_least_es30)
    {
        if (conservative_depth_supported)
        {
            if (!isOkProgram)
            {
                m_log << TestLog::Section("", "GL_EXT_conservative_depth extension is supported but not enabled in "
                                              "shader, gl_FragDepth redeclaration is not allowed");
                m_log << TestLog::EndSection;
            }
            else
            {
                m_testCtx.setTestResult(QP_TEST_RESULT_FAIL,
                                        "Fail, GL_EXT_conservative_depth extension is supported and not enabled in "
                                        "shader. There should be compile/link error");
            }
        }
        else if (!conservative_depth_supported && !isOkProgram)
        {
            m_log << TestLog::Section("", "GL_EXT_conservative_depth extension is not supported and not enabled in "
                                          "shader, gl_FragDepth redeclaration is not allowed");
            m_log << TestLog::EndSection;
        }
        else
        {
            m_testCtx.setTestResult(
                QP_TEST_RESULT_FAIL,
                "Fail, GL_EXT_conservative_depth extension is not supported and not enabled in shader; "
                "gl_FragDepth redeclaration must cause a compile failure");
        }
    }
    else
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Not supported");
    }

    m_log << TestLog::EndSection;
}
//};

} // namespace Functional
} // namespace gles3
} // namespace deqp
