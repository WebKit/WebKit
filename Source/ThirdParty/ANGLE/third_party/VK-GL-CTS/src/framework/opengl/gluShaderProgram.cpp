/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief Wrapper for GL program object.
 *//*--------------------------------------------------------------------*/

#include "gluShaderProgram.hpp"
#include "gluRenderContext.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "tcuTestLog.hpp"
#include "deClock.h"

#include <cstring>

using std::string;

namespace glu
{

// Shader

Shader::Shader(const RenderContext &renderCtx, ShaderType shaderType) : m_gl(renderCtx.getFunctions()), m_shader(0)
{
    m_info.type = shaderType;
    m_shader    = m_gl.createShader(getGLShaderType(shaderType));
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glCreateShader()");
    TCU_CHECK(m_shader);
}

Shader::Shader(const glw::Functions &gl, ShaderType shaderType) : m_gl(gl), m_shader(0)
{
    m_info.type = shaderType;
    m_shader    = m_gl.createShader(getGLShaderType(shaderType));
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glCreateShader()");
    TCU_CHECK(m_shader);
}

Shader::~Shader(void)
{
    m_gl.deleteShader(m_shader);
}

void Shader::setSources(int numSourceStrings, const char *const *sourceStrings, const int *lengths)
{
    m_gl.shaderSource(m_shader, numSourceStrings, sourceStrings, lengths);
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glShaderSource()");

    m_info.source.clear();
    for (int ndx = 0; ndx < numSourceStrings; ndx++)
    {
        const size_t length = lengths && lengths[ndx] >= 0 ? lengths[ndx] : strlen(sourceStrings[ndx]);
        m_info.source += std::string(sourceStrings[ndx], length);
    }
}

void Shader::compile(void)
{
    m_info.compileOk     = false;
    m_info.compileTimeUs = 0;
    m_info.infoLog.clear();

    {
        uint64_t compileStart = deGetMicroseconds();
        m_gl.compileShader(m_shader);
        m_info.compileTimeUs = deGetMicroseconds() - compileStart;
    }

    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glCompileShader()");

    // Query status
    {
        int compileStatus = 0;

        m_gl.getShaderiv(m_shader, GL_COMPILE_STATUS, &compileStatus);
        GLU_EXPECT_NO_ERROR(m_gl.getError(), "glGetShaderiv()");

        m_info.compileOk = compileStatus != GL_FALSE;
    }

    // Query log
    {
        int infoLogLen = 0;
        int unusedLen;

        m_gl.getShaderiv(m_shader, GL_INFO_LOG_LENGTH, &infoLogLen);
        GLU_EXPECT_NO_ERROR(m_gl.getError(), "glGetShaderiv()");

        if (infoLogLen > 0)
        {
            // The INFO_LOG_LENGTH query and the buffer query implementations have
            // very commonly off-by-one errors. Try to work around these issues.

            // add tolerance for off-by-one in log length, buffer write, and for terminator
            std::vector<char> infoLog(infoLogLen + 3, '\0');

            // claim buf size is one smaller to protect from off-by-one writing over buffer bounds
            m_gl.getShaderInfoLog(m_shader, (int)infoLog.size() - 1, &unusedLen, &infoLog[0]);

            if (infoLog[(int)(infoLog.size()) - 1] != '\0')
            {
                // return whole buffer if null terminator was overwritten
                m_info.infoLog = std::string(&infoLog[0], infoLog.size());
            }
            else
            {
                // read as C string. infoLog is guaranteed to be 0-terminated
                m_info.infoLog = std::string(&infoLog[0]);
            }
        }
    }
}

void Shader::specialize(const char *entryPoint, glw::GLuint numSpecializationConstants,
                        const glw::GLuint *constantIndex, const glw::GLuint *constantValue)
{
    m_info.compileOk     = false;
    m_info.compileTimeUs = 0;
    m_info.infoLog.clear();

    {
        uint64_t compileStart = deGetMicroseconds();
        m_gl.specializeShader(m_shader, entryPoint, numSpecializationConstants, constantIndex, constantValue);
        m_info.compileTimeUs = deGetMicroseconds() - compileStart;
    }

    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glSpecializeShader()");

    // Query status
    {
        int compileStatus = 0;

        m_gl.getShaderiv(m_shader, GL_COMPILE_STATUS, &compileStatus);
        GLU_EXPECT_NO_ERROR(m_gl.getError(), "glGetShaderiv()");

        m_info.compileOk = compileStatus != GL_FALSE;
    }

    // Query log
    {
        int infoLogLen = 0;
        int unusedLen;

        m_gl.getShaderiv(m_shader, GL_INFO_LOG_LENGTH, &infoLogLen);
        GLU_EXPECT_NO_ERROR(m_gl.getError(), "glGetShaderiv()");

        if (infoLogLen > 0)
        {
            // The INFO_LOG_LENGTH query and the buffer query implementations have
            // very commonly off-by-one errors. Try to work around these issues.

            // add tolerance for off-by-one in log length, buffer write, and for terminator
            std::vector<char> infoLog(infoLogLen + 3, '\0');

            // claim buf size is one smaller to protect from off-by-one writing over buffer bounds
            m_gl.getShaderInfoLog(m_shader, (int)infoLog.size() - 1, &unusedLen, &infoLog[0]);
            GLU_EXPECT_NO_ERROR(m_gl.getError(), "glGetShaderInfoLog()");

            if (infoLog[(int)(infoLog.size()) - 1] != '\0')
            {
                // return whole buffer if null terminator was overwritten
                m_info.infoLog = std::string(&infoLog[0], infoLog.size());
            }
            else
            {
                // read as C string. infoLog is guaranteed to be 0-terminated
                m_info.infoLog = std::string(&infoLog[0]);
            }
        }
    }
}

// Program

static bool getProgramLinkStatus(const glw::Functions &gl, uint32_t program)
{
    int linkStatus = 0;

    gl.getProgramiv(program, GL_LINK_STATUS, &linkStatus);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetProgramiv()");
    return (linkStatus != GL_FALSE);
}

static std::string getProgramInfoLog(const glw::Functions &gl, uint32_t program)
{
    int infoLogLen = 0;
    int unusedLen;

    gl.getProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGetProgramiv()");

    if (infoLogLen > 0)
    {
        // The INFO_LOG_LENGTH query and the buffer query implementations have
        // very commonly off-by-one errors. Try to work around these issues.

        // add tolerance for off-by-one in log length, buffer write, and for terminator
        std::vector<char> infoLog(infoLogLen + 3, '\0');

        // claim buf size is one smaller to protect from off-by-one writing over buffer bounds
        gl.getProgramInfoLog(program, (int)infoLog.size() - 1, &unusedLen, &infoLog[0]);

        // return whole buffer if null terminator was overwritten
        if (infoLog[(int)(infoLog.size()) - 1] != '\0')
            return std::string(&infoLog[0], infoLog.size());

        // read as C string. infoLog is guaranteed to be 0-terminated
        return std::string(&infoLog[0]);
    }
    return std::string();
}

Program::Program(const RenderContext &renderCtx) : m_gl(renderCtx.getFunctions()), m_program(0)
{
    m_program = m_gl.createProgram();
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glCreateProgram()");
}

Program::Program(const glw::Functions &gl) : m_gl(gl), m_program(0)
{
    m_program = m_gl.createProgram();
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glCreateProgram()");
}

Program::Program(const RenderContext &renderCtx, uint32_t program) : m_gl(renderCtx.getFunctions()), m_program(program)
{
    m_info.linkOk  = getProgramLinkStatus(m_gl, program);
    m_info.infoLog = getProgramInfoLog(m_gl, program);
}

Program::~Program(void)
{
    m_gl.deleteProgram(m_program);
}

void Program::attachShader(uint32_t shader)
{
    m_gl.attachShader(m_program, shader);
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glAttachShader()");
}

void Program::detachShader(uint32_t shader)
{
    m_gl.detachShader(m_program, shader);
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glDetachShader()");
}

void Program::bindAttribLocation(uint32_t location, const char *name)
{
    m_gl.bindAttribLocation(m_program, location, name);
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glBindAttribLocation()");
}

void Program::transformFeedbackVaryings(int count, const char *const *varyings, uint32_t bufferMode)
{
    m_gl.transformFeedbackVaryings(m_program, count, varyings, bufferMode);
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glTransformFeedbackVaryings()");
}

void Program::link(void)
{
    m_info.linkOk     = false;
    m_info.linkTimeUs = 0;
    m_info.infoLog.clear();

    {
        uint64_t linkStart = deGetMicroseconds();
        m_gl.linkProgram(m_program);
        m_info.linkTimeUs = deGetMicroseconds() - linkStart;
    }
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glLinkProgram()");

    m_info.linkOk  = getProgramLinkStatus(m_gl, m_program);
    m_info.infoLog = getProgramInfoLog(m_gl, m_program);
}

bool Program::isSeparable(void) const
{
    int separable = GL_FALSE;

    m_gl.getProgramiv(m_program, GL_PROGRAM_SEPARABLE, &separable);
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glGetProgramiv()");

    return (separable != GL_FALSE);
}

void Program::setSeparable(bool separable)
{
    m_gl.programParameteri(m_program, GL_PROGRAM_SEPARABLE, separable);
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glProgramParameteri()");
}

// ProgramPipeline

ProgramPipeline::ProgramPipeline(const RenderContext &renderCtx) : m_gl(renderCtx.getFunctions()), m_pipeline(0)
{
    m_gl.genProgramPipelines(1, &m_pipeline);
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glGenProgramPipelines()");
}

ProgramPipeline::ProgramPipeline(const glw::Functions &gl) : m_gl(gl), m_pipeline(0)
{
    m_gl.genProgramPipelines(1, &m_pipeline);
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glGenProgramPipelines()");
}

ProgramPipeline::~ProgramPipeline(void)
{
    m_gl.deleteProgramPipelines(1, &m_pipeline);
}

void ProgramPipeline::useProgramStages(uint32_t stages, uint32_t program)
{
    m_gl.useProgramStages(m_pipeline, stages, program);
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glUseProgramStages()");
}

void ProgramPipeline::activeShaderProgram(uint32_t program)
{
    m_gl.activeShaderProgram(m_pipeline, program);
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glActiveShaderProgram()");
}

bool ProgramPipeline::isValid(void)
{
    glw::GLint status = GL_FALSE;
    m_gl.validateProgramPipeline(m_pipeline);
    GLU_EXPECT_NO_ERROR(m_gl.getError(), "glValidateProgramPipeline()");

    m_gl.getProgramPipelineiv(m_pipeline, GL_VALIDATE_STATUS, &status);

    return (status != GL_FALSE);
}

// ShaderProgram

ShaderProgram::ShaderProgram(const RenderContext &renderCtx, const ProgramSources &sources)
    : m_program(renderCtx.getFunctions())
{
    init(renderCtx.getFunctions(), sources);
}

ShaderProgram::ShaderProgram(const RenderContext &renderCtx, const ProgramBinaries &binaries)
    : m_program(renderCtx.getFunctions())
{
    init(renderCtx.getFunctions(), binaries);
}

ShaderProgram::ShaderProgram(const glw::Functions &gl, const ProgramSources &sources) : m_program(gl)
{
    init(gl, sources);
}

ShaderProgram::ShaderProgram(const glw::Functions &gl, const ProgramBinaries &binaries) : m_program(gl)
{
    init(gl, binaries);
}

void ShaderProgram::init(const glw::Functions &gl, const ProgramSources &sources)
{
    try
    {
        bool shadersOk = true;

        for (int shaderType = 0; shaderType < SHADERTYPE_LAST; shaderType++)
        {
            for (int shaderNdx = 0; shaderNdx < (int)sources.sources[shaderType].size(); ++shaderNdx)
            {
                const char *source = sources.sources[shaderType][shaderNdx].c_str();
                const int length   = (int)sources.sources[shaderType][shaderNdx].size();

                m_shaders[shaderType].reserve(m_shaders[shaderType].size() + 1);

                m_shaders[shaderType].push_back(new Shader(gl, ShaderType(shaderType)));
                m_shaders[shaderType].back()->setSources(1, &source, &length);
                m_shaders[shaderType].back()->compile();

                shadersOk = shadersOk && m_shaders[shaderType].back()->getCompileStatus();
            }
        }

        if (shadersOk)
        {
            for (int shaderType = 0; shaderType < SHADERTYPE_LAST; shaderType++)
                for (int shaderNdx = 0; shaderNdx < (int)m_shaders[shaderType].size(); ++shaderNdx)
                    m_program.attachShader(m_shaders[shaderType][shaderNdx]->getShader());

            for (std::vector<AttribLocationBinding>::const_iterator binding = sources.attribLocationBindings.begin();
                 binding != sources.attribLocationBindings.end(); ++binding)
                m_program.bindAttribLocation(binding->location, binding->name.c_str());

            DE_ASSERT((sources.transformFeedbackBufferMode == GL_NONE) == sources.transformFeedbackVaryings.empty());
            if (sources.transformFeedbackBufferMode != GL_NONE)
            {
                std::vector<const char *> tfVaryings(sources.transformFeedbackVaryings.size());
                for (int ndx = 0; ndx < (int)tfVaryings.size(); ndx++)
                    tfVaryings[ndx] = sources.transformFeedbackVaryings[ndx].c_str();

                m_program.transformFeedbackVaryings((int)tfVaryings.size(), &tfVaryings[0],
                                                    sources.transformFeedbackBufferMode);
            }

            if (sources.separable)
                m_program.setSeparable(true);

            m_program.link();
        }
    }
    catch (...)
    {
        for (int shaderType = 0; shaderType < SHADERTYPE_LAST; shaderType++)
            for (int shaderNdx = 0; shaderNdx < (int)m_shaders[shaderType].size(); ++shaderNdx)
                delete m_shaders[shaderType][shaderNdx];
        throw;
    }
}

void ShaderProgram::init(const glw::Functions &gl, const ProgramBinaries &binaries)
{
    try
    {
        bool shadersOk = true;

        for (uint32_t binaryNdx = 0; binaryNdx < binaries.binaries.size(); ++binaryNdx)
        {
            ShaderBinary shaderBinary = binaries.binaries[binaryNdx];
            if (!shaderBinary.binary.empty())
            {
                const char *binary = (const char *)shaderBinary.binary.data();
                const int length   = (int)(shaderBinary.binary.size() * sizeof(uint32_t));

                DE_ASSERT(shaderBinary.shaderEntryPoints.size() == shaderBinary.shaderTypes.size());

                std::vector<Shader *> shaders;
                for (uint32_t shaderTypeNdx = 0; shaderTypeNdx < shaderBinary.shaderTypes.size(); ++shaderTypeNdx)
                {
                    ShaderType shaderType = shaderBinary.shaderTypes[shaderTypeNdx];

                    Shader *shader = new Shader(gl, ShaderType(shaderType));

                    m_shaders[shaderType].reserve(m_shaders[shaderType].size() + 1);
                    m_shaders[shaderType].push_back(shader);
                    shaders.push_back(shader);
                }

                setBinary(gl, shaders, binaries.binaryFormat, binary, length);

                for (uint32_t shaderNdx = 0; shaderNdx < shaders.size(); ++shaderNdx)
                {
                    shaders[shaderNdx]->specialize(shaderBinary.shaderEntryPoints[shaderNdx].c_str(),
                                                   (uint32_t)shaderBinary.specializationIndices.size(),
                                                   shaderBinary.specializationIndices.data(),
                                                   shaderBinary.specializationValues.data());

                    shadersOk = shadersOk && shaders[shaderNdx]->getCompileStatus();
                }
            }
        }

        if (shadersOk)
        {
            for (int shaderType = 0; shaderType < SHADERTYPE_LAST; shaderType++)
                for (int shaderNdx = 0; shaderNdx < (int)m_shaders[shaderType].size(); ++shaderNdx)
                    m_program.attachShader(m_shaders[shaderType][shaderNdx]->getShader());

            m_program.link();
        }
    }
    catch (...)
    {
        for (int shaderType = 0; shaderType < SHADERTYPE_LAST; shaderType++)
            for (int shaderNdx = 0; shaderNdx < (int)m_shaders[shaderType].size(); ++shaderNdx)
                delete m_shaders[shaderType][shaderNdx];
        throw;
    }
}

void ShaderProgram::setBinary(const glw::Functions &gl, std::vector<Shader *> &shaders, glw::GLenum binaryFormat,
                              const void *binaryData, const int length)
{
    std::vector<glw::GLuint> shaderVec;
    for (uint32_t shaderNdx = 0; shaderNdx < shaders.size(); ++shaderNdx)
        shaderVec.push_back(shaders[shaderNdx]->getShader());

    gl.shaderBinary((glw::GLsizei)shaderVec.size(), shaderVec.data(), binaryFormat, binaryData, length);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glShaderBinary");

    for (uint32_t shaderNdx = 0; shaderNdx < shaders.size(); ++shaderNdx)
    {
        glw::GLint shaderState;
        gl.getShaderiv(shaders[shaderNdx]->getShader(), GL_SPIR_V_BINARY_ARB, &shaderState);
        GLU_EXPECT_NO_ERROR(gl.getError(), "getShaderiv");

        DE_ASSERT(shaderState == GL_TRUE);
    }
}

ShaderProgram::~ShaderProgram(void)
{
    for (int shaderType = 0; shaderType < SHADERTYPE_LAST; shaderType++)
        for (int shaderNdx = 0; shaderNdx < (int)m_shaders[shaderType].size(); ++shaderNdx)
            delete m_shaders[shaderType][shaderNdx];
}

// Utilities

uint32_t getGLShaderType(ShaderType shaderType)
{
    static const uint32_t s_typeMap[] = {
        GL_VERTEX_SHADER,
        GL_FRAGMENT_SHADER,
        GL_GEOMETRY_SHADER,
        GL_TESS_CONTROL_SHADER,
        GL_TESS_EVALUATION_SHADER,
        GL_COMPUTE_SHADER,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    };
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_typeMap) == SHADERTYPE_LAST);
    DE_ASSERT(de::inBounds<int>(shaderType, 0, DE_LENGTH_OF_ARRAY(s_typeMap)));
    return s_typeMap[shaderType];
}

uint32_t getGLShaderTypeBit(ShaderType shaderType)
{
    static const uint32_t s_typebitMap[] = {
        GL_VERTEX_SHADER_BIT,
        GL_FRAGMENT_SHADER_BIT,
        GL_GEOMETRY_SHADER_BIT,
        GL_TESS_CONTROL_SHADER_BIT,
        GL_TESS_EVALUATION_SHADER_BIT,
        GL_COMPUTE_SHADER_BIT,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    };
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_typebitMap) == SHADERTYPE_LAST);
    DE_ASSERT(de::inBounds<int>(shaderType, 0, DE_LENGTH_OF_ARRAY(s_typebitMap)));
    return s_typebitMap[shaderType];
}

qpShaderType getLogShaderType(ShaderType shaderType)
{
    static const qpShaderType s_typeMap[] = {
        QP_SHADER_TYPE_VERTEX,
        QP_SHADER_TYPE_FRAGMENT,
        QP_SHADER_TYPE_GEOMETRY,
        QP_SHADER_TYPE_TESS_CONTROL,
        QP_SHADER_TYPE_TESS_EVALUATION,
        QP_SHADER_TYPE_COMPUTE,
        QP_SHADER_TYPE_RAYGEN,
        QP_SHADER_TYPE_ANY_HIT,
        QP_SHADER_TYPE_CLOSEST_HIT,
        QP_SHADER_TYPE_MISS,
        QP_SHADER_TYPE_INTERSECTION,
        QP_SHADER_TYPE_CALLABLE,
        QP_SHADER_TYPE_TASK,
        QP_SHADER_TYPE_MESH,
    };
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_typeMap) == SHADERTYPE_LAST);
    DE_ASSERT(de::inBounds<int>(shaderType, 0, DE_LENGTH_OF_ARRAY(s_typeMap)));
    return s_typeMap[shaderType];
}

tcu::TestLog &operator<<(tcu::TestLog &log, const ShaderInfo &shaderInfo)
{
    return log << tcu::TestLog::Shader(getLogShaderType(shaderInfo.type), shaderInfo.source, shaderInfo.compileOk,
                                       shaderInfo.infoLog);
}

tcu::TestLog &operator<<(tcu::TestLog &log, const Shader &shader)
{
    return log << tcu::TestLog::ShaderProgram(false, "Plain shader") << shader.getInfo()
               << tcu::TestLog::EndShaderProgram;
}

static void logShaderProgram(tcu::TestLog &log, const ProgramInfo &programInfo, size_t numShaders,
                             const ShaderInfo *const *shaderInfos)
{
    log << tcu::TestLog::ShaderProgram(programInfo.linkOk, programInfo.infoLog);
    try
    {
        for (size_t shaderNdx = 0; shaderNdx < numShaders; ++shaderNdx)
            log << *shaderInfos[shaderNdx];
    }
    catch (...)
    {
        log << tcu::TestLog::EndShaderProgram;
        throw;
    }
    log << tcu::TestLog::EndShaderProgram;

    // Write statistics.
    {
        static const struct
        {
            const char *name;
            const char *description;
        } s_compileTimeDesc[] = {
            {"VertexCompileTime", "Vertex shader compile time"},
            {"FragmentCompileTime", "Fragment shader compile time"},
            {"GeometryCompileTime", "Geometry shader compile time"},
            {"TessControlCompileTime", "Tesselation control shader compile time"},
            {"TessEvaluationCompileTime", "Tesselation evaluation shader compile time"},
            {"ComputeCompileTime", "Compute shader compile time"},
            {"RaygenCompileTime", "Raygen shader compile time"},
            {"AnyHitCompileTime", "Any hit shader compile time"},
            {"ClosestHitCompileTime", "Closest hit shader compile time"},
            {"MissCompileTime", "Miss shader compile time"},
            {"IntersectionCompileTime", "Intersection shader compile time"},
            {"CallableCompileTime", "Callable shader compile time"},
            {"TaskCompileTime", "Task shader compile time"},
            {"MeshCompileTime", "Mesh shader compile time"},
        };
        DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_compileTimeDesc) == SHADERTYPE_LAST);

        bool allShadersOk = true;

        for (size_t shaderNdx = 0; shaderNdx < numShaders; ++shaderNdx)
        {
            const ShaderInfo &shaderInfo = *shaderInfos[shaderNdx];

            log << tcu::TestLog::Float(s_compileTimeDesc[shaderInfo.type].name,
                                       s_compileTimeDesc[shaderInfo.type].description, "ms", QP_KEY_TAG_TIME,
                                       (float)shaderInfo.compileTimeUs / 1000.0f);

            allShadersOk = allShadersOk && shaderInfo.compileOk;
        }

        if (allShadersOk)
            log << tcu::TestLog::Float("LinkTime", "Link time", "ms", QP_KEY_TAG_TIME,
                                       (float)programInfo.linkTimeUs / 1000.0f);
    }
}

tcu::TestLog &operator<<(tcu::TestLog &log, const ShaderProgramInfo &shaderProgramInfo)
{
    std::vector<const ShaderInfo *> shaderPtrs(shaderProgramInfo.shaders.size());

    for (size_t ndx = 0; ndx < shaderPtrs.size(); ndx++)
        shaderPtrs[ndx] = &shaderProgramInfo.shaders[ndx];

    logShaderProgram(log, shaderProgramInfo.program, shaderPtrs.size(), shaderPtrs.empty() ? nullptr : &shaderPtrs[0]);

    return log;
}

tcu::TestLog &operator<<(tcu::TestLog &log, const ShaderProgram &shaderProgram)
{
    std::vector<const ShaderInfo *> shaderPtrs;

    for (int shaderType = 0; shaderType < SHADERTYPE_LAST; shaderType++)
    {
        for (int shaderNdx = 0; shaderNdx < shaderProgram.getNumShaders((ShaderType)shaderType); shaderNdx++)
            shaderPtrs.push_back(&shaderProgram.getShaderInfo((ShaderType)shaderType, shaderNdx));
    }

    logShaderProgram(log, shaderProgram.getProgramInfo(), shaderPtrs.size(),
                     shaderPtrs.empty() ? nullptr : &shaderPtrs[0]);

    return log;
}

tcu::TestLog &operator<<(tcu::TestLog &log, const ProgramSources &sources)
{
    log << tcu::TestLog::ShaderProgram(false, "(Source only)");

    try
    {
        for (int shaderType = 0; shaderType < SHADERTYPE_LAST; shaderType++)
        {
            for (size_t shaderNdx = 0; shaderNdx < sources.sources[shaderType].size(); shaderNdx++)
            {
                log << tcu::TestLog::Shader(getLogShaderType((ShaderType)shaderType),
                                            sources.sources[shaderType][shaderNdx], false, "");
            }
        }
    }
    catch (...)
    {
        log << tcu::TestLog::EndShaderProgram;
        throw;
    }

    log << tcu::TestLog::EndShaderProgram;

    return log;
}

} // namespace glu
