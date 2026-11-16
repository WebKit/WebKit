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
 * \brief Shader API tests.
 *//*--------------------------------------------------------------------*/

#include "es2fShaderApiTests.hpp"
#include "es2fApiCase.hpp"
#include "tcuTestLog.hpp"

#include "gluRenderContext.hpp"
#include "gluShaderProgram.hpp"
#include "gluContextInfo.hpp"
#include "glwFunctions.hpp"
#include "glwDefs.hpp"
#include "glwEnums.hpp"

#include "deString.h"

#include "deRandom.hpp"
#include "deStringUtil.hpp"

#include <string>
#include <vector>
#include <map>

using namespace glw; // GL types

namespace deqp
{
namespace gles2
{
namespace Functional
{

using tcu::TestLog;

namespace
{

enum ShaderSourceCaseFlags
{
    CASE_EXPLICIT_SOURCE_LENGTHS = 1,
    CASE_RANDOM_NULL_TERMINATED  = 2
};

struct ShaderSources
{
    std::vector<std::string> strings;
    std::vector<int> lengths;
};

// Simple shaders

const char *getSimpleShaderSource(const glu::ShaderType shaderType)
{
    const char *simpleVertexShaderSource   = "void main (void) { gl_Position = vec4(0.0); }\n";
    const char *simpleFragmentShaderSource = "void main (void) { gl_FragColor = vec4(0.0); }\n";

    switch (shaderType)
    {
    case glu::SHADERTYPE_VERTEX:
        return simpleVertexShaderSource;
    case glu::SHADERTYPE_FRAGMENT:
        return simpleFragmentShaderSource;
    default:
        DE_ASSERT(false);
    }

    return 0;
}

void setShaderSources(glu::Shader &shader, const ShaderSources &sources)
{
    std::vector<const char *> cStrings(sources.strings.size(), 0);

    for (size_t ndx = 0; ndx < sources.strings.size(); ndx++)
        cStrings[ndx] = sources.strings[ndx].c_str();

    if (sources.lengths.size() > 0)
        shader.setSources((int)cStrings.size(), &cStrings[0], &sources.lengths[0]);
    else
        shader.setSources((int)cStrings.size(), &cStrings[0], 0);
}

void sliceSourceString(const std::string &in, ShaderSources &out, const int numSlices, const size_t paddingLength = 0)
{
    DE_ASSERT(numSlices > 0);

    const size_t sliceSize          = in.length() / numSlices;
    const size_t sliceSizeRemainder = in.length() - (sliceSize * numSlices);
    const std::string padding(paddingLength, 'E');

    for (int i = 0; i < numSlices; i++)
    {
        out.strings.push_back(in.substr(i * sliceSize, sliceSize) + padding);

        if (paddingLength > 0)
            out.lengths.push_back((int)sliceSize);
    }

    if (sliceSizeRemainder > 0)
    {
        const std::string lastString = in.substr(numSlices * sliceSize);
        const int lastStringLength   = (int)lastString.length();

        out.strings.push_back(lastString + padding);

        if (paddingLength > 0)
            out.lengths.push_back(lastStringLength);
    }
}

void queryShaderInfo(glu::RenderContext &renderCtx, uint32_t shader, glu::ShaderInfo &info)
{
    const glw::Functions &gl = renderCtx.getFunctions();

    info.compileOk     = false;
    info.compileTimeUs = 0;
    info.infoLog.clear();

    // Query source, status & log.
    {
        int compileStatus = 0;
        int sourceLen     = 0;
        int infoLogLen    = 0;
        int unusedLen;

        gl.getShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
        gl.getShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &sourceLen);
        gl.getShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glGetShaderiv()");

        info.compileOk = compileStatus != GL_FALSE;

        if (sourceLen > 0)
        {
            std::vector<char> source(sourceLen);
            gl.getShaderSource(shader, (int)source.size(), &unusedLen, &source[0]);
            info.source = std::string(&source[0], sourceLen);
        }

        if (infoLogLen > 0)
        {
            std::vector<char> infoLog(infoLogLen);
            gl.getShaderInfoLog(shader, (int)infoLog.size(), &unusedLen, &infoLog[0]);
            info.infoLog = std::string(&infoLog[0], infoLogLen);
        }
    }
}

// Shader source generator

class SourceGenerator
{
public:
    virtual ~SourceGenerator(void)
    {
    }

    virtual std::string next(const glu::ShaderType shaderType)    = 0;
    virtual bool finished(const glu::ShaderType shaderType) const = 0;
};

class ConstantShaderGenerator : public SourceGenerator
{
public:
    ConstantShaderGenerator(de::Random &rnd) : m_rnd(rnd)
    {
    }
    ~ConstantShaderGenerator(void)
    {
    }

    bool finished(const glu::ShaderType shaderType) const
    {
        DE_UNREF(shaderType);
        return false;
    }

    std::string next(const glu::ShaderType shaderType);

private:
    de::Random m_rnd;
};

std::string ConstantShaderGenerator::next(const glu::ShaderType shaderType)
{
    DE_ASSERT(shaderType == glu::SHADERTYPE_VERTEX || shaderType == glu::SHADERTYPE_FRAGMENT);

    const float value             = m_rnd.getFloat(0.0f, 1.0f);
    const std::string valueString = de::toString(value);
    const std::string outputName  = (shaderType == glu::SHADERTYPE_VERTEX) ? "gl_Position" : "gl_FragColor";

    std::string source = "#version 100\n"
                         "void main (void) { " +
                         outputName + " = vec4(" + valueString + "); }\n";

    return source;
}

// Shader allocation utility

class ShaderAllocator
{
public:
    ShaderAllocator(glu::RenderContext &context, SourceGenerator &generator);
    ~ShaderAllocator(void);

    bool hasShader(const glu::ShaderType shaderType);

    void setSource(const glu::ShaderType shaderType);

    glu::Shader &createShader(const glu::ShaderType shaderType);
    void deleteShader(const glu::ShaderType shaderType);

    glu::Shader &get(const glu::ShaderType shaderType)
    {
        DE_ASSERT(hasShader(shaderType));
        return *m_shaders[shaderType];
    }

private:
    const glu::RenderContext &m_context;
    SourceGenerator &m_srcGen;
    std::map<glu::ShaderType, glu::Shader *> m_shaders;
};

ShaderAllocator::ShaderAllocator(glu::RenderContext &context, SourceGenerator &generator)
    : m_context(context)
    , m_srcGen(generator)
{
}

ShaderAllocator::~ShaderAllocator(void)
{
    for (std::map<glu::ShaderType, glu::Shader *>::iterator shaderIter = m_shaders.begin();
         shaderIter != m_shaders.end(); shaderIter++)
        delete shaderIter->second;
    m_shaders.clear();
}

bool ShaderAllocator::hasShader(const glu::ShaderType shaderType)
{
    if (m_shaders.find(shaderType) != m_shaders.end())
        return true;
    else
        return false;
}

glu::Shader &ShaderAllocator::createShader(const glu::ShaderType shaderType)
{
    DE_ASSERT(!this->hasShader(shaderType));

    glu::Shader *const shader = new glu::Shader(m_context, shaderType);

    m_shaders[shaderType] = shader;
    this->setSource(shaderType);

    return *shader;
}

void ShaderAllocator::deleteShader(const glu::ShaderType shaderType)
{
    DE_ASSERT(this->hasShader(shaderType));

    delete m_shaders[shaderType];
    m_shaders.erase(shaderType);
}

void ShaderAllocator::setSource(const glu::ShaderType shaderType)
{
    DE_ASSERT(this->hasShader(shaderType));
    DE_ASSERT(!m_srcGen.finished(shaderType));

    const std::string source  = m_srcGen.next(shaderType);
    const char *const cSource = source.c_str();

    m_shaders[shaderType]->setSources(1, &cSource, 0);
}

// Logging utilities

void logShader(TestLog &log, glu::RenderContext &renderCtx, glu::Shader &shader)
{
    glu::ShaderInfo info;
    queryShaderInfo(renderCtx, shader.getShader(), info);

    log << TestLog::Shader(getLogShaderType(shader.getType()), info.source, info.compileOk, info.infoLog);
}

void logProgram(TestLog &log, glu::RenderContext &renderCtx, glu::Program &program, ShaderAllocator &shaders)
{
    log << TestLog::ShaderProgram(program.getLinkStatus(), program.getInfoLog());

    for (int shaderTypeInt = 0; shaderTypeInt < glu::SHADERTYPE_LAST; shaderTypeInt++)
    {
        const glu::ShaderType shaderType = (glu::ShaderType)shaderTypeInt;

        if (shaders.hasShader(shaderType))
            logShader(log, renderCtx, shaders.get(shaderType));
    }

    log << TestLog::EndShaderProgram;
}

void logVertexFragmentProgram(TestLog &log, glu::RenderContext &renderCtx, glu::Program &program,
                              glu::Shader &vertShader, glu::Shader &fragShader)
{
    DE_ASSERT(vertShader.getType() == glu::SHADERTYPE_VERTEX && fragShader.getType() == glu::SHADERTYPE_FRAGMENT);

    log << TestLog::ShaderProgram(program.getLinkStatus(), program.getInfoLog());

    logShader(log, renderCtx, vertShader);
    logShader(log, renderCtx, fragShader);

    log << TestLog::EndShaderProgram;
}

} // namespace

// Simple glCreateShader() case

class CreateShaderCase : public ApiCase
{
public:
    CreateShaderCase(Context &context, const char *name, const char *desc, glu::ShaderType shaderType)
        : ApiCase(context, name, desc)
        , m_shaderType(shaderType)
    {
    }

    void test(void)
    {
        const GLuint shaderObject = glCreateShader(glu::getGLShaderType(m_shaderType));

        TCU_CHECK(shaderObject != 0);

        glDeleteShader(shaderObject);
    }

private:
    const glu::ShaderType m_shaderType;
};

// Simple glCompileShader() case

class CompileShaderCase : public ApiCase
{
public:
    CompileShaderCase(Context &context, const char *name, const char *desc, glu::ShaderType shaderType)
        : ApiCase(context, name, desc)
        , m_shaderType(shaderType)
    {
    }

    bool checkCompileStatus(const GLuint shaderObject)
    {
        GLint compileStatus = -1;
        glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &compileStatus);
        GLU_CHECK();

        return (compileStatus == GL_TRUE);
    }

    void test(void)
    {
        const char *shaderSource  = getSimpleShaderSource(m_shaderType);
        const GLuint shaderObject = glCreateShader(glu::getGLShaderType(m_shaderType));

        TCU_CHECK(shaderObject != 0);

        glShaderSource(shaderObject, 1, &shaderSource, 0);
        glCompileShader(shaderObject);

        TCU_CHECK(checkCompileStatus(shaderObject));

        glDeleteShader(shaderObject);
    }

private:
    const glu::ShaderType m_shaderType;
};

// Base class for simple program API tests

class SimpleProgramCase : public ApiCase
{
public:
    SimpleProgramCase(Context &context, const char *name, const char *desc)
        : ApiCase(context, name, desc)
        , m_vertShader(0)
        , m_fragShader(0)
        , m_program(0)
    {
    }

    virtual ~SimpleProgramCase(void)
    {
    }

    virtual void compileShaders(void)
    {
        const char *vertSource = getSimpleShaderSource(glu::SHADERTYPE_VERTEX);
        const char *fragSource = getSimpleShaderSource(glu::SHADERTYPE_FRAGMENT);

        const GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
        const GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);

        TCU_CHECK(vertShader != 0);
        TCU_CHECK(fragShader != 0);

        glShaderSource(vertShader, 1, &vertSource, 0);
        glCompileShader(vertShader);

        glShaderSource(fragShader, 1, &fragSource, 0);
        glCompileShader(fragShader);

        GLU_CHECK();

        m_vertShader = vertShader;
        m_fragShader = fragShader;
    }

    void linkProgram(void)
    {
        const GLuint program = glCreateProgram();

        TCU_CHECK(program != 0);

        glAttachShader(program, m_vertShader);
        glAttachShader(program, m_fragShader);
        GLU_CHECK();

        glLinkProgram(program);

        m_program = program;
    }

    void cleanup(void)
    {
        glDeleteShader(m_vertShader);
        glDeleteShader(m_fragShader);
        glDeleteProgram(m_program);
    }

protected:
    GLuint m_vertShader;
    GLuint m_fragShader;
    GLuint m_program;
};

// glDeleteShader() case

class DeleteShaderCase : public SimpleProgramCase
{
public:
    DeleteShaderCase(Context &context, const char *name, const char *desc) : SimpleProgramCase(context, name, desc)
    {
    }

    bool checkDeleteStatus(GLuint shader)
    {
        GLint deleteStatus = -1;
        glGetShaderiv(shader, GL_DELETE_STATUS, &deleteStatus);
        GLU_CHECK();

        return (deleteStatus == GL_TRUE);
    }

    void deleteShaders(void)
    {
        glDeleteShader(m_vertShader);
        glDeleteShader(m_fragShader);
        GLU_CHECK();
    }

    void test(void)
    {
        compileShaders();
        linkProgram();
        GLU_CHECK();

        deleteShaders();

        TCU_CHECK(checkDeleteStatus(m_vertShader) && checkDeleteStatus(m_fragShader));

        glDeleteProgram(m_program);

        TCU_CHECK(!(glIsShader(m_vertShader) || glIsShader(m_fragShader)));
    }
};

// Simple glLinkProgram() case

class LinkVertexFragmentCase : public SimpleProgramCase
{
public:
    LinkVertexFragmentCase(Context &context, const char *name, const char *desc)
        : SimpleProgramCase(context, name, desc)
    {
    }

    bool checkLinkStatus(const GLuint programObject)
    {
        GLint linkStatus = -1;
        glGetProgramiv(programObject, GL_LINK_STATUS, &linkStatus);
        GLU_CHECK();

        return (linkStatus == GL_TRUE);
    }

    void test(void)
    {
        compileShaders();
        linkProgram();

        GLU_CHECK_MSG("Linking failed.");
        TCU_CHECK_MSG(checkLinkStatus(m_program), "Fail, expected LINK_STATUS to be TRUE.");

        cleanup();
    }
};

class ShaderSourceReplaceCase : public ApiCase
{
public:
    ShaderSourceReplaceCase(Context &context, const char *name, const char *desc, glu::ShaderType shaderType)
        : ApiCase(context, name, desc)
        , m_shaderType(shaderType)
    {
    }

    std::string generateFirstSource(void)
    {
        return getSimpleShaderSource(m_shaderType);
    }

    std::string generateSecondSource(void)
    {
        std::string str;

        str = "#version 100\n";
        str += "precision highp float;\n\n";

        str += "void main()\n";
        str += "{\n";
        str += "    float variable = 1.0;\n";

        if (m_shaderType == glu::SHADERTYPE_VERTEX)
            str += "    gl_Position = vec4(variable);\n";
        else if (m_shaderType == glu::SHADERTYPE_FRAGMENT)
            str += "    gl_FragColor = vec4(variable);\n";

        str += "}\n";

        return str;
    }

    GLint getSourceLength(glu::Shader &shader)
    {
        GLint sourceLength = 0;
        glGetShaderiv(shader.getShader(), GL_SHADER_SOURCE_LENGTH, &sourceLength);
        GLU_CHECK();

        return sourceLength;
    }

    std::string readSource(glu::Shader &shader)
    {
        const GLint sourceLength = getSourceLength(shader);
        std::vector<char> sourceBuffer(sourceLength + 1);

        glGetShaderSource(shader.getShader(), (GLsizei)sourceBuffer.size(), 0, &sourceBuffer[0]);

        return std::string(&sourceBuffer[0]);
    }

    void verifyShaderSourceReplaced(glu::Shader &shader, const std::string &firstSource,
                                    const std::string &secondSource)
    {
        TestLog &log             = m_testCtx.getLog();
        const std::string result = readSource(shader);

        if (result == firstSource)
        {
            log << TestLog::Message << "Fail, source was not replaced." << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Shader source nor replaced");
        }
        else if (result != secondSource)
        {
            log << TestLog::Message << "Fail, invalid shader source." << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Invalid source");
        }
    }

    void test(void)
    {
        TestLog &log = m_testCtx.getLog();

        glu::Shader shader(m_context.getRenderContext(), m_shaderType);

        const std::string firstSourceStr  = generateFirstSource();
        const std::string secondSourceStr = generateSecondSource();

        const char *firstSource  = firstSourceStr.c_str();
        const char *secondSource = secondSourceStr.c_str();

        log << TestLog::Message << "Setting shader source." << TestLog::EndMessage;

        shader.setSources(1, &firstSource, 0);
        GLU_CHECK();

        log << TestLog::Message << "Replacing shader source." << TestLog::EndMessage;

        shader.setSources(1, &secondSource, 0);
        GLU_CHECK();

        verifyShaderSourceReplaced(shader, firstSourceStr, secondSourceStr);
    }

private:
    glu::ShaderType m_shaderType;
};

// glShaderSource() split source case

class ShaderSourceSplitCase : public ApiCase
{
public:
    ShaderSourceSplitCase(Context &context, const char *name, const char *desc, glu::ShaderType shaderType,
                          const int numSlices, const uint32_t flags = 0)
        : ApiCase(context, name, desc)
        , m_rnd(deStringHash(getName()) ^ 0x4fb2337d)
        , m_shaderType(shaderType)
        , m_numSlices(numSlices)
        , m_explicitLengths((flags & CASE_EXPLICIT_SOURCE_LENGTHS) != 0)
        , m_randomNullTerm((flags & CASE_RANDOM_NULL_TERMINATED) != 0)
    {
        DE_ASSERT(m_shaderType == glu::SHADERTYPE_VERTEX || m_shaderType == glu::SHADERTYPE_FRAGMENT);
    }

    virtual ~ShaderSourceSplitCase(void)
    {
    }

    std::string generateFullSource(void)
    {
        std::string str;

        str = "#version 100\n";
        str += "precision highp float;\n\n";

        str += "void main()\n";
        str += "{\n";
        str += "    float variable = 1.0;\n";

        if (m_shaderType == glu::SHADERTYPE_VERTEX)
            str += "    gl_Position = vec4(variable);\n";
        else if (m_shaderType == glu::SHADERTYPE_FRAGMENT)
            str += "    gl_FragColor = vec4(variable);\n";

        str += "}\n";

        return str;
    }

    void insertRandomNullTermStrings(ShaderSources &sources)
    {
        const int numInserts = de::max(m_numSlices >> 2, 1);
        std::vector<int> indices(sources.strings.size(), 0);

        DE_ASSERT(sources.lengths.size() > 0);
        DE_ASSERT(sources.lengths.size() == sources.strings.size());

        for (int i = 0; i < (int)sources.strings.size(); i++)
            indices[i] = i;

        m_rnd.shuffle(indices.begin(), indices.end());

        for (int i = 0; i < numInserts; i++)
        {
            const int ndx                    = indices[i];
            const int unpaddedLength         = sources.lengths[ndx];
            const std::string unpaddedString = sources.strings[ndx].substr(0, unpaddedLength);

            sources.strings[ndx] = unpaddedString;
            sources.lengths[ndx] = m_rnd.getInt(-10, -1);
        }
    }

    void generateSources(ShaderSources &sources)
    {
        const size_t paddingLength = (m_explicitLengths ? 10 : 0);
        std::string str            = generateFullSource();

        sliceSourceString(str, sources, m_numSlices, paddingLength);

        if (m_randomNullTerm)
            insertRandomNullTermStrings(sources);
    }

    void buildProgram(glu::Shader &shader)
    {
        TestLog &log                  = m_testCtx.getLog();
        glu::RenderContext &renderCtx = m_context.getRenderContext();

        const glu::ShaderType supportShaderType =
            (m_shaderType == glu::SHADERTYPE_FRAGMENT ? glu::SHADERTYPE_VERTEX : glu::SHADERTYPE_FRAGMENT);
        const char *supportShaderSource = getSimpleShaderSource(supportShaderType);
        glu::Shader supportShader(renderCtx, supportShaderType);

        glu::Program program(renderCtx);

        supportShader.setSources(1, &supportShaderSource, 0);
        supportShader.compile();

        program.attachShader(shader.getShader());
        program.attachShader(supportShader.getShader());

        program.link();

        if (m_shaderType == glu::SHADERTYPE_VERTEX)
            logVertexFragmentProgram(log, renderCtx, program, shader, supportShader);
        else
            logVertexFragmentProgram(log, renderCtx, program, supportShader, shader);
    }

    void test(void)
    {
        TestLog &log                  = m_testCtx.getLog();
        glu::RenderContext &renderCtx = m_context.getRenderContext();

        ShaderSources sources;
        glu::Shader shader(renderCtx, m_shaderType);

        generateSources(sources);
        setShaderSources(shader, sources);
        shader.compile();

        buildProgram(shader);

        if (!shader.getCompileStatus())
        {
            log << TestLog::Message << "Compilation failed." << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Compile failed");
        }
    }

private:
    de::Random m_rnd;

    glu::ShaderType m_shaderType;
    const int m_numSlices;

    const bool m_explicitLengths;
    const bool m_randomNullTerm;
};

// Base class for program state persistence cases

class ProgramStateCase : public ApiCase
{
public:
    ProgramStateCase(Context &context, const char *name, const char *desc, glu::ShaderType shaderType);
    virtual ~ProgramStateCase(void)
    {
    }

    void buildProgram(glu::Program &program, ShaderAllocator &shaders);
    void verify(glu::Program &program, const glu::ProgramInfo &reference);

    void test(void);

    virtual void executeForProgram(glu::Program &program, ShaderAllocator &shaders) = 0;

protected:
    de::Random m_rnd;
    const glu::ShaderType m_shaderType;
};

ProgramStateCase::ProgramStateCase(Context &context, const char *name, const char *desc, glu::ShaderType shaderType)
    : ApiCase(context, name, desc)
    , m_rnd(deStringHash(name) ^ 0x713de0ca)
    , m_shaderType(shaderType)
{
    DE_ASSERT(m_shaderType == glu::SHADERTYPE_VERTEX || m_shaderType == glu::SHADERTYPE_FRAGMENT);
}

void ProgramStateCase::buildProgram(glu::Program &program, ShaderAllocator &shaders)
{
    TestLog &log = m_testCtx.getLog();

    glu::Shader &vertShader = shaders.createShader(glu::SHADERTYPE_VERTEX);
    glu::Shader &fragShader = shaders.createShader(glu::SHADERTYPE_FRAGMENT);

    vertShader.compile();
    fragShader.compile();

    program.attachShader(vertShader.getShader());
    program.attachShader(fragShader.getShader());
    program.link();

    logProgram(log, m_context.getRenderContext(), program, shaders);
}

void ProgramStateCase::verify(glu::Program &program, const glu::ProgramInfo &reference)
{
    TestLog &log                        = m_testCtx.getLog();
    const glu::ProgramInfo &programInfo = program.getInfo();

    if (!programInfo.linkOk)
    {
        log << TestLog::Message
            << "Fail, link status may only change as a result of linking or loading a program binary."
            << TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Link status changed");
    }

    if (programInfo.linkTimeUs != reference.linkTimeUs)
    {
        log << TestLog::Message << "Fail, reported link time changed." << TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Link time changed");
    }

    if (programInfo.infoLog != reference.infoLog)
    {
        log << TestLog::Message << "Fail, program infolog changed." << TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Infolog changed");
    }
}

void ProgramStateCase::test(void)
{
    TestLog &log                  = m_testCtx.getLog();
    glu::RenderContext &renderCtx = m_context.getRenderContext();

    ConstantShaderGenerator sourceGen(m_rnd);

    ShaderAllocator shaders(renderCtx, sourceGen);
    glu::Program program(renderCtx);

    buildProgram(program, shaders);

    if (program.getLinkStatus())
    {
        glu::ProgramInfo programInfo = program.getInfo();

        executeForProgram(program, shaders);

        verify(program, programInfo);

        logProgram(log, renderCtx, program, shaders);
    }
    else
    {
        log << TestLog::Message << "Fail, couldn't link program." << TestLog::EndMessage;
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Linking failed");
    }
}

// Program state case utilities

namespace
{

template <class T>
void addProgramStateCase(TestCaseGroup *group, Context &context, const std::string &name, const std::string &desc)
{
    for (int shaderTypeInt = 0; shaderTypeInt < 2; shaderTypeInt++)
    {
        const glu::ShaderType shaderType = (shaderTypeInt == 1) ? glu::SHADERTYPE_FRAGMENT : glu::SHADERTYPE_VERTEX;
        const std::string shaderTypeName = getShaderTypeName(shaderType);

        const std::string caseName = name + "_" + shaderTypeName;
        const std::string caseDesc = "Build program, " + desc + ", for " + shaderTypeName + " shader.";

        group->addChild(new T(context, caseName.c_str(), caseDesc.c_str(), shaderType));
    }
}

} // namespace

// Specialized program state cases

class ProgramStateDetachShaderCase : public ProgramStateCase
{
public:
    ProgramStateDetachShaderCase(Context &context, const char *name, const char *desc, glu::ShaderType shaderType)
        : ProgramStateCase(context, name, desc, shaderType)
    {
    }

    virtual ~ProgramStateDetachShaderCase(void)
    {
    }

    void executeForProgram(glu::Program &program, ShaderAllocator &shaders)
    {
        TestLog &log            = m_testCtx.getLog();
        glu::Shader &caseShader = shaders.get(m_shaderType);

        log << TestLog::Message << "Detaching " + std::string(getShaderTypeName(m_shaderType)) + " shader"
            << TestLog::EndMessage;
        program.detachShader(caseShader.getShader());
    }
};

class ProgramStateReattachShaderCase : public ProgramStateCase
{
public:
    ProgramStateReattachShaderCase(Context &context, const char *name, const char *desc, glu::ShaderType shaderType)
        : ProgramStateCase(context, name, desc, shaderType)
    {
    }

    virtual ~ProgramStateReattachShaderCase(void)
    {
    }

    void executeForProgram(glu::Program &program, ShaderAllocator &shaders)
    {
        TestLog &log            = m_testCtx.getLog();
        glu::Shader &caseShader = shaders.get(m_shaderType);

        log << TestLog::Message << "Reattaching " + std::string(getShaderTypeName(m_shaderType)) + " shader"
            << TestLog::EndMessage;
        program.detachShader(caseShader.getShader());
        program.attachShader(caseShader.getShader());
    }
};

class ProgramStateDeleteShaderCase : public ProgramStateCase
{
public:
    ProgramStateDeleteShaderCase(Context &context, const char *name, const char *desc, glu::ShaderType shaderType)
        : ProgramStateCase(context, name, desc, shaderType)
    {
    }

    virtual ~ProgramStateDeleteShaderCase(void)
    {
    }

    void executeForProgram(glu::Program &program, ShaderAllocator &shaders)
    {
        TestLog &log            = m_testCtx.getLog();
        glu::Shader &caseShader = shaders.get(m_shaderType);

        log << TestLog::Message << "Deleting " + std::string(getShaderTypeName(m_shaderType)) + " shader"
            << TestLog::EndMessage;
        program.detachShader(caseShader.getShader());
        shaders.deleteShader(m_shaderType);
    }
};

class ProgramStateReplaceShaderCase : public ProgramStateCase
{
public:
    ProgramStateReplaceShaderCase(Context &context, const char *name, const char *desc, glu::ShaderType shaderType)
        : ProgramStateCase(context, name, desc, shaderType)
    {
    }

    virtual ~ProgramStateReplaceShaderCase(void)
    {
    }

    void executeForProgram(glu::Program &program, ShaderAllocator &shaders)
    {
        TestLog &log            = m_testCtx.getLog();
        glu::Shader &caseShader = shaders.get(m_shaderType);

        log << TestLog::Message << "Deleting and replacing " + std::string(getShaderTypeName(m_shaderType)) + " shader"
            << TestLog::EndMessage;
        program.detachShader(caseShader.getShader());
        shaders.deleteShader(m_shaderType);
        program.attachShader(shaders.createShader(m_shaderType).getShader());
    }
};

class ProgramStateRecompileShaderCase : public ProgramStateCase
{
public:
    ProgramStateRecompileShaderCase(Context &context, const char *name, const char *desc, glu::ShaderType shaderType)
        : ProgramStateCase(context, name, desc, shaderType)
    {
    }

    virtual ~ProgramStateRecompileShaderCase(void)
    {
    }

    void executeForProgram(glu::Program &program, ShaderAllocator &shaders)
    {
        TestLog &log            = m_testCtx.getLog();
        glu::Shader &caseShader = shaders.get(m_shaderType);

        log << TestLog::Message << "Recompiling " + std::string(getShaderTypeName(m_shaderType)) + " shader"
            << TestLog::EndMessage;
        caseShader.compile();
        DE_UNREF(program);
    }
};

class ProgramStateReplaceSourceCase : public ProgramStateCase
{
public:
    ProgramStateReplaceSourceCase(Context &context, const char *name, const char *desc, glu::ShaderType shaderType)
        : ProgramStateCase(context, name, desc, shaderType)
    {
    }

    virtual ~ProgramStateReplaceSourceCase(void)
    {
    }

    void executeForProgram(glu::Program &program, ShaderAllocator &shaders)
    {
        TestLog &log            = m_testCtx.getLog();
        glu::Shader &caseShader = shaders.get(m_shaderType);

        log << TestLog::Message
            << "Replacing " + std::string(getShaderTypeName(m_shaderType)) + " shader source and recompiling"
            << TestLog::EndMessage;
        shaders.setSource(m_shaderType);
        caseShader.compile();
        DE_UNREF(program);
    }
};

// Test group

ShaderApiTests::ShaderApiTests(Context &context) : TestCaseGroup(context, "shader_api", "Shader API Cases")
{
}

ShaderApiTests::~ShaderApiTests(void)
{
}

void ShaderApiTests::init(void)
{
    // create and delete shaders
    {
        TestCaseGroup *createDeleteGroup = new TestCaseGroup(m_context, "create_delete", "glCreateShader() tests");
        addChild(createDeleteGroup);

        createDeleteGroup->addChild(new CreateShaderCase(m_context, "create_vertex_shader",
                                                         "Create vertex shader object", glu::SHADERTYPE_VERTEX));
        createDeleteGroup->addChild(new CreateShaderCase(m_context, "create_fragment_shader",
                                                         "Create fragment shader object", glu::SHADERTYPE_FRAGMENT));

        createDeleteGroup->addChild(
            new DeleteShaderCase(m_context, "delete_vertex_fragment", "Delete vertex shader and fragment shader"));
    }

    // compile and link
    {
        TestCaseGroup *compileLinkGroup = new TestCaseGroup(m_context, "compile_link", "Compile and link tests");
        addChild(compileLinkGroup);

        compileLinkGroup->addChild(
            new CompileShaderCase(m_context, "compile_vertex_shader", "Compile vertex shader", glu::SHADERTYPE_VERTEX));
        compileLinkGroup->addChild(new CompileShaderCase(m_context, "compile_fragment_shader",
                                                         "Compile fragment shader", glu::SHADERTYPE_FRAGMENT));

        compileLinkGroup->addChild(
            new LinkVertexFragmentCase(m_context, "link_vertex_fragment", "Link vertex and fragment shaders"));
    }

    // shader source
    {
        TestCaseGroup *shaderSourceGroup = new TestCaseGroup(m_context, "shader_source", "glShaderSource() tests");
        addChild(shaderSourceGroup);

        for (int shaderTypeInt = 0; shaderTypeInt < 2; shaderTypeInt++)
        {
            const glu::ShaderType shaderType = (shaderTypeInt == 1) ? glu::SHADERTYPE_FRAGMENT : glu::SHADERTYPE_VERTEX;

            const std::string caseName =
                std::string("replace_source") + ((shaderType == glu::SHADERTYPE_FRAGMENT) ? "_fragment" : "_vertex");
            const std::string caseDesc = std::string("Replace source code of ") +
                                         ((shaderType == glu::SHADERTYPE_FRAGMENT) ? "fragment" : "vertex") +
                                         " shader.";

            shaderSourceGroup->addChild(
                new ShaderSourceReplaceCase(m_context, caseName.c_str(), caseDesc.c_str(), shaderType));
        }

        for (int stringLengthsInt = 0; stringLengthsInt < 3; stringLengthsInt++)
            for (int caseNdx = 1; caseNdx <= 3; caseNdx++)
                for (int shaderTypeInt = 0; shaderTypeInt < 2; shaderTypeInt++)
                {
                    const int numSlices = 1 << caseNdx;
                    const glu::ShaderType shaderType =
                        (shaderTypeInt == 1) ? glu::SHADERTYPE_FRAGMENT : glu::SHADERTYPE_VERTEX;

                    const bool explicitLengths = (stringLengthsInt != 0);
                    const bool randomNullTerm  = (stringLengthsInt == 2);

                    const uint32_t flags = (explicitLengths ? CASE_EXPLICIT_SOURCE_LENGTHS : 0) |
                                           (randomNullTerm ? CASE_RANDOM_NULL_TERMINATED : 0);

                    const std::string caseName =
                        "split_source_" + de::toString(numSlices) +
                        (randomNullTerm ? "_random_negative_length" :
                                          (explicitLengths ? "_specify_lengths" : "_null_terminated")) +
                        ((shaderType == glu::SHADERTYPE_FRAGMENT) ? "_fragment" : "_vertex");

                    const std::string caseDesc =
                        std::string((shaderType == glu::SHADERTYPE_FRAGMENT) ? "Fragment" : "Vertex") +
                        " shader source split into " + de::toString(numSlices) + " pieces" +
                        (explicitLengths ? ", using explicitly specified string lengths" : "") +
                        (randomNullTerm ? " with random negative length values" : "");

                    shaderSourceGroup->addChild(new ShaderSourceSplitCase(m_context, caseName.c_str(), caseDesc.c_str(),
                                                                          shaderType, numSlices, flags));
                }
    }

    // link status and infolog
    {
        TestCaseGroup *linkStatusGroup =
            new TestCaseGroup(m_context, "program_state", "Program state persistence tests");
        addChild(linkStatusGroup);

        addProgramStateCase<ProgramStateDetachShaderCase>(linkStatusGroup, m_context, "detach_shader", "detach shader");
        addProgramStateCase<ProgramStateReattachShaderCase>(linkStatusGroup, m_context, "reattach_shader",
                                                            "reattach shader");
        addProgramStateCase<ProgramStateDeleteShaderCase>(linkStatusGroup, m_context, "delete_shader", "delete shader");
        addProgramStateCase<ProgramStateReplaceShaderCase>(linkStatusGroup, m_context, "replace_shader",
                                                           "replace shader object");
        addProgramStateCase<ProgramStateRecompileShaderCase>(linkStatusGroup, m_context, "recompile_shader",
                                                             "recompile shader");
        addProgramStateCase<ProgramStateReplaceSourceCase>(linkStatusGroup, m_context, "replace_source",
                                                           "replace shader source");
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
