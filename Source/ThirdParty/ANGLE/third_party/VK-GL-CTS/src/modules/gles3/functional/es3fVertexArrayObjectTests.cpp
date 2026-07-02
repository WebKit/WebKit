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
 * \brief Vertex array object tests
 *//*--------------------------------------------------------------------*/
#include "es3fVertexArrayObjectTests.hpp"

#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "gluRenderContext.hpp"

#include "tcuTestLog.hpp"
#include "tcuImageCompare.hpp"
#include "tcuSurface.hpp"
#include "tcuRenderTarget.hpp"

#include "deRandom.hpp"
#include "deString.h"
#include "deMemory.h"

#include <vector>
#include <string>
#include <memory>

#include "glw.h"

using std::string;
using std::vector;

namespace deqp
{
namespace gles3
{
namespace Functional
{

namespace
{
struct Attribute
{
    Attribute(void);
    GLboolean enabled;
    GLint size;
    GLint stride;
    GLenum type;
    GLboolean integer;
    GLint divisor;
    GLint offset;
    GLboolean normalized;

    int bufferNdx;
};

struct VertexArrayState
{
    VertexArrayState(void);

    vector<Attribute> attributes;
    int elementArrayBuffer;
};

VertexArrayState::VertexArrayState(void) : elementArrayBuffer(-1)
{
}

Attribute::Attribute(void)
    : enabled(GL_FALSE)
    , size(1)
    , stride(0)
    , type(GL_FLOAT)
    , integer(GL_FALSE)
    , divisor(0)
    , offset(0)
    , normalized(GL_FALSE)
    , bufferNdx(0)
{
}

struct BufferSpec
{
    int count;
    int size;
    int componentCount;
    int stride;
    int offset;

    GLenum type;

    int intRangeMin;
    int intRangeMax;

    float floatRangeMin;
    float floatRangeMax;
};

struct Spec
{
    Spec(void);

    int count;
    int instances;
    bool useDrawElements;
    GLenum indexType;
    int indexOffset;
    int indexRangeMin;
    int indexRangeMax;
    int indexCount;
    VertexArrayState state;
    VertexArrayState vao;
    vector<BufferSpec> buffers;
};

Spec::Spec(void)
    : count(-1)
    , instances(-1)
    , useDrawElements(false)
    , indexType(GL_NONE)
    , indexOffset(-1)
    , indexRangeMin(-1)
    , indexRangeMax(-1)
    , indexCount(-1)
{
}

} // namespace

class VertexArrayObjectTest : public TestCase
{
public:
    VertexArrayObjectTest(Context &context, const Spec &spec, const char *name, const char *description);
    ~VertexArrayObjectTest(void);
    virtual void init(void);
    virtual void deinit(void);
    virtual IterateResult iterate(void);

private:
    Spec m_spec;
    tcu::TestLog &m_log;
    vector<GLuint> m_buffers;
    glu::ShaderProgram *m_vaoProgram;
    glu::ShaderProgram *m_stateProgram;
    de::Random m_random;
    uint8_t *m_indices;

    void logVertexArrayState(tcu::TestLog &log, const VertexArrayState &state, const std::string &msg);
    uint8_t *createRandomBufferData(const BufferSpec &buffer);
    uint8_t *generateIndices(void);
    glu::ShaderProgram *createProgram(const VertexArrayState &state);
    void setState(const VertexArrayState &state);
    void render(tcu::Surface &vaoResult, tcu::Surface &defaultResult);
    void makeDrawCall(const VertexArrayState &state);
    void genReferences(tcu::Surface &vaoRef, tcu::Surface &defaultRef);

    VertexArrayObjectTest(const VertexArrayObjectTest &);
    VertexArrayObjectTest &operator=(const VertexArrayObjectTest &);
};

VertexArrayObjectTest::VertexArrayObjectTest(Context &context, const Spec &spec, const char *name,
                                             const char *description)
    : TestCase(context, name, description)
    , m_spec(spec)
    , m_log(context.getTestContext().getLog())
    , m_vaoProgram(NULL)
    , m_stateProgram(NULL)
    , m_random(deStringHash(name))
    , m_indices(NULL)
{
    // Makes zero to zero mapping for buffers
    m_buffers.push_back(0);
}

VertexArrayObjectTest::~VertexArrayObjectTest(void)
{
}

void VertexArrayObjectTest::logVertexArrayState(tcu::TestLog &log, const VertexArrayState &state,
                                                const std::string &msg)
{
    std::stringstream message;

    message << msg << "\n";
    message << "GL_ELEMENT_ARRAY_BUFFER : " << state.elementArrayBuffer << "\n";

    for (int attribNdx = 0; attribNdx < (int)state.attributes.size(); attribNdx++)
    {
        message << "attribute : " << attribNdx << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_ENABLED : "
                << (state.attributes[attribNdx].enabled ? "GL_TRUE" : "GL_FALSE") << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_SIZE : " << state.attributes[attribNdx].size << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_STRIDE : " << state.attributes[attribNdx].stride << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_TYPE : " << state.attributes[attribNdx].type << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_NORMALIZED : "
                << (state.attributes[attribNdx].normalized ? "GL_TRUE" : "GL_FALSE") << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_INTEGER : "
                << (state.attributes[attribNdx].integer ? "GL_TRUE" : "GL_FALSE") << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_DIVISOR : " << state.attributes[attribNdx].divisor << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_POINTER : " << state.attributes[attribNdx].offset << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING : " << m_buffers[state.attributes[attribNdx].bufferNdx]
                << "\n";
    }
    log << tcu::TestLog::Message << message.str() << tcu::TestLog::EndMessage;
}

void VertexArrayObjectTest::init(void)
{
    // \note [mika] Index 0 is reserved for 0 buffer
    for (int bufferNdx = 0; bufferNdx < (int)m_spec.buffers.size(); bufferNdx++)
    {
        uint8_t *data = createRandomBufferData(m_spec.buffers[bufferNdx]);

        try
        {
            GLuint buffer;
            GLU_CHECK_CALL(glGenBuffers(1, &buffer));
            m_buffers.push_back(buffer);

            GLU_CHECK_CALL(glBindBuffer(GL_ARRAY_BUFFER, buffer));
            GLU_CHECK_CALL(glBufferData(GL_ARRAY_BUFFER, m_spec.buffers[bufferNdx].size, data, GL_DYNAMIC_DRAW));
            GLU_CHECK_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
        }
        catch (...)
        {
            delete[] data;
            throw;
        }

        delete[] data;
    }

    m_vaoProgram = createProgram(m_spec.vao);
    m_log << tcu::TestLog::Message << "Program used with Vertex Array Object" << tcu::TestLog::EndMessage;
    m_log << *m_vaoProgram;
    m_stateProgram = createProgram(m_spec.state);
    m_log << tcu::TestLog::Message << "Program used with Vertex Array State" << tcu::TestLog::EndMessage;
    m_log << *m_stateProgram;

    if (!m_vaoProgram->isOk() || !m_stateProgram->isOk())
        TCU_FAIL("Failed to compile shaders");

    if (m_spec.useDrawElements && (m_spec.vao.elementArrayBuffer == 0 || m_spec.state.elementArrayBuffer == 0))
        m_indices = generateIndices();
}

void VertexArrayObjectTest::deinit(void)
{
    GLU_CHECK_CALL(glDeleteBuffers((GLsizei)m_buffers.size(), &(m_buffers[0])));
    m_buffers.clear();
    delete m_vaoProgram;
    delete m_stateProgram;
    delete[] m_indices;
}

uint8_t *VertexArrayObjectTest::generateIndices(void)
{
    int typeSize = 0;
    switch (m_spec.indexType)
    {
    case GL_UNSIGNED_INT:
        typeSize = sizeof(GLuint);
        break;
    case GL_UNSIGNED_SHORT:
        typeSize = sizeof(GLushort);
        break;
    case GL_UNSIGNED_BYTE:
        typeSize = sizeof(GLubyte);
        break;
    default:
        DE_ASSERT(false);
    }

    uint8_t *indices = new uint8_t[m_spec.indexCount * typeSize];

    for (int i = 0; i < m_spec.indexCount; i++)
    {
        uint8_t *pos = indices + typeSize * i;

        switch (m_spec.indexType)
        {
        case GL_UNSIGNED_INT:
        {
            GLuint v = (GLuint)m_random.getInt(m_spec.indexRangeMin, m_spec.indexRangeMax);
            deMemcpy(pos, &v, sizeof(v));
            break;
        }

        case GL_UNSIGNED_SHORT:
        {
            GLushort v = (GLushort)m_random.getInt(m_spec.indexRangeMin, m_spec.indexRangeMax);
            deMemcpy(pos, &v, sizeof(v));
            break;
        }

        case GL_UNSIGNED_BYTE:
        {
            GLubyte v = (GLubyte)m_random.getInt(m_spec.indexRangeMin, m_spec.indexRangeMax);
            deMemcpy(pos, &v, sizeof(v));
            break;
        }

        default:
            DE_ASSERT(false);
        }
    }

    return indices;
}

uint8_t *VertexArrayObjectTest::createRandomBufferData(const BufferSpec &buffer)
{
    uint8_t *data = new uint8_t[buffer.size];

    int stride;

    if (buffer.stride != 0)
    {
        stride = buffer.stride;
    }
    else
    {
        switch (buffer.type)
        {
        case GL_FLOAT:
            stride = buffer.componentCount * (int)sizeof(GLfloat);
            break;
        case GL_INT:
            stride = buffer.componentCount * (int)sizeof(GLint);
            break;
        case GL_UNSIGNED_INT:
            stride = buffer.componentCount * (int)sizeof(GLuint);
            break;
        case GL_SHORT:
            stride = buffer.componentCount * (int)sizeof(GLshort);
            break;
        case GL_UNSIGNED_SHORT:
            stride = buffer.componentCount * (int)sizeof(GLushort);
            break;
        case GL_BYTE:
            stride = buffer.componentCount * (int)sizeof(GLbyte);
            break;
        case GL_UNSIGNED_BYTE:
            stride = buffer.componentCount * (int)sizeof(GLubyte);
            break;

        default:
            stride = 0;
            DE_ASSERT(false);
        }
    }

    uint8_t *itr = data;

    for (int pos = 0; pos < buffer.count; pos++)
    {
        uint8_t *componentItr = itr;
        for (int componentNdx = 0; componentNdx < buffer.componentCount; componentNdx++)
        {
            switch (buffer.type)
            {
            case GL_FLOAT:
            {
                float v = buffer.floatRangeMin + (buffer.floatRangeMax - buffer.floatRangeMin) * m_random.getFloat();
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            case GL_INT:
            {
                GLint v = m_random.getInt(buffer.intRangeMin, buffer.intRangeMax);
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            case GL_UNSIGNED_INT:
            {
                GLuint v = m_random.getInt(buffer.intRangeMin, buffer.intRangeMax);
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            case GL_SHORT:
            {
                GLshort v = (GLshort)m_random.getInt(buffer.intRangeMin, buffer.intRangeMax);
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            case GL_UNSIGNED_SHORT:
            {
                GLushort v = (GLushort)m_random.getInt(buffer.intRangeMin, buffer.intRangeMax);
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            case GL_BYTE:
            {
                GLbyte v = (GLbyte)m_random.getInt(buffer.intRangeMin, buffer.intRangeMax);
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            case GL_UNSIGNED_BYTE:
            {
                GLubyte v = (GLubyte)m_random.getInt(buffer.intRangeMin, buffer.intRangeMax);
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            default:
                DE_ASSERT(false);
            }
        }

        itr += stride;
    }

    return data;
}

glu::ShaderProgram *VertexArrayObjectTest::createProgram(const VertexArrayState &state)
{
    std::stringstream vertexShaderStream;
    std::stringstream value;

    vertexShaderStream << "#version 300 es\n";

    for (int attribNdx = 0; attribNdx < (int)state.attributes.size(); attribNdx++)
    {
        if (state.attributes[attribNdx].integer)
            vertexShaderStream << "layout(location = " << attribNdx << ") in mediump ivec4 a_attrib" << attribNdx
                               << ";\n";
        else
            vertexShaderStream << "layout(location = " << attribNdx << ") in mediump vec4 a_attrib" << attribNdx
                               << ";\n";

        if (state.attributes[attribNdx].integer)
        {
            float scale = 0.0f;

            switch (state.attributes[0].type)
            {
            case GL_SHORT:
                scale = (1.0f / float((1u << 14) - 1u));
                break;
            case GL_UNSIGNED_SHORT:
                scale = (1.0f / float((1u << 15) - 1u));
                break;
            case GL_INT:
                scale = (1.0f / float((1u << 30) - 1u));
                break;
            case GL_UNSIGNED_INT:
                scale = (1.0f / float((1u << 31) - 1u));
                break;
            case GL_BYTE:
                scale = (1.0f / float((1u << 6) - 1u));
                break;
            case GL_UNSIGNED_BYTE:
                scale = (1.0f / float((1u << 7) - 1u));
                break;

            default:
                DE_ASSERT(false);
            }
            value << (attribNdx != 0 ? " + " : "") << scale << " * vec4(a_attrib" << attribNdx << ")";
        }
        else if (state.attributes[attribNdx].type != GL_FLOAT && !state.attributes[attribNdx].normalized)
        {
            float scale = 0.0f;

            switch (state.attributes[0].type)
            {
            case GL_SHORT:
                scale = (0.5f / float((1u << 14) - 1u));
                break;
            case GL_UNSIGNED_SHORT:
                scale = (0.5f / float((1u << 15) - 1u));
                break;
            case GL_INT:
                scale = (0.5f / float((1u << 30) - 1u));
                break;
            case GL_UNSIGNED_INT:
                scale = (0.5f / float((1u << 31) - 1u));
                break;
            case GL_BYTE:
                scale = (0.5f / float((1u << 6) - 1u));
                break;
            case GL_UNSIGNED_BYTE:
                scale = (0.5f / float((1u << 7) - 1u));
                break;

            default:
                DE_ASSERT(false);
            }
            value << (attribNdx != 0 ? " + " : "") << scale << " * a_attrib" << attribNdx;
        }
        else
            value << (attribNdx != 0 ? " + " : "") << "a_attrib" << attribNdx;
    }

    vertexShaderStream << "out mediump vec4 v_value;\n"
                       << "void main (void)\n"
                       << "{\n"
                       << "\tv_value = " << value.str() << ";\n";

    if (state.attributes[0].integer)
    {
        float scale = 0.0f;

        switch (state.attributes[0].type)
        {
        case GL_SHORT:
            scale = (1.0f / float((1u << 14) - 1u));
            break;
        case GL_UNSIGNED_SHORT:
            scale = (1.0f / float((1u << 15) - 1u));
            break;
        case GL_INT:
            scale = (1.0f / float((1u << 30) - 1u));
            break;
        case GL_UNSIGNED_INT:
            scale = (1.0f / float((1u << 31) - 1u));
            break;
        case GL_BYTE:
            scale = (1.0f / float((1u << 6) - 1u));
            break;
        case GL_UNSIGNED_BYTE:
            scale = (1.0f / float((1u << 7) - 1u));
            break;

        default:
            DE_ASSERT(false);
        }

        vertexShaderStream << "\tgl_Position = vec4(" << scale << " * "
                           << "vec3(a_attrib0.xyz), 1.0);\n"
                           << "}";
    }
    else
    {
        if (state.attributes[0].normalized || state.attributes[0].type == GL_FLOAT)
        {
            vertexShaderStream << "\tgl_Position = vec4(a_attrib0.xyz, 1.0);\n"
                               << "}";
        }
        else
        {
            float scale = 0.0f;

            switch (state.attributes[0].type)
            {
            case GL_SHORT:
                scale = (1.0f / float((1u << 14) - 1u));
                break;
            case GL_UNSIGNED_SHORT:
                scale = (1.0f / float((1u << 15) - 1u));
                break;
            case GL_INT:
                scale = (1.0f / float((1u << 30) - 1u));
                break;
            case GL_UNSIGNED_INT:
                scale = (1.0f / float((1u << 31) - 1u));
                break;
            case GL_BYTE:
                scale = (1.0f / float((1u << 6) - 1u));
                break;
            case GL_UNSIGNED_BYTE:
                scale = (1.0f / float((1u << 7) - 1u));
                break;

            default:
                DE_ASSERT(false);
            }

            scale *= 0.5f;

            vertexShaderStream << "\tgl_Position = vec4(" << scale << " * "
                               << "a_attrib0.xyz, 1.0);\n"
                               << "}";
        }
    }

    const char *fragmentShader = "#version 300 es\n"
                                 "in mediump vec4 v_value;\n"
                                 "layout(location = 0) out mediump vec4 fragColor;\n"
                                 "void main (void)\n"
                                 "{\n"
                                 "\tfragColor = vec4(v_value.xyz, 1.0);\n"
                                 "}";

    return new glu::ShaderProgram(m_context.getRenderContext(),
                                  glu::makeVtxFragSources(vertexShaderStream.str(), fragmentShader));
}

void VertexArrayObjectTest::setState(const VertexArrayState &state)
{
    GLU_CHECK_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffers[state.elementArrayBuffer]));

    for (int attribNdx = 0; attribNdx < (int)state.attributes.size(); attribNdx++)
    {
        GLU_CHECK_CALL(glBindBuffer(GL_ARRAY_BUFFER, m_buffers[state.attributes[attribNdx].bufferNdx]));
        if (state.attributes[attribNdx].enabled)
            GLU_CHECK_CALL(glEnableVertexAttribArray(attribNdx));
        else
            GLU_CHECK_CALL(glDisableVertexAttribArray(attribNdx));

        if (state.attributes[attribNdx].integer)
            GLU_CHECK_CALL(glVertexAttribIPointer(attribNdx, state.attributes[attribNdx].size,
                                                  state.attributes[attribNdx].type, state.attributes[attribNdx].stride,
                                                  (const GLvoid *)((GLintptr)state.attributes[attribNdx].offset)));
        else
            GLU_CHECK_CALL(
                glVertexAttribPointer(attribNdx, state.attributes[attribNdx].size, state.attributes[attribNdx].type,
                                      state.attributes[attribNdx].normalized, state.attributes[attribNdx].stride,
                                      (const GLvoid *)((GLintptr)state.attributes[attribNdx].offset)));

        GLU_CHECK_CALL(glVertexAttribDivisor(attribNdx, state.attributes[attribNdx].divisor));
    }
}

void VertexArrayObjectTest::makeDrawCall(const VertexArrayState &state)
{
    GLU_CHECK_CALL(glClearColor(0.7f, 0.7f, 0.7f, 1.0f));
    GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));

    for (int attribNdx = 0; attribNdx < (int)state.attributes.size(); attribNdx++)
    {
        if (state.attributes[attribNdx].integer)
            glVertexAttribI4i(attribNdx, 0, 0, 0, 1);
        else
            glVertexAttrib4f(attribNdx, 0.0f, 0.0f, 0.0f, 1.0f);
    }

    if (m_spec.useDrawElements)
    {
        if (state.elementArrayBuffer == 0)
        {
            if (m_spec.instances == 0)
                GLU_CHECK_CALL(glDrawElements(GL_TRIANGLES, m_spec.count, m_spec.indexType, m_indices));
            else
                GLU_CHECK_CALL(
                    glDrawElementsInstanced(GL_TRIANGLES, m_spec.count, m_spec.indexType, m_indices, m_spec.instances));
        }
        else
        {
            if (m_spec.instances == 0)
                GLU_CHECK_CALL(glDrawElements(GL_TRIANGLES, m_spec.count, m_spec.indexType,
                                              (GLvoid *)((GLintptr)m_spec.indexOffset)));
            else
                GLU_CHECK_CALL(glDrawElementsInstanced(GL_TRIANGLES, m_spec.count, m_spec.indexType,
                                                       (GLvoid *)((GLintptr)m_spec.indexOffset), m_spec.instances));
        }
    }
    else
    {
        if (m_spec.instances == 0)
            GLU_CHECK_CALL(glDrawArrays(GL_TRIANGLES, 0, m_spec.count));
        else
            GLU_CHECK_CALL(glDrawArraysInstanced(GL_TRIANGLES, 0, m_spec.count, m_spec.instances));
    }
}

void VertexArrayObjectTest::render(tcu::Surface &vaoResult, tcu::Surface &defaultResult)
{
    GLuint vao = 0;

    GLU_CHECK_CALL(glGenVertexArrays(1, &vao));
    GLU_CHECK_CALL(glBindVertexArray(vao));
    setState(m_spec.vao);
    GLU_CHECK_CALL(glBindVertexArray(0));

    setState(m_spec.state);

    GLU_CHECK_CALL(glBindVertexArray(vao));
    GLU_CHECK_CALL(glUseProgram(m_vaoProgram->getProgram()));
    makeDrawCall(m_spec.vao);
    glu::readPixels(m_context.getRenderContext(), 0, 0, vaoResult.getAccess());
    setState(m_spec.vao);
    GLU_CHECK_CALL(glBindVertexArray(0));

    GLU_CHECK_CALL(glUseProgram(m_stateProgram->getProgram()));
    makeDrawCall(m_spec.state);
    glu::readPixels(m_context.getRenderContext(), 0, 0, defaultResult.getAccess());
}

void VertexArrayObjectTest::genReferences(tcu::Surface &vaoRef, tcu::Surface &defaultRef)
{
    setState(m_spec.vao);
    GLU_CHECK_CALL(glUseProgram(m_vaoProgram->getProgram()));
    makeDrawCall(m_spec.vao);
    glu::readPixels(m_context.getRenderContext(), 0, 0, vaoRef.getAccess());

    setState(m_spec.state);
    GLU_CHECK_CALL(glUseProgram(m_stateProgram->getProgram()));
    makeDrawCall(m_spec.state);
    glu::readPixels(m_context.getRenderContext(), 0, 0, defaultRef.getAccess());
}

TestCase::IterateResult VertexArrayObjectTest::iterate(void)
{
    tcu::Surface vaoReference(m_context.getRenderContext().getRenderTarget().getWidth(),
                              m_context.getRenderContext().getRenderTarget().getHeight());
    tcu::Surface stateReference(m_context.getRenderContext().getRenderTarget().getWidth(),
                                m_context.getRenderContext().getRenderTarget().getHeight());

    tcu::Surface vaoResult(m_context.getRenderContext().getRenderTarget().getWidth(),
                           m_context.getRenderContext().getRenderTarget().getHeight());
    tcu::Surface stateResult(m_context.getRenderContext().getRenderTarget().getWidth(),
                             m_context.getRenderContext().getRenderTarget().getHeight());

    bool isOk;

    logVertexArrayState(m_log, m_spec.vao, "Vertex Array Object State");
    logVertexArrayState(m_log, m_spec.state, "OpenGL Vertex Array State");
    genReferences(stateReference, vaoReference);
    render(stateResult, vaoResult);

    isOk = tcu::pixelThresholdCompare(m_log, "Results", "Comparison result from rendering with Vertex Array State",
                                      stateReference, stateResult, tcu::RGBA(0, 0, 0, 0), tcu::COMPARE_LOG_RESULT);
    isOk = isOk &&
           tcu::pixelThresholdCompare(m_log, "Results", "Comparison result from rendering with Vertex Array Object",
                                      vaoReference, vaoResult, tcu::RGBA(0, 0, 0, 0), tcu::COMPARE_LOG_RESULT);

    if (isOk)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return STOP;
    }
    else
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }
}

class MultiVertexArrayObjectTest : public TestCase
{
public:
    MultiVertexArrayObjectTest(Context &context, const char *name, const char *description);
    ~MultiVertexArrayObjectTest(void);
    virtual void init(void);
    virtual void deinit(void);
    virtual IterateResult iterate(void);

private:
    Spec m_spec;
    tcu::TestLog &m_log;
    vector<GLuint> m_buffers;
    glu::ShaderProgram *m_vaoProgram;
    glu::ShaderProgram *m_stateProgram;
    de::Random m_random;
    uint8_t *m_indices;

    void logVertexArrayState(tcu::TestLog &log, const VertexArrayState &state, const std::string &msg);
    uint8_t *createRandomBufferData(const BufferSpec &buffer);
    uint8_t *generateIndices(void);
    glu::ShaderProgram *createProgram(const VertexArrayState &state);
    void setState(const VertexArrayState &state);
    void render(tcu::Surface &vaoResult, tcu::Surface &defaultResult);
    void makeDrawCall(const VertexArrayState &state);
    void genReferences(tcu::Surface &vaoRef, tcu::Surface &defaultRef);

    MultiVertexArrayObjectTest(const MultiVertexArrayObjectTest &);
    MultiVertexArrayObjectTest &operator=(const MultiVertexArrayObjectTest &);
};

MultiVertexArrayObjectTest::MultiVertexArrayObjectTest(Context &context, const char *name, const char *description)
    : TestCase(context, name, description)
    , m_log(context.getTestContext().getLog())
    , m_vaoProgram(NULL)
    , m_stateProgram(NULL)
    , m_random(deStringHash(name))
    , m_indices(NULL)
{
    // Makes zero to zero mapping for buffers
    m_buffers.push_back(0);
}

MultiVertexArrayObjectTest::~MultiVertexArrayObjectTest(void)
{
}

void MultiVertexArrayObjectTest::logVertexArrayState(tcu::TestLog &log, const VertexArrayState &state,
                                                     const std::string &msg)
{
    std::stringstream message;

    message << msg << "\n";
    message << "GL_ELEMENT_ARRAY_BUFFER : " << state.elementArrayBuffer << "\n";

    for (int attribNdx = 0; attribNdx < (int)state.attributes.size(); attribNdx++)
    {
        message << "attribute : " << attribNdx << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_ENABLED : "
                << (state.attributes[attribNdx].enabled ? "GL_TRUE" : "GL_FALSE") << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_SIZE : " << state.attributes[attribNdx].size << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_STRIDE : " << state.attributes[attribNdx].stride << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_TYPE : " << state.attributes[attribNdx].type << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_NORMALIZED : "
                << (state.attributes[attribNdx].normalized ? "GL_TRUE" : "GL_FALSE") << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_INTEGER : "
                << (state.attributes[attribNdx].integer ? "GL_TRUE" : "GL_FALSE") << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_DIVISOR : " << state.attributes[attribNdx].divisor << "\n"
                << "\tGL_VERTEX_ATTRIB_ARRAY_POINTER : " << state.attributes[attribNdx].offset << "\n"
                << "\t GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING : " << m_buffers[state.attributes[attribNdx].bufferNdx]
                << "\n";
    }
    log << tcu::TestLog::Message << message.str() << tcu::TestLog::EndMessage;
}

void MultiVertexArrayObjectTest::init(void)
{
    GLint attribCount;

    GLU_CHECK_CALL(glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attribCount));

    m_spec.useDrawElements          = false;
    m_spec.instances                = 0;
    m_spec.count                    = 24;
    m_spec.indexOffset              = 0;
    m_spec.indexRangeMin            = 0;
    m_spec.indexRangeMax            = 0;
    m_spec.indexType                = GL_NONE;
    m_spec.indexCount               = 0;
    m_spec.vao.elementArrayBuffer   = 0;
    m_spec.state.elementArrayBuffer = 0;

    for (int attribNdx = 0; attribNdx < attribCount; attribNdx++)
    {
        BufferSpec shortCoordBuffer48 = {48, 2 * 384, 4, 0, 0, GL_SHORT, -32768, 32768, 0.0f, 0.0f};
        m_spec.buffers.push_back(shortCoordBuffer48);

        m_spec.state.attributes.push_back(Attribute());
        m_spec.state.attributes[attribNdx].enabled    = (m_random.getInt(0, 4) == 0) ? GL_FALSE : GL_TRUE;
        m_spec.state.attributes[attribNdx].size       = m_random.getInt(2, 4);
        m_spec.state.attributes[attribNdx].stride     = 2 * m_random.getInt(1, 3);
        m_spec.state.attributes[attribNdx].type       = GL_SHORT;
        m_spec.state.attributes[attribNdx].integer    = m_random.getBool();
        m_spec.state.attributes[attribNdx].divisor    = m_random.getInt(0, 1);
        m_spec.state.attributes[attribNdx].offset     = 2 * m_random.getInt(0, 2);
        m_spec.state.attributes[attribNdx].normalized = m_random.getBool();
        m_spec.state.attributes[attribNdx].bufferNdx  = attribNdx + 1;

        if (attribNdx == 0)
        {
            m_spec.state.attributes[attribNdx].divisor = 0;
            m_spec.state.attributes[attribNdx].enabled = GL_TRUE;
            m_spec.state.attributes[attribNdx].size    = 2;
        }

        m_spec.vao.attributes.push_back(Attribute());
        m_spec.vao.attributes[attribNdx].enabled    = (m_random.getInt(0, 4) == 0) ? GL_FALSE : GL_TRUE;
        m_spec.vao.attributes[attribNdx].size       = m_random.getInt(2, 4);
        m_spec.vao.attributes[attribNdx].stride     = 2 * m_random.getInt(1, 3);
        m_spec.vao.attributes[attribNdx].type       = GL_SHORT;
        m_spec.vao.attributes[attribNdx].integer    = m_random.getBool();
        m_spec.vao.attributes[attribNdx].divisor    = m_random.getInt(0, 1);
        m_spec.vao.attributes[attribNdx].offset     = 2 * m_random.getInt(0, 2);
        m_spec.vao.attributes[attribNdx].normalized = m_random.getBool();
        m_spec.vao.attributes[attribNdx].bufferNdx  = attribCount - attribNdx;

        if (attribNdx == 0)
        {
            m_spec.vao.attributes[attribNdx].divisor = 0;
            m_spec.vao.attributes[attribNdx].enabled = GL_TRUE;
            m_spec.vao.attributes[attribNdx].size    = 2;
        }
    }

    // \note [mika] Index 0 is reserved for 0 buffer
    for (int bufferNdx = 0; bufferNdx < (int)m_spec.buffers.size(); bufferNdx++)
    {
        uint8_t *data = createRandomBufferData(m_spec.buffers[bufferNdx]);

        try
        {
            GLuint buffer;
            GLU_CHECK_CALL(glGenBuffers(1, &buffer));
            m_buffers.push_back(buffer);

            GLU_CHECK_CALL(glBindBuffer(GL_ARRAY_BUFFER, buffer));
            GLU_CHECK_CALL(glBufferData(GL_ARRAY_BUFFER, m_spec.buffers[bufferNdx].size, data, GL_DYNAMIC_DRAW));
            GLU_CHECK_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
        }
        catch (...)
        {
            delete[] data;
            throw;
        }

        delete[] data;
    }

    m_vaoProgram = createProgram(m_spec.vao);
    m_log << tcu::TestLog::Message << "Program used with Vertex Array Object" << tcu::TestLog::EndMessage;
    m_log << *m_vaoProgram;
    m_stateProgram = createProgram(m_spec.state);
    m_log << tcu::TestLog::Message << "Program used with Vertex Array State" << tcu::TestLog::EndMessage;
    m_log << *m_stateProgram;

    if (!m_vaoProgram->isOk() || !m_stateProgram->isOk())
        TCU_FAIL("Failed to compile shaders");

    if (m_spec.useDrawElements && (m_spec.vao.elementArrayBuffer == 0 || m_spec.state.elementArrayBuffer == 0))
        m_indices = generateIndices();
}

void MultiVertexArrayObjectTest::deinit(void)
{
    GLU_CHECK_CALL(glDeleteBuffers((GLsizei)m_buffers.size(), &(m_buffers[0])));
    m_buffers.clear();
    delete m_vaoProgram;
    delete m_stateProgram;
    delete[] m_indices;
}

uint8_t *MultiVertexArrayObjectTest::generateIndices(void)
{
    int typeSize = 0;
    switch (m_spec.indexType)
    {
    case GL_UNSIGNED_INT:
        typeSize = sizeof(GLuint);
        break;
    case GL_UNSIGNED_SHORT:
        typeSize = sizeof(GLushort);
        break;
    case GL_UNSIGNED_BYTE:
        typeSize = sizeof(GLubyte);
        break;
    default:
        DE_ASSERT(false);
    }

    uint8_t *indices = new uint8_t[m_spec.indexCount * typeSize];

    for (int i = 0; i < m_spec.indexCount; i++)
    {
        uint8_t *pos = indices + typeSize * i;

        switch (m_spec.indexType)
        {
        case GL_UNSIGNED_INT:
        {
            GLuint v = (GLuint)m_random.getInt(m_spec.indexRangeMin, m_spec.indexRangeMax);
            deMemcpy(pos, &v, sizeof(v));
            break;
        }

        case GL_UNSIGNED_SHORT:
        {
            GLushort v = (GLushort)m_random.getInt(m_spec.indexRangeMin, m_spec.indexRangeMax);
            deMemcpy(pos, &v, sizeof(v));
            break;
        }

        case GL_UNSIGNED_BYTE:
        {
            GLubyte v = (GLubyte)m_random.getInt(m_spec.indexRangeMin, m_spec.indexRangeMax);
            deMemcpy(pos, &v, sizeof(v));
            break;
        }

        default:
            DE_ASSERT(false);
        }
    }

    return indices;
}

uint8_t *MultiVertexArrayObjectTest::createRandomBufferData(const BufferSpec &buffer)
{
    uint8_t *data = new uint8_t[buffer.size];

    int stride;

    if (buffer.stride != 0)
    {
        stride = buffer.stride;
    }
    else
    {
        switch (buffer.type)
        {
        case GL_FLOAT:
            stride = buffer.componentCount * (int)sizeof(GLfloat);
            break;
        case GL_INT:
            stride = buffer.componentCount * (int)sizeof(GLint);
            break;
        case GL_UNSIGNED_INT:
            stride = buffer.componentCount * (int)sizeof(GLuint);
            break;
        case GL_SHORT:
            stride = buffer.componentCount * (int)sizeof(GLshort);
            break;
        case GL_UNSIGNED_SHORT:
            stride = buffer.componentCount * (int)sizeof(GLushort);
            break;
        case GL_BYTE:
            stride = buffer.componentCount * (int)sizeof(GLbyte);
            break;
        case GL_UNSIGNED_BYTE:
            stride = buffer.componentCount * (int)sizeof(GLubyte);
            break;

        default:
            stride = 0;
            DE_ASSERT(false);
        }
    }

    uint8_t *itr = data;

    for (int pos = 0; pos < buffer.count; pos++)
    {
        uint8_t *componentItr = itr;
        for (int componentNdx = 0; componentNdx < buffer.componentCount; componentNdx++)
        {
            switch (buffer.type)
            {
            case GL_FLOAT:
            {
                float v = buffer.floatRangeMin + (buffer.floatRangeMax - buffer.floatRangeMin) * m_random.getFloat();
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            case GL_INT:
            {
                GLint v = m_random.getInt(buffer.intRangeMin, buffer.intRangeMax);
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            case GL_UNSIGNED_INT:
            {
                GLuint v = (GLuint)m_random.getInt(buffer.intRangeMin, buffer.intRangeMax);
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            case GL_SHORT:
            {
                GLshort v = (GLshort)m_random.getInt(buffer.intRangeMin, buffer.intRangeMax);
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            case GL_UNSIGNED_SHORT:
            {
                GLushort v = (GLushort)m_random.getInt(buffer.intRangeMin, buffer.intRangeMax);
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            case GL_BYTE:
            {
                GLbyte v = (GLbyte)m_random.getInt(buffer.intRangeMin, buffer.intRangeMax);
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            case GL_UNSIGNED_BYTE:
            {
                GLubyte v = (GLubyte)m_random.getInt(buffer.intRangeMin, buffer.intRangeMax);
                deMemcpy(componentItr, &v, sizeof(v));
                componentItr += sizeof(v);
                break;
            }

            default:
                DE_ASSERT(false);
            }
        }

        itr += stride;
    }

    return data;
}

glu::ShaderProgram *MultiVertexArrayObjectTest::createProgram(const VertexArrayState &state)
{
    std::stringstream vertexShaderStream;
    std::stringstream value;

    vertexShaderStream << "#version 300 es\n";

    for (int attribNdx = 0; attribNdx < (int)state.attributes.size(); attribNdx++)
    {
        if (state.attributes[attribNdx].integer)
            vertexShaderStream << "layout(location = " << attribNdx << ") in mediump ivec4 a_attrib" << attribNdx
                               << ";\n";
        else
            vertexShaderStream << "layout(location = " << attribNdx << ") in mediump vec4 a_attrib" << attribNdx
                               << ";\n";

        if (state.attributes[attribNdx].integer)
        {
            float scale = 0.0f;

            switch (state.attributes[0].type)
            {
            case GL_SHORT:
                scale = (1.0f / float((1u << 14) - 1u));
                break;
            case GL_UNSIGNED_SHORT:
                scale = (1.0f / float((1u << 15) - 1u));
                break;
            case GL_INT:
                scale = (1.0f / float((1u << 30) - 1u));
                break;
            case GL_UNSIGNED_INT:
                scale = (1.0f / float((1u << 31) - 1u));
                break;
            case GL_BYTE:
                scale = (1.0f / float((1u << 6) - 1u));
                break;
            case GL_UNSIGNED_BYTE:
                scale = (1.0f / float((1u << 7) - 1u));
                break;

            default:
                DE_ASSERT(false);
            }
            value << (attribNdx != 0 ? " + " : "") << scale << " * vec4(a_attrib" << attribNdx << ")";
        }
        else if (state.attributes[attribNdx].type != GL_FLOAT && !state.attributes[attribNdx].normalized)
        {
            float scale = 0.0f;

            switch (state.attributes[0].type)
            {
            case GL_SHORT:
                scale = (0.5f / float((1u << 14) - 1u));
                break;
            case GL_UNSIGNED_SHORT:
                scale = (0.5f / float((1u << 15) - 1u));
                break;
            case GL_INT:
                scale = (0.5f / float((1u << 30) - 1u));
                break;
            case GL_UNSIGNED_INT:
                scale = (0.5f / float((1u << 31) - 1u));
                break;
            case GL_BYTE:
                scale = (0.5f / float((1u << 6) - 1u));
                break;
            case GL_UNSIGNED_BYTE:
                scale = (0.5f / float((1u << 7) - 1u));
                break;

            default:
                DE_ASSERT(false);
            }
            value << (attribNdx != 0 ? " + " : "") << scale << " * a_attrib" << attribNdx;
        }
        else
            value << (attribNdx != 0 ? " + " : "") << "a_attrib" << attribNdx;
    }

    vertexShaderStream << "out mediump vec4 v_value;\n"
                       << "void main (void)\n"
                       << "{\n"
                       << "\tv_value = " << value.str() << ";\n";

    if (state.attributes[0].integer)
    {
        float scale = 0.0f;

        switch (state.attributes[0].type)
        {
        case GL_SHORT:
            scale = (1.0f / float((1u << 14) - 1u));
            break;
        case GL_UNSIGNED_SHORT:
            scale = (1.0f / float((1u << 15) - 1u));
            break;
        case GL_INT:
            scale = (1.0f / float((1u << 30) - 1u));
            break;
        case GL_UNSIGNED_INT:
            scale = (1.0f / float((1u << 31) - 1u));
            break;
        case GL_BYTE:
            scale = (1.0f / float((1u << 6) - 1u));
            break;
        case GL_UNSIGNED_BYTE:
            scale = (1.0f / float((1u << 7) - 1u));
            break;

        default:
            DE_ASSERT(false);
        }

        vertexShaderStream << "\tgl_Position = vec4(" << scale << " * "
                           << "a_attrib0.xyz, 1.0);\n"
                           << "}";
    }
    else
    {
        if (state.attributes[0].normalized || state.attributes[0].type == GL_FLOAT)
        {
            vertexShaderStream << "\tgl_Position = vec4(a_attrib0.xyz, 1.0);\n"
                               << "}";
        }
        else
        {
            float scale = 0.0f;

            switch (state.attributes[0].type)
            {
            case GL_SHORT:
                scale = (1.0f / float((1u << 14) - 1u));
                break;
            case GL_UNSIGNED_SHORT:
                scale = (1.0f / float((1u << 15) - 1u));
                break;
            case GL_INT:
                scale = (1.0f / float((1u << 30) - 1u));
                break;
            case GL_UNSIGNED_INT:
                scale = (1.0f / float((1u << 31) - 1u));
                break;
            case GL_BYTE:
                scale = (1.0f / float((1u << 6) - 1u));
                break;
            case GL_UNSIGNED_BYTE:
                scale = (1.0f / float((1u << 7) - 1u));
                break;

            default:
                DE_ASSERT(false);
            }

            scale *= 0.5f;

            vertexShaderStream << "\tgl_Position = vec4(" << scale << " * "
                               << "vec3(a_attrib0.xyz), 1.0);\n"
                               << "}";
        }
    }

    const char *fragmentShader = "#version 300 es\n"
                                 "in mediump vec4 v_value;\n"
                                 "layout(location = 0) out mediump vec4 fragColor;\n"
                                 "void main (void)\n"
                                 "{\n"
                                 "\tfragColor = vec4(v_value.xyz, 1.0);\n"
                                 "}";

    return new glu::ShaderProgram(m_context.getRenderContext(),
                                  glu::makeVtxFragSources(vertexShaderStream.str(), fragmentShader));
}

void MultiVertexArrayObjectTest::setState(const VertexArrayState &state)
{
    GLU_CHECK_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffers[state.elementArrayBuffer]));

    for (int attribNdx = 0; attribNdx < (int)state.attributes.size(); attribNdx++)
    {
        GLU_CHECK_CALL(glBindBuffer(GL_ARRAY_BUFFER, m_buffers[state.attributes[attribNdx].bufferNdx]));
        if (state.attributes[attribNdx].enabled)
            GLU_CHECK_CALL(glEnableVertexAttribArray(attribNdx));
        else
            GLU_CHECK_CALL(glDisableVertexAttribArray(attribNdx));

        if (state.attributes[attribNdx].integer)
            GLU_CHECK_CALL(glVertexAttribIPointer(attribNdx, state.attributes[attribNdx].size,
                                                  state.attributes[attribNdx].type, state.attributes[attribNdx].stride,
                                                  (const GLvoid *)((GLintptr)state.attributes[attribNdx].offset)));
        else
            GLU_CHECK_CALL(
                glVertexAttribPointer(attribNdx, state.attributes[attribNdx].size, state.attributes[attribNdx].type,
                                      state.attributes[attribNdx].normalized, state.attributes[attribNdx].stride,
                                      (const GLvoid *)((GLintptr)state.attributes[attribNdx].offset)));

        GLU_CHECK_CALL(glVertexAttribDivisor(attribNdx, state.attributes[attribNdx].divisor));
    }
}

void MultiVertexArrayObjectTest::makeDrawCall(const VertexArrayState &state)
{
    GLU_CHECK_CALL(glClearColor(0.7f, 0.7f, 0.7f, 1.0f));
    GLU_CHECK_CALL(glClear(GL_COLOR_BUFFER_BIT));

    if (m_spec.useDrawElements)
    {
        if (state.elementArrayBuffer == 0)
        {
            if (m_spec.instances == 0)
                GLU_CHECK_CALL(glDrawElements(GL_TRIANGLES, m_spec.count, m_spec.indexType, m_indices));
            else
                GLU_CHECK_CALL(
                    glDrawElementsInstanced(GL_TRIANGLES, m_spec.count, m_spec.indexType, m_indices, m_spec.instances));
        }
        else
        {
            if (m_spec.instances == 0)
                GLU_CHECK_CALL(glDrawElements(GL_TRIANGLES, m_spec.count, m_spec.indexType,
                                              (GLvoid *)((GLintptr)m_spec.indexOffset)));
            else
                GLU_CHECK_CALL(glDrawElementsInstanced(GL_TRIANGLES, m_spec.count, m_spec.indexType,
                                                       (GLvoid *)((GLintptr)m_spec.indexOffset), m_spec.instances));
        }
    }
    else
    {
        if (m_spec.instances == 0)
            GLU_CHECK_CALL(glDrawArrays(GL_TRIANGLES, 0, m_spec.count));
        else
            GLU_CHECK_CALL(glDrawArraysInstanced(GL_TRIANGLES, 0, m_spec.count, m_spec.instances));
    }
}

void MultiVertexArrayObjectTest::render(tcu::Surface &vaoResult, tcu::Surface &defaultResult)
{
    GLuint vao = 0;

    GLU_CHECK_CALL(glGenVertexArrays(1, &vao));
    GLU_CHECK_CALL(glBindVertexArray(vao));
    setState(m_spec.vao);
    GLU_CHECK_CALL(glBindVertexArray(0));

    setState(m_spec.state);

    GLU_CHECK_CALL(glBindVertexArray(vao));
    GLU_CHECK_CALL(glUseProgram(m_vaoProgram->getProgram()));
    makeDrawCall(m_spec.vao);
    glu::readPixels(m_context.getRenderContext(), 0, 0, vaoResult.getAccess());
    setState(m_spec.vao);
    GLU_CHECK_CALL(glBindVertexArray(0));

    GLU_CHECK_CALL(glUseProgram(m_stateProgram->getProgram()));
    makeDrawCall(m_spec.state);
    glu::readPixels(m_context.getRenderContext(), 0, 0, defaultResult.getAccess());
}

void MultiVertexArrayObjectTest::genReferences(tcu::Surface &vaoRef, tcu::Surface &defaultRef)
{
    setState(m_spec.vao);
    GLU_CHECK_CALL(glUseProgram(m_vaoProgram->getProgram()));
    makeDrawCall(m_spec.vao);
    glu::readPixels(m_context.getRenderContext(), 0, 0, vaoRef.getAccess());

    setState(m_spec.state);
    GLU_CHECK_CALL(glUseProgram(m_stateProgram->getProgram()));
    makeDrawCall(m_spec.state);
    glu::readPixels(m_context.getRenderContext(), 0, 0, defaultRef.getAccess());
}

TestCase::IterateResult MultiVertexArrayObjectTest::iterate(void)
{
    tcu::Surface vaoReference(m_context.getRenderContext().getRenderTarget().getWidth(),
                              m_context.getRenderContext().getRenderTarget().getHeight());
    tcu::Surface stateReference(m_context.getRenderContext().getRenderTarget().getWidth(),
                                m_context.getRenderContext().getRenderTarget().getHeight());

    tcu::Surface vaoResult(m_context.getRenderContext().getRenderTarget().getWidth(),
                           m_context.getRenderContext().getRenderTarget().getHeight());
    tcu::Surface stateResult(m_context.getRenderContext().getRenderTarget().getWidth(),
                             m_context.getRenderContext().getRenderTarget().getHeight());

    bool isOk;

    logVertexArrayState(m_log, m_spec.vao, "Vertex Array Object State");
    logVertexArrayState(m_log, m_spec.state, "OpenGL Vertex Array State");
    genReferences(stateReference, vaoReference);
    render(stateResult, vaoResult);

    isOk = tcu::pixelThresholdCompare(m_log, "Results", "Comparison result from rendering with Vertex Array State",
                                      stateReference, stateResult, tcu::RGBA(0, 0, 0, 0), tcu::COMPARE_LOG_RESULT);
    isOk = isOk &&
           tcu::pixelThresholdCompare(m_log, "Results", "Comparison result from rendering with Vertex Array Object",
                                      vaoReference, vaoResult, tcu::RGBA(0, 0, 0, 0), tcu::COMPARE_LOG_RESULT);

    if (isOk)
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
        return STOP;
    }
    else
    {
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Fail");
        return STOP;
    }
}

VertexArrayObjectTestGroup::VertexArrayObjectTestGroup(Context &context)
    : TestCaseGroup(context, "vertex_array_objects", "Vertex array object test cases")
{
}

VertexArrayObjectTestGroup::~VertexArrayObjectTestGroup(void)
{
}

void VertexArrayObjectTestGroup::init(void)
{
    BufferSpec floatCoordBuffer48_1 = {48, 384, 2, 0, 0, GL_FLOAT, 0, 0, -1.0f, 1.0f};
    BufferSpec floatCoordBuffer48_2 = {48, 384, 2, 0, 0, GL_FLOAT, 0, 0, -1.0f, 1.0f};

    BufferSpec shortCoordBuffer48 = {48, 192, 2, 0, 0, GL_SHORT, -32768, 32768, 0.0f, 0.0f};

    // Different buffer
    {
        Spec spec;

        VertexArrayState state;

        state.attributes.push_back(Attribute());

        state.attributes[0].enabled    = true;
        state.attributes[0].size       = 2;
        state.attributes[0].stride     = 0;
        state.attributes[0].type       = GL_FLOAT;
        state.attributes[0].integer    = GL_FALSE;
        state.attributes[0].divisor    = 0;
        state.attributes[0].offset     = 0;
        state.attributes[0].normalized = GL_FALSE;

        state.elementArrayBuffer = 0;

        spec.buffers.push_back(floatCoordBuffer48_1);
        spec.buffers.push_back(floatCoordBuffer48_2);

        spec.useDrawElements = false;
        spec.instances       = 0;
        spec.count           = 48;
        spec.vao             = state;
        spec.state           = state;
        spec.indexOffset     = 0;
        spec.indexRangeMin   = 0;
        spec.indexRangeMax   = 0;
        spec.indexType       = GL_NONE;
        spec.indexCount      = 0;

        spec.state.attributes[0].bufferNdx = 1;
        spec.vao.attributes[0].bufferNdx   = 2;
        addChild(new VertexArrayObjectTest(m_context, spec, "diff_buffer", "diff_buffer"));
    }
    // Different size
    {
        Spec spec;

        VertexArrayState state;

        state.attributes.push_back(Attribute());

        state.attributes[0].enabled    = true;
        state.attributes[0].size       = 2;
        state.attributes[0].stride     = 0;
        state.attributes[0].type       = GL_FLOAT;
        state.attributes[0].integer    = GL_FALSE;
        state.attributes[0].divisor    = 0;
        state.attributes[0].offset     = 0;
        state.attributes[0].normalized = GL_FALSE;
        state.attributes[0].bufferNdx  = 1;

        state.elementArrayBuffer = 0;

        spec.buffers.push_back(floatCoordBuffer48_1);

        spec.useDrawElements = false;
        spec.instances       = 0;
        spec.count           = 24;
        spec.vao             = state;
        spec.state           = state;
        spec.indexOffset     = 0;
        spec.indexRangeMin   = 0;
        spec.indexRangeMax   = 0;
        spec.indexType       = GL_NONE;
        spec.indexCount      = 0;

        spec.state.attributes[0].size = 2;
        spec.vao.attributes[0].size   = 3;
        addChild(new VertexArrayObjectTest(m_context, spec, "diff_size", "diff_size"));
    }

    // Different stride
    {
        Spec spec;

        VertexArrayState state;

        state.attributes.push_back(Attribute());

        state.attributes[0].enabled    = true;
        state.attributes[0].size       = 2;
        state.attributes[0].stride     = 0;
        state.attributes[0].type       = GL_SHORT;
        state.attributes[0].integer    = GL_FALSE;
        state.attributes[0].divisor    = 0;
        state.attributes[0].offset     = 0;
        state.attributes[0].normalized = GL_TRUE;
        state.attributes[0].bufferNdx  = 1;

        state.elementArrayBuffer = 0;

        spec.buffers.push_back(shortCoordBuffer48);

        spec.useDrawElements = false;
        spec.instances       = 0;
        spec.count           = 24;
        spec.vao             = state;
        spec.state           = state;
        spec.indexOffset     = 0;
        spec.indexRangeMin   = 0;
        spec.indexRangeMax   = 0;
        spec.indexType       = GL_NONE;
        spec.indexCount      = 0;

        spec.vao.attributes[0].stride   = 2;
        spec.state.attributes[0].stride = 4;
        addChild(new VertexArrayObjectTest(m_context, spec, "diff_stride", "diff_stride"));
    }

    // Different types
    {
        Spec spec;

        VertexArrayState state;

        state.attributes.push_back(Attribute());

        state.attributes[0].enabled    = true;
        state.attributes[0].size       = 2;
        state.attributes[0].stride     = 0;
        state.attributes[0].type       = GL_SHORT;
        state.attributes[0].integer    = GL_FALSE;
        state.attributes[0].divisor    = 0;
        state.attributes[0].offset     = 0;
        state.attributes[0].normalized = GL_TRUE;
        state.attributes[0].bufferNdx  = 1;

        state.elementArrayBuffer = 0;

        spec.buffers.push_back(shortCoordBuffer48);

        spec.useDrawElements = false;
        spec.instances       = 0;
        spec.count           = 24;
        spec.vao             = state;
        spec.state           = state;
        spec.indexOffset     = 0;
        spec.indexRangeMin   = 0;
        spec.indexRangeMax   = 0;
        spec.indexType       = GL_NONE;
        spec.indexCount      = 0;

        spec.vao.attributes[0].type   = GL_SHORT;
        spec.state.attributes[0].type = GL_BYTE;
        addChild(new VertexArrayObjectTest(m_context, spec, "diff_type", "diff_type"));
    }
    // Different "integer"
    {
        Spec spec;

        VertexArrayState state;

        state.attributes.push_back(Attribute());

        state.attributes[0].enabled    = true;
        state.attributes[0].size       = 2;
        state.attributes[0].stride     = 0;
        state.attributes[0].type       = GL_BYTE;
        state.attributes[0].integer    = GL_TRUE;
        state.attributes[0].divisor    = 0;
        state.attributes[0].offset     = 0;
        state.attributes[0].normalized = GL_FALSE;
        state.attributes[0].bufferNdx  = 1;

        state.elementArrayBuffer = 0;

        spec.buffers.push_back(shortCoordBuffer48);

        spec.useDrawElements = false;
        spec.count           = 24;
        spec.vao             = state;
        spec.state           = state;
        spec.instances       = 0;
        spec.indexOffset     = 0;
        spec.indexRangeMin   = 0;
        spec.indexRangeMax   = 0;
        spec.indexType       = GL_NONE;
        spec.indexCount      = 0;

        spec.state.attributes[0].integer = GL_FALSE;
        spec.vao.attributes[0].integer   = GL_TRUE;
        addChild(new VertexArrayObjectTest(m_context, spec, "diff_integer", "diff_integer"));
    }
    // Different divisor
    {
        Spec spec;

        VertexArrayState state;

        state.attributes.push_back(Attribute());
        state.attributes.push_back(Attribute());

        state.attributes[0].enabled    = true;
        state.attributes[0].size       = 2;
        state.attributes[0].stride     = 0;
        state.attributes[0].type       = GL_SHORT;
        state.attributes[0].integer    = GL_FALSE;
        state.attributes[0].divisor    = 0;
        state.attributes[0].offset     = 0;
        state.attributes[0].normalized = GL_TRUE;
        state.attributes[0].bufferNdx  = 1;

        state.attributes[1].enabled    = true;
        state.attributes[1].size       = 4;
        state.attributes[1].stride     = 0;
        state.attributes[1].type       = GL_FLOAT;
        state.attributes[1].integer    = GL_FALSE;
        state.attributes[1].divisor    = 0;
        state.attributes[1].offset     = 0;
        state.attributes[1].normalized = GL_FALSE;
        state.attributes[1].bufferNdx  = 2;

        state.elementArrayBuffer = 0;

        spec.buffers.push_back(shortCoordBuffer48);
        spec.buffers.push_back(floatCoordBuffer48_1);

        spec.useDrawElements = false;
        spec.instances       = 10;
        spec.count           = 12;
        spec.vao             = state;
        spec.state           = state;
        spec.indexOffset     = 0;
        spec.indexRangeMin   = 0;
        spec.indexRangeMax   = 0;
        spec.indexType       = GL_NONE;
        spec.indexCount      = 0;

        spec.vao.attributes[1].divisor   = 3;
        spec.state.attributes[1].divisor = 2;

        addChild(new VertexArrayObjectTest(m_context, spec, "diff_divisor", "diff_divisor"));
    }
    // Different offset
    {
        Spec spec;

        VertexArrayState state;

        state.attributes.push_back(Attribute());

        state.attributes[0].enabled    = true;
        state.attributes[0].size       = 2;
        state.attributes[0].stride     = 0;
        state.attributes[0].type       = GL_SHORT;
        state.attributes[0].integer    = GL_FALSE;
        state.attributes[0].divisor    = 0;
        state.attributes[0].offset     = 0;
        state.attributes[0].normalized = GL_TRUE;
        state.attributes[0].bufferNdx  = 1;

        state.elementArrayBuffer = 0;

        spec.buffers.push_back(shortCoordBuffer48);

        spec.useDrawElements = false;
        spec.instances       = 0;
        spec.count           = 24;
        spec.vao             = state;
        spec.state           = state;
        spec.indexOffset     = 0;
        spec.indexRangeMin   = 0;
        spec.indexRangeMax   = 0;
        spec.indexType       = GL_NONE;
        spec.indexCount      = 0;

        spec.vao.attributes[0].offset   = 2;
        spec.state.attributes[0].offset = 4;
        addChild(new VertexArrayObjectTest(m_context, spec, "diff_offset", "diff_offset"));
    }
    // Different normalize
    {
        Spec spec;

        VertexArrayState state;

        state.attributes.push_back(Attribute());

        state.attributes[0].enabled    = true;
        state.attributes[0].size       = 2;
        state.attributes[0].stride     = 0;
        state.attributes[0].type       = GL_SHORT;
        state.attributes[0].integer    = GL_FALSE;
        state.attributes[0].divisor    = 0;
        state.attributes[0].offset     = 0;
        state.attributes[0].normalized = GL_TRUE;
        state.attributes[0].bufferNdx  = 1;

        state.elementArrayBuffer = 0;

        spec.buffers.push_back(shortCoordBuffer48);

        spec.useDrawElements = false;
        spec.instances       = 0;
        spec.count           = 48;
        spec.vao             = state;
        spec.state           = state;
        spec.indexOffset     = 0;
        spec.indexRangeMin   = 0;
        spec.indexRangeMax   = 0;
        spec.indexType       = GL_NONE;
        spec.indexCount      = 0;

        spec.vao.attributes[0].normalized   = GL_TRUE;
        spec.state.attributes[0].normalized = GL_FALSE;
        addChild(new VertexArrayObjectTest(m_context, spec, "diff_normalize", "diff_normalize"));
    }
    // DrawElements with buffer / Pointer
    {
        Spec spec;

        VertexArrayState state;

        state.attributes.push_back(Attribute());

        state.attributes[0].enabled    = true;
        state.attributes[0].size       = 2;
        state.attributes[0].stride     = 0;
        state.attributes[0].type       = GL_FLOAT;
        state.attributes[0].integer    = GL_FALSE;
        state.attributes[0].divisor    = 0;
        state.attributes[0].offset     = 0;
        state.attributes[0].normalized = GL_TRUE;
        state.attributes[0].bufferNdx  = 1;

        state.elementArrayBuffer = 0;

        spec.buffers.push_back(floatCoordBuffer48_1);

        BufferSpec indexBuffer = {24, 192, 1, 0, 0, GL_UNSIGNED_SHORT, 0, 48, 0.0f, 0.0f};
        spec.buffers.push_back(indexBuffer);

        spec.useDrawElements = true;
        spec.count           = 24;
        spec.vao             = state;
        spec.state           = state;
        spec.instances       = 0;
        spec.indexOffset     = 0;
        spec.indexRangeMin   = 0;
        spec.indexRangeMax   = 48;
        spec.indexType       = GL_UNSIGNED_SHORT;
        spec.indexCount      = 24;

        spec.state.elementArrayBuffer = 0;
        spec.vao.elementArrayBuffer   = 2;
        addChild(new VertexArrayObjectTest(m_context, spec, "diff_indices", "diff_indices"));
    }
    // Use all attributes

    addChild(new MultiVertexArrayObjectTest(m_context, "all_attributes", "all_attributes"));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
