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
 * \brief Draw call batching performance tests
 *//*--------------------------------------------------------------------*/

#include "es2pDrawCallBatchingTests.hpp"

#include "gluShaderProgram.hpp"
#include "gluRenderContext.hpp"

#include "glwDefs.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"

#include "tcuTestLog.hpp"

#include "deRandom.hpp"
#include "deStringUtil.hpp"

#include "deFile.h"
#include "deString.h"
#include "deClock.h"
#include "deThread.h"

#include <cmath>
#include <vector>
#include <string>
#include <sstream>

using tcu::TestLog;

using namespace glw;

using std::string;
using std::vector;

namespace deqp
{
namespace gles2
{
namespace Performance
{

namespace
{
const int CALIBRATION_SAMPLE_COUNT = 34;

class DrawCallBatchingTest : public tcu::TestCase
{
public:
    struct TestSpec
    {
        bool useStaticBuffer;
        int staticAttributeCount;

        bool useDynamicBuffer;
        int dynamicAttributeCount;

        int triangleCount;
        int drawCallCount;

        bool useDrawElements;
        bool useIndexBuffer;
        bool dynamicIndices;
    };

    DrawCallBatchingTest(Context &context, const char *name, const char *description, const TestSpec &spec);
    ~DrawCallBatchingTest(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    enum State
    {
        STATE_LOG_INFO = 0,
        STATE_WARMUP_BATCHED,
        STATE_WARMUP_UNBATCHED,
        STATE_CALC_CALIBRATION,
        STATE_SAMPLE
    };

    State m_state;

    glu::RenderContext &m_renderCtx;
    de::Random m_rnd;
    int m_sampleIteration;

    int m_unbatchedSampleCount;
    int m_batchedSampleCount;

    TestSpec m_spec;

    glu::ShaderProgram *m_program;

    vector<uint8_t> m_dynamicIndexData;
    vector<uint8_t> m_staticIndexData;

    vector<GLuint> m_unbatchedDynamicIndexBuffers;
    GLuint m_batchedDynamicIndexBuffer;

    GLuint m_unbatchedStaticIndexBuffer;
    GLuint m_batchedStaticIndexBuffer;

    vector<vector<int8_t>> m_staticAttributeDatas;
    vector<vector<int8_t>> m_dynamicAttributeDatas;

    vector<GLuint> m_batchedStaticBuffers;
    vector<GLuint> m_unbatchedStaticBuffers;

    vector<GLuint> m_batchedDynamicBuffers;
    vector<vector<GLuint>> m_unbatchedDynamicBuffers;

    vector<uint64_t> m_unbatchedSamplesUs;
    vector<uint64_t> m_batchedSamplesUs;

    void logTestInfo(void);

    uint64_t renderUnbatched(void);
    uint64_t renderBatched(void);

    void createIndexData(void);
    void createIndexBuffer(void);

    void createShader(void);
    void createAttributeDatas(void);
    void createArrayBuffers(void);
};

DrawCallBatchingTest::DrawCallBatchingTest(Context &context, const char *name, const char *description,
                                           const TestSpec &spec)
    : tcu::TestCase(context.getTestContext(), tcu::NODETYPE_PERFORMANCE, name, description)
    , m_state(STATE_LOG_INFO)
    , m_renderCtx(context.getRenderContext())
    , m_rnd(deStringHash(name))
    , m_sampleIteration(0)
    , m_unbatchedSampleCount(CALIBRATION_SAMPLE_COUNT)
    , m_batchedSampleCount(CALIBRATION_SAMPLE_COUNT)
    , m_spec(spec)
    , m_program(NULL)
    , m_batchedDynamicIndexBuffer(0)
    , m_unbatchedStaticIndexBuffer(0)
    , m_batchedStaticIndexBuffer(0)
{
}

DrawCallBatchingTest::~DrawCallBatchingTest(void)
{
    deinit();
}

void DrawCallBatchingTest::createIndexData(void)
{
    if (m_spec.dynamicIndices)
    {
        for (int drawNdx = 0; drawNdx < m_spec.drawCallCount; drawNdx++)
        {
            for (int triangleNdx = 0; triangleNdx < m_spec.triangleCount; triangleNdx++)
            {
                m_dynamicIndexData.push_back(uint8_t(triangleNdx * 3));
                m_dynamicIndexData.push_back(uint8_t(triangleNdx * 3 + 1));
                m_dynamicIndexData.push_back(uint8_t(triangleNdx * 3 + 2));
            }
        }
    }
    else
    {
        for (int drawNdx = 0; drawNdx < m_spec.drawCallCount; drawNdx++)
        {
            for (int triangleNdx = 0; triangleNdx < m_spec.triangleCount; triangleNdx++)
            {
                m_staticIndexData.push_back(uint8_t(triangleNdx * 3));
                m_staticIndexData.push_back(uint8_t(triangleNdx * 3 + 1));
                m_staticIndexData.push_back(uint8_t(triangleNdx * 3 + 2));
            }
        }
    }
}

void DrawCallBatchingTest::createShader(void)
{
    std::ostringstream vertexShader;
    std::ostringstream fragmentShader;

    for (int attributeNdx = 0; attributeNdx < m_spec.staticAttributeCount; attributeNdx++)
        vertexShader << "attribute mediump vec4 a_static" << attributeNdx << ";\n";

    if (m_spec.staticAttributeCount > 0 && m_spec.dynamicAttributeCount > 0)
        vertexShader << "\n";

    for (int attributeNdx = 0; attributeNdx < m_spec.dynamicAttributeCount; attributeNdx++)
        vertexShader << "attribute mediump vec4 a_dyn" << attributeNdx << ";\n";

    vertexShader << "\n"
                 << "varying mediump vec4 v_color;\n"
                 << "\n"
                 << "void main (void)\n"
                 << "{\n";

    vertexShader << "\tv_color = ";

    bool first = true;

    for (int attributeNdx = 0; attributeNdx < m_spec.staticAttributeCount; attributeNdx++)
    {
        if (!first)
            vertexShader << " + ";
        first = false;

        vertexShader << "a_static" << attributeNdx;
    }

    for (int attributeNdx = 0; attributeNdx < m_spec.dynamicAttributeCount; attributeNdx++)
    {
        if (!first)
            vertexShader << " + ";
        first = false;

        vertexShader << "a_dyn" << attributeNdx;
    }

    vertexShader << ";\n";

    if (m_spec.dynamicAttributeCount > 0)
        vertexShader << "\tgl_Position = a_dyn0;\n";
    else
        vertexShader << "\tgl_Position = a_static0;\n";

    vertexShader << "}";

    fragmentShader << "varying mediump vec4 v_color;\n"
                   << "\n"
                   << "void main(void)\n"
                   << "{\n"
                   << "\tgl_FragColor = v_color;\n"
                   << "}\n";

    m_program = new glu::ShaderProgram(m_renderCtx, glu::ProgramSources() << glu::VertexSource(vertexShader.str())
                                                                          << glu::FragmentSource(fragmentShader.str()));

    m_testCtx.getLog() << (*m_program);
    TCU_CHECK(m_program->isOk());
}

void DrawCallBatchingTest::createAttributeDatas(void)
{
    // Generate data for static attributes
    for (int attribute = 0; attribute < m_spec.staticAttributeCount; attribute++)
    {
        vector<int8_t> data;

        if (m_spec.dynamicAttributeCount == 0 && attribute == 0)
        {
            data.reserve(4 * 3 * m_spec.triangleCount * m_spec.drawCallCount);

            for (int i = 0; i < m_spec.triangleCount * m_spec.drawCallCount; i++)
            {
                int sign = (m_spec.triangleCount % 2 == 1 || i % 2 == 0 ? 1 : -1);

                data.push_back(int8_t(-127 * sign));
                data.push_back(int8_t(-127 * sign));
                data.push_back(0);
                data.push_back(127);

                data.push_back(int8_t(127 * sign));
                data.push_back(int8_t(-127 * sign));
                data.push_back(0);
                data.push_back(127);

                data.push_back(int8_t(127 * sign));
                data.push_back(int8_t(127 * sign));
                data.push_back(0);
                data.push_back(127);
            }
        }
        else
        {
            data.reserve(4 * 3 * m_spec.triangleCount * m_spec.drawCallCount);

            for (int i = 0; i < 4 * 3 * m_spec.triangleCount * m_spec.drawCallCount; i++)
                data.push_back((int8_t)m_rnd.getUint32());
        }

        m_staticAttributeDatas.push_back(data);
    }

    // Generate data for dynamic attributes
    for (int attribute = 0; attribute < m_spec.dynamicAttributeCount; attribute++)
    {
        vector<int8_t> data;

        if (attribute == 0)
        {
            data.reserve(4 * 3 * m_spec.triangleCount * m_spec.drawCallCount);

            for (int i = 0; i < m_spec.triangleCount * m_spec.drawCallCount; i++)
            {
                int sign = (i % 2 == 0 ? 1 : -1);

                data.push_back(int8_t(-127 * sign));
                data.push_back(int8_t(-127 * sign));
                data.push_back(0);
                data.push_back(127);

                data.push_back(int8_t(127 * sign));
                data.push_back(int8_t(-127 * sign));
                data.push_back(0);
                data.push_back(127);

                data.push_back(int8_t(127 * sign));
                data.push_back(int8_t(127 * sign));
                data.push_back(0);
                data.push_back(127);
            }
        }
        else
        {
            data.reserve(4 * 3 * m_spec.triangleCount * m_spec.drawCallCount);

            for (int i = 0; i < 4 * 3 * m_spec.triangleCount * m_spec.drawCallCount; i++)
                data.push_back((int8_t)m_rnd.getUint32());
        }

        m_dynamicAttributeDatas.push_back(data);
    }
}

void DrawCallBatchingTest::createArrayBuffers(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    if (m_spec.useStaticBuffer)
    {
        // Upload static attributes for batched
        for (int attribute = 0; attribute < m_spec.staticAttributeCount; attribute++)
        {
            GLuint buffer;

            gl.genBuffers(1, &buffer);
            gl.bindBuffer(GL_ARRAY_BUFFER, buffer);
            gl.bufferData(GL_ARRAY_BUFFER, 4 * 3 * m_spec.triangleCount * m_spec.drawCallCount,
                          &(m_staticAttributeDatas[attribute][0]), GL_STATIC_DRAW);
            gl.bindBuffer(GL_ARRAY_BUFFER, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "Creating static buffer failed");

            m_batchedStaticBuffers.push_back(buffer);
        }

        // Upload static attributes for unbatched
        for (int attribute = 0; attribute < m_spec.staticAttributeCount; attribute++)
        {
            GLuint buffer;

            gl.genBuffers(1, &buffer);
            gl.bindBuffer(GL_ARRAY_BUFFER, buffer);
            gl.bufferData(GL_ARRAY_BUFFER, 4 * 3 * m_spec.triangleCount, &(m_staticAttributeDatas[attribute][0]),
                          GL_STATIC_DRAW);
            gl.bindBuffer(GL_ARRAY_BUFFER, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "Creating static buffer failed");

            m_unbatchedStaticBuffers.push_back(buffer);
        }
    }

    if (m_spec.useDynamicBuffer)
    {
        // Upload dynamic attributes for batched
        for (int attribute = 0; attribute < m_spec.dynamicAttributeCount; attribute++)
        {
            GLuint buffer;

            gl.genBuffers(1, &buffer);
            gl.bindBuffer(GL_ARRAY_BUFFER, buffer);
            gl.bufferData(GL_ARRAY_BUFFER, 4 * 3 * m_spec.triangleCount * m_spec.drawCallCount,
                          &(m_dynamicAttributeDatas[attribute][0]), GL_STATIC_DRAW);
            gl.bindBuffer(GL_ARRAY_BUFFER, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "Creating dynamic buffer failed");

            m_batchedDynamicBuffers.push_back(buffer);
        }

        // Upload dynamic attributes for unbatched
        for (int attribute = 0; attribute < m_spec.dynamicAttributeCount; attribute++)
        {
            vector<GLuint> buffers;

            for (int drawNdx = 0; drawNdx < m_spec.drawCallCount; drawNdx++)
            {
                GLuint buffer;

                gl.genBuffers(1, &buffer);
                gl.bindBuffer(GL_ARRAY_BUFFER, buffer);
                gl.bufferData(GL_ARRAY_BUFFER, 4 * 3 * m_spec.triangleCount * m_spec.drawCallCount,
                              &(m_dynamicAttributeDatas[attribute][0]), GL_STATIC_DRAW);
                gl.bindBuffer(GL_ARRAY_BUFFER, 0);
                GLU_EXPECT_NO_ERROR(gl.getError(), "Creating dynamic buffer failed");

                buffers.push_back(buffer);
            }

            m_unbatchedDynamicBuffers.push_back(buffers);
        }
    }
}

void DrawCallBatchingTest::createIndexBuffer(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    if (m_spec.dynamicIndices)
    {
        for (int drawNdx = 0; drawNdx < m_spec.drawCallCount; drawNdx++)
        {
            GLuint buffer;

            gl.genBuffers(1, &buffer);
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
            gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * m_spec.triangleCount,
                          &(m_dynamicIndexData[drawNdx * m_spec.triangleCount * 3]), GL_STATIC_DRAW);
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "Creating dynamic index buffer failed");

            m_unbatchedDynamicIndexBuffers.push_back(buffer);
        }

        {
            GLuint buffer;

            gl.genBuffers(1, &buffer);
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
            gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * m_spec.triangleCount * m_spec.drawCallCount,
                          &(m_dynamicIndexData[0]), GL_STATIC_DRAW);
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "Creating dynamic index buffer failed");

            m_batchedDynamicIndexBuffer = buffer;
        }
    }
    else
    {
        {
            GLuint buffer;

            gl.genBuffers(1, &buffer);
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
            gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * m_spec.triangleCount * m_spec.drawCallCount,
                          &(m_staticIndexData[0]), GL_STATIC_DRAW);
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "Creating dynamic index buffer failed");

            m_batchedStaticIndexBuffer = buffer;
        }

        {
            GLuint buffer;

            gl.genBuffers(1, &buffer);
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
            gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * m_spec.triangleCount, &(m_staticIndexData[0]), GL_STATIC_DRAW);
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            GLU_EXPECT_NO_ERROR(gl.getError(), "Creating dynamic index buffer failed");

            m_unbatchedStaticIndexBuffer = buffer;
        }
    }
}

void DrawCallBatchingTest::init(void)
{
    createShader();
    createAttributeDatas();
    createArrayBuffers();

    if (m_spec.useDrawElements)
    {
        createIndexData();

        if (m_spec.useIndexBuffer)
            createIndexBuffer();
    }
}

void DrawCallBatchingTest::deinit(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    delete m_program;
    m_program = NULL;

    m_dynamicIndexData = vector<uint8_t>();
    m_staticIndexData  = vector<uint8_t>();

    if (!m_unbatchedDynamicIndexBuffers.empty())
    {
        gl.deleteBuffers((GLsizei)m_unbatchedDynamicIndexBuffers.size(), &(m_unbatchedDynamicIndexBuffers[0]));
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers()");

        m_unbatchedDynamicIndexBuffers = vector<GLuint>();
    }

    if (m_batchedDynamicIndexBuffer)
    {
        gl.deleteBuffers((GLsizei)1, &m_batchedDynamicIndexBuffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers()");

        m_batchedDynamicIndexBuffer = 0;
    }

    if (m_unbatchedStaticIndexBuffer)
    {
        gl.deleteBuffers((GLsizei)1, &m_unbatchedStaticIndexBuffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers()");

        m_unbatchedStaticIndexBuffer = 0;
    }

    if (m_batchedStaticIndexBuffer)
    {
        gl.deleteBuffers((GLsizei)1, &m_batchedStaticIndexBuffer);
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers()");

        m_batchedStaticIndexBuffer = 0;
    }

    m_staticAttributeDatas  = vector<vector<int8_t>>();
    m_dynamicAttributeDatas = vector<vector<int8_t>>();

    if (!m_batchedStaticBuffers.empty())
    {
        gl.deleteBuffers((GLsizei)m_batchedStaticBuffers.size(), &(m_batchedStaticBuffers[0]));
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers()");

        m_batchedStaticBuffers = vector<GLuint>();
    }

    if (!m_unbatchedStaticBuffers.empty())
    {
        gl.deleteBuffers((GLsizei)m_unbatchedStaticBuffers.size(), &(m_unbatchedStaticBuffers[0]));
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers()");

        m_unbatchedStaticBuffers = vector<GLuint>();
    }

    if (!m_batchedDynamicBuffers.empty())
    {
        gl.deleteBuffers((GLsizei)m_batchedDynamicBuffers.size(), &(m_batchedDynamicBuffers[0]));
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers()");

        m_batchedDynamicBuffers = vector<GLuint>();
    }

    for (int i = 0; i < (int)m_unbatchedDynamicBuffers.size(); i++)
    {
        gl.deleteBuffers((GLsizei)m_unbatchedDynamicBuffers[i].size(), &(m_unbatchedDynamicBuffers[i][0]));
        GLU_EXPECT_NO_ERROR(gl.getError(), "glDeleteBuffers()");
    }

    m_unbatchedDynamicBuffers = vector<vector<GLuint>>();

    m_unbatchedSamplesUs = vector<uint64_t>();
    m_batchedSamplesUs   = vector<uint64_t>();
}

uint64_t DrawCallBatchingTest::renderUnbatched(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    uint64_t beginUs         = 0;
    uint64_t endUs           = 0;
    vector<GLint> dynamicAttributeLocations;

    gl.viewport(0, 0, 32, 32);
    gl.useProgram(m_program->getProgram());

    // Setup static buffers
    for (int attribNdx = 0; attribNdx < m_spec.staticAttributeCount; attribNdx++)
    {
        GLint location = gl.getAttribLocation(m_program->getProgram(), ("a_static" + de::toString(attribNdx)).c_str());

        gl.enableVertexAttribArray(location);

        if (m_spec.useStaticBuffer)
        {
            gl.bindBuffer(GL_ARRAY_BUFFER, m_unbatchedStaticBuffers[attribNdx]);
            gl.vertexAttribPointer(location, 4, GL_BYTE, GL_TRUE, 0, NULL);
            gl.bindBuffer(GL_ARRAY_BUFFER, 0);
        }
        else
            gl.vertexAttribPointer(location, 4, GL_BYTE, GL_TRUE, 0, &(m_staticAttributeDatas[attribNdx][0]));
    }

    // Get locations of dynamic attributes
    for (int attribNdx = 0; attribNdx < m_spec.dynamicAttributeCount; attribNdx++)
    {
        GLint location = gl.getAttribLocation(m_program->getProgram(), ("a_dyn" + de::toString(attribNdx)).c_str());

        gl.enableVertexAttribArray(location);
        dynamicAttributeLocations.push_back(location);
    }

    if (m_spec.useDrawElements && m_spec.useIndexBuffer && !m_spec.dynamicIndices)
        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_unbatchedStaticIndexBuffer);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to setup initial state for rendering.");

    gl.finish();

    beginUs = deGetMicroseconds();

    for (int drawNdx = 0; drawNdx < m_spec.drawCallCount; drawNdx++)
    {
        for (int attribNdx = 0; attribNdx < m_spec.dynamicAttributeCount; attribNdx++)
        {
            if (m_spec.useDynamicBuffer)
            {
                gl.bindBuffer(GL_ARRAY_BUFFER, m_unbatchedDynamicBuffers[attribNdx][drawNdx]);
                gl.vertexAttribPointer(dynamicAttributeLocations[attribNdx], 4, GL_BYTE, GL_TRUE, 0, NULL);
                gl.bindBuffer(GL_ARRAY_BUFFER, 0);
            }
            else
                gl.vertexAttribPointer(dynamicAttributeLocations[attribNdx], 4, GL_BYTE, GL_TRUE, 0,
                                       &(m_dynamicAttributeDatas[attribNdx][m_spec.triangleCount * 3 * drawNdx * 4]));
        }

        if (m_spec.useDrawElements)
        {
            if (m_spec.useIndexBuffer)
            {
                if (m_spec.dynamicIndices)
                {
                    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_unbatchedDynamicIndexBuffers[drawNdx]);
                    gl.drawElements(GL_TRIANGLES, m_spec.triangleCount * 3, GL_UNSIGNED_BYTE, NULL);
                    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                }
                else
                    gl.drawElements(GL_TRIANGLES, m_spec.triangleCount * 3, GL_UNSIGNED_BYTE, NULL);
            }
            else
            {
                if (m_spec.dynamicIndices)
                    gl.drawElements(GL_TRIANGLES, m_spec.triangleCount * 3, GL_UNSIGNED_BYTE,
                                    &(m_dynamicIndexData[drawNdx * m_spec.triangleCount * 3]));
                else
                    gl.drawElements(GL_TRIANGLES, m_spec.triangleCount * 3, GL_UNSIGNED_BYTE, &(m_staticIndexData[0]));
            }
        }
        else
            gl.drawArrays(GL_TRIANGLES, 0, 3 * m_spec.triangleCount);
    }

    gl.finish();

    endUs = deGetMicroseconds();

    GLU_EXPECT_NO_ERROR(gl.getError(), "Unbatched rendering failed");

    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    for (int attribNdx = 0; attribNdx < m_spec.staticAttributeCount; attribNdx++)
    {
        GLint location = gl.getAttribLocation(m_program->getProgram(), ("a_static" + de::toString(attribNdx)).c_str());
        gl.disableVertexAttribArray(location);
    }

    for (int attribNdx = 0; attribNdx < m_spec.dynamicAttributeCount; attribNdx++)
        gl.disableVertexAttribArray(dynamicAttributeLocations[attribNdx]);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to reset state after unbatched rendering");

    return endUs - beginUs;
}

uint64_t DrawCallBatchingTest::renderBatched(void)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    uint64_t beginUs         = 0;
    uint64_t endUs           = 0;
    vector<GLint> dynamicAttributeLocations;

    gl.viewport(0, 0, 32, 32);
    gl.useProgram(m_program->getProgram());

    // Setup static buffers
    for (int attribNdx = 0; attribNdx < m_spec.staticAttributeCount; attribNdx++)
    {
        GLint location = gl.getAttribLocation(m_program->getProgram(), ("a_static" + de::toString(attribNdx)).c_str());

        gl.enableVertexAttribArray(location);

        if (m_spec.useStaticBuffer)
        {
            gl.bindBuffer(GL_ARRAY_BUFFER, m_batchedStaticBuffers[attribNdx]);
            gl.vertexAttribPointer(location, 4, GL_BYTE, GL_TRUE, 0, NULL);
            gl.bindBuffer(GL_ARRAY_BUFFER, 0);
        }
        else
            gl.vertexAttribPointer(location, 4, GL_BYTE, GL_TRUE, 0, &(m_staticAttributeDatas[attribNdx][0]));
    }

    // Get locations of dynamic attributes
    for (int attribNdx = 0; attribNdx < m_spec.dynamicAttributeCount; attribNdx++)
    {
        GLint location = gl.getAttribLocation(m_program->getProgram(), ("a_dyn" + de::toString(attribNdx)).c_str());

        gl.enableVertexAttribArray(location);
        dynamicAttributeLocations.push_back(location);
    }

    if (m_spec.useDrawElements && m_spec.useIndexBuffer && !m_spec.dynamicIndices)
        gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_batchedStaticIndexBuffer);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to setup initial state for rendering.");

    gl.finish();

    beginUs = deGetMicroseconds();

    for (int attribute = 0; attribute < m_spec.dynamicAttributeCount; attribute++)
    {
        if (m_spec.useDynamicBuffer)
        {
            gl.bindBuffer(GL_ARRAY_BUFFER, m_batchedDynamicBuffers[attribute]);
            gl.vertexAttribPointer(dynamicAttributeLocations[attribute], 4, GL_BYTE, GL_TRUE, 0, NULL);
            gl.bindBuffer(GL_ARRAY_BUFFER, 0);
        }
        else
            gl.vertexAttribPointer(dynamicAttributeLocations[attribute], 4, GL_BYTE, GL_TRUE, 0,
                                   &(m_dynamicAttributeDatas[attribute][0]));
    }

    if (m_spec.useDrawElements)
    {
        if (m_spec.useIndexBuffer)
        {
            if (m_spec.dynamicIndices)
            {
                gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_batchedDynamicIndexBuffer);
                gl.drawElements(GL_TRIANGLES, m_spec.triangleCount * 3 * m_spec.drawCallCount, GL_UNSIGNED_BYTE, NULL);
                gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }
            else
                gl.drawElements(GL_TRIANGLES, m_spec.triangleCount * 3 * m_spec.drawCallCount, GL_UNSIGNED_BYTE, NULL);
        }
        else
        {
            if (m_spec.dynamicIndices)
                gl.drawElements(GL_TRIANGLES, m_spec.triangleCount * 3 * m_spec.drawCallCount, GL_UNSIGNED_BYTE,
                                &(m_dynamicIndexData[0]));
            else
                gl.drawElements(GL_TRIANGLES, m_spec.triangleCount * 3 * m_spec.drawCallCount, GL_UNSIGNED_BYTE,
                                &(m_staticIndexData[0]));
        }
    }
    else
        gl.drawArrays(GL_TRIANGLES, 0, 3 * m_spec.triangleCount * m_spec.drawCallCount);

    gl.finish();

    endUs = deGetMicroseconds();

    GLU_EXPECT_NO_ERROR(gl.getError(), "Batched rendering failed");

    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    for (int attribNdx = 0; attribNdx < m_spec.staticAttributeCount; attribNdx++)
    {
        GLint location = gl.getAttribLocation(m_program->getProgram(), ("a_static" + de::toString(attribNdx)).c_str());
        gl.disableVertexAttribArray(location);
    }

    for (int attribNdx = 0; attribNdx < m_spec.dynamicAttributeCount; attribNdx++)
        gl.disableVertexAttribArray(dynamicAttributeLocations[attribNdx]);

    GLU_EXPECT_NO_ERROR(gl.getError(), "Failed to reset state after batched rendering");

    return endUs - beginUs;
}

struct Statistics
{
    double mean;
    double standardDeviation;
    double standardErrorOfMean;
};

Statistics calculateStats(const vector<uint64_t> &samples)
{
    double mean = 0.0;

    for (int i = 0; i < (int)samples.size(); i++)
        mean += (double)samples[i];

    mean /= (double)samples.size();

    double standardDeviation = 0.0;

    for (int i = 0; i < (int)samples.size(); i++)
    {
        double x = (double)samples[i];
        standardDeviation += (x - mean) * (x - mean);
    }

    standardDeviation /= (double)samples.size();
    standardDeviation = std::sqrt(standardDeviation);

    double standardErrorOfMean = standardDeviation / std::sqrt((double)samples.size());

    Statistics stats;

    stats.mean                = mean;
    stats.standardDeviation   = standardDeviation;
    stats.standardErrorOfMean = standardErrorOfMean;

    return stats;
}

void DrawCallBatchingTest::logTestInfo(void)
{
    TestLog &log = m_testCtx.getLog();
    tcu::ScopedLogSection section(log, "Test info", "Test info");

    log << TestLog::Message << "Rendering using " << (m_spec.useDrawElements ? "glDrawElements()" : "glDrawArrays()")
        << "." << TestLog::EndMessage;

    if (m_spec.useDrawElements)
        log << TestLog::Message << "Using " << (m_spec.dynamicIndices ? "dynamic " : "") << "indices from "
            << (m_spec.useIndexBuffer ? "buffer" : "pointer") << "." << TestLog::EndMessage;

    if (m_spec.staticAttributeCount > 0)
        log << TestLog::Message << "Using " << m_spec.staticAttributeCount << " static attribute"
            << (m_spec.staticAttributeCount > 1 ? "s" : "") << " from "
            << (m_spec.useStaticBuffer ? "buffer" : "pointer") << "." << TestLog::EndMessage;

    if (m_spec.dynamicAttributeCount > 0)
        log << TestLog::Message << "Using " << m_spec.dynamicAttributeCount << " dynamic attribute"
            << (m_spec.dynamicAttributeCount > 1 ? "s" : "") << " from "
            << (m_spec.useDynamicBuffer ? "buffer" : "pointer") << "." << TestLog::EndMessage;

    log << TestLog::Message << "Rendering " << m_spec.drawCallCount << " draw calls with " << m_spec.triangleCount
        << " triangles per call." << TestLog::EndMessage;
}

tcu::TestCase::IterateResult DrawCallBatchingTest::iterate(void)
{
    if (m_state == STATE_LOG_INFO)
    {
        logTestInfo();
        m_state = STATE_WARMUP_BATCHED;
    }
    else if (m_state == STATE_WARMUP_BATCHED)
    {
        renderBatched();
        m_state = STATE_WARMUP_UNBATCHED;
    }
    else if (m_state == STATE_WARMUP_UNBATCHED)
    {
        renderUnbatched();
        m_state = STATE_SAMPLE;
    }
    else if (m_state == STATE_SAMPLE)
    {
        if ((int)m_unbatchedSamplesUs.size() < m_unbatchedSampleCount &&
            ((double)m_unbatchedSamplesUs.size() / ((double)m_unbatchedSampleCount) <
                 (double)m_batchedSamplesUs.size() / ((double)m_batchedSampleCount) ||
             (int)m_batchedSamplesUs.size() >= m_batchedSampleCount))
            m_unbatchedSamplesUs.push_back(renderUnbatched());
        else if ((int)m_batchedSamplesUs.size() < m_batchedSampleCount)
            m_batchedSamplesUs.push_back(renderBatched());
        else
            m_state = STATE_CALC_CALIBRATION;
    }
    else if (m_state == STATE_CALC_CALIBRATION)
    {
        TestLog &log = m_testCtx.getLog();

        tcu::ScopedLogSection section(log, ("Sampling iteration " + de::toString(m_sampleIteration)).c_str(),
                                      ("Sampling iteration " + de::toString(m_sampleIteration)).c_str());
        const double targetSEM = 0.02;
        const double limitSEM  = 0.025;

        Statistics unbatchedStats = calculateStats(m_unbatchedSamplesUs);
        Statistics batchedStats   = calculateStats(m_batchedSamplesUs);

        log << TestLog::Message << "Batched samples; Count: " << m_batchedSamplesUs.size()
            << ", Mean: " << batchedStats.mean << "us, Standard deviation: " << batchedStats.standardDeviation
            << "us, Standard error of mean: " << batchedStats.standardErrorOfMean << "us("
            << (batchedStats.standardErrorOfMean / batchedStats.mean) << ")" << TestLog::EndMessage;
        log << TestLog::Message << "Unbatched samples; Count: " << m_unbatchedSamplesUs.size()
            << ", Mean: " << unbatchedStats.mean << "us, Standard deviation: " << unbatchedStats.standardDeviation
            << "us, Standard error of mean: " << unbatchedStats.standardErrorOfMean << "us("
            << (unbatchedStats.standardErrorOfMean / unbatchedStats.mean) << ")" << TestLog::EndMessage;

        if (m_sampleIteration > 2 ||
            (m_sampleIteration > 0 && (unbatchedStats.standardErrorOfMean / unbatchedStats.mean) +
                                              (batchedStats.standardErrorOfMean / batchedStats.mean) <=
                                          2.0 * limitSEM))
        {
            if (m_sampleIteration > 2)
                log << TestLog::Message << "Maximum iteration count reached." << TestLog::EndMessage;

            log << TestLog::Message << "Standard errors in target range." << TestLog::EndMessage;
            log << TestLog::Message << "Batched/Unbatched ratio: " << (batchedStats.mean / unbatchedStats.mean)
                << TestLog::EndMessage;

            m_testCtx.setTestResult(QP_TEST_RESULT_PASS,
                                    de::floatToString((float)(batchedStats.mean / unbatchedStats.mean), 1).c_str());
            return STOP;
        }
        else
        {
            if ((unbatchedStats.standardErrorOfMean / unbatchedStats.mean) > targetSEM)
                log << TestLog::Message << "Unbatched standard error of mean outside of range." << TestLog::EndMessage;

            if ((batchedStats.standardErrorOfMean / batchedStats.mean) > targetSEM)
                log << TestLog::Message << "Batched standard error of mean outside of range." << TestLog::EndMessage;

            if (unbatchedStats.standardDeviation > 0.0)
            {
                double x               = (unbatchedStats.standardDeviation / unbatchedStats.mean) / targetSEM;
                m_unbatchedSampleCount = std::max((int)m_unbatchedSamplesUs.size(), (int)(x * x));
            }
            else
                m_unbatchedSampleCount = (int)m_unbatchedSamplesUs.size();

            if (batchedStats.standardDeviation > 0.0)
            {
                double x             = (batchedStats.standardDeviation / batchedStats.mean) / targetSEM;
                m_batchedSampleCount = std::max((int)m_batchedSamplesUs.size(), (int)(x * x));
            }
            else
                m_batchedSampleCount = (int)m_batchedSamplesUs.size();

            m_batchedSamplesUs.clear();
            m_unbatchedSamplesUs.clear();

            m_sampleIteration++;
            m_state = STATE_SAMPLE;
        }
    }
    else
        DE_ASSERT(false);

    return CONTINUE;
}

string specToName(const DrawCallBatchingTest::TestSpec &spec)
{
    std::ostringstream stream;

    DE_ASSERT(!spec.useStaticBuffer || spec.staticAttributeCount > 0);
    DE_ASSERT(!spec.useDynamicBuffer || spec.dynamicAttributeCount > 0);

    if (spec.staticAttributeCount > 0)
        stream << spec.staticAttributeCount << "_static_";

    if (spec.useStaticBuffer)
        stream << (spec.staticAttributeCount == 1 ? "buffer_" : "buffers_");

    if (spec.dynamicAttributeCount > 0)
        stream << spec.dynamicAttributeCount << "_dynamic_";

    if (spec.useDynamicBuffer)
        stream << (spec.dynamicAttributeCount == 1 ? "buffer_" : "buffers_");

    stream << spec.triangleCount << "_triangles";

    return stream.str();
}

string specToDescrpition(const DrawCallBatchingTest::TestSpec &spec)
{
    DE_UNREF(spec);
    return "Test performance of batched rendering against non-batched rendering.";
}

} // namespace

DrawCallBatchingTests::DrawCallBatchingTests(Context &context)
    : TestCaseGroup(context, "draw_call_batching", "Draw call batching performance tests.")
{
}

DrawCallBatchingTests::~DrawCallBatchingTests(void)
{
}

void DrawCallBatchingTests::init(void)
{
    int drawCallCounts[] = {10, 100};

    int triangleCounts[] = {2, 10};

    int staticAttributeCounts[] = {1, 0, 4, 8, 0};

    int dynamicAttributeCounts[] = {0, 1, 4, 0, 8};

    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(staticAttributeCounts) == DE_LENGTH_OF_ARRAY(dynamicAttributeCounts));

    for (int drawType = 0; drawType < 2; drawType++)
    {
        bool drawElements = (drawType == 1);

        for (int indexBufferNdx = 0; indexBufferNdx < 2; indexBufferNdx++)
        {
            bool useIndexBuffer = (indexBufferNdx == 1);

            if (useIndexBuffer && !drawElements)
                continue;

            for (int dynamicIndexNdx = 0; dynamicIndexNdx < 2; dynamicIndexNdx++)
            {
                bool dynamicIndices = (dynamicIndexNdx == 1);

                if (dynamicIndices && !drawElements)
                    continue;

                if (dynamicIndices && !useIndexBuffer)
                    continue;

                TestCaseGroup *drawTypeGroup = new TestCaseGroup(
                    m_context,
                    (string(dynamicIndices ? "dynamic_" : "") + (useIndexBuffer ? "buffer_" : "") +
                     (drawElements ? "draw_elements" : "draw_arrays"))
                        .c_str(),
                    (string("Test batched rendering with ") + (drawElements ? "draw_elements" : "draw_arrays"))
                        .c_str());

                addChild(drawTypeGroup);

                for (int drawCallCountNdx = 0; drawCallCountNdx < DE_LENGTH_OF_ARRAY(drawCallCounts);
                     drawCallCountNdx++)
                {
                    int drawCallCount = drawCallCounts[drawCallCountNdx];

                    TestCaseGroup *callCountGroup = new TestCaseGroup(
                        m_context, (de::toString(drawCallCount) + (drawCallCount == 1 ? "_draw" : "_draws")).c_str(),
                        ("Test batched rendering performance with " + de::toString(drawCallCount) + " draw calls.")
                            .c_str());
                    TestCaseGroup *attributeCount1Group =
                        new TestCaseGroup(m_context, "1_attribute", "Test draw call batching with 1 attribute.");
                    TestCaseGroup *attributeCount8Group =
                        new TestCaseGroup(m_context, "8_attributes", "Test draw call batching with 8 attributes.");

                    callCountGroup->addChild(attributeCount1Group);
                    callCountGroup->addChild(attributeCount8Group);

                    drawTypeGroup->addChild(callCountGroup);

                    for (int attributeCountNdx = 0; attributeCountNdx < DE_LENGTH_OF_ARRAY(dynamicAttributeCounts);
                         attributeCountNdx++)
                    {
                        TestCaseGroup *attributeCountGroup = NULL;

                        int staticAttributeCount  = staticAttributeCounts[attributeCountNdx];
                        int dynamicAttributeCount = dynamicAttributeCounts[attributeCountNdx];

                        if (staticAttributeCount + dynamicAttributeCount == 1)
                            attributeCountGroup = attributeCount1Group;
                        else if (staticAttributeCount + dynamicAttributeCount == 8)
                            attributeCountGroup = attributeCount8Group;
                        else
                            DE_ASSERT(false);

                        for (int triangleCountNdx = 0; triangleCountNdx < DE_LENGTH_OF_ARRAY(triangleCounts);
                             triangleCountNdx++)
                        {
                            int triangleCount = triangleCounts[triangleCountNdx];

                            for (int dynamicBufferNdx = 0; dynamicBufferNdx < 2; dynamicBufferNdx++)
                            {
                                bool useDynamicBuffer = (dynamicBufferNdx != 0);

                                for (int staticBufferNdx = 0; staticBufferNdx < 2; staticBufferNdx++)
                                {
                                    bool useStaticBuffer = (staticBufferNdx != 0);

                                    DrawCallBatchingTest::TestSpec spec;

                                    spec.useStaticBuffer      = useStaticBuffer;
                                    spec.staticAttributeCount = staticAttributeCount;

                                    spec.useDynamicBuffer      = useDynamicBuffer;
                                    spec.dynamicAttributeCount = dynamicAttributeCount;

                                    spec.drawCallCount = drawCallCount;
                                    spec.triangleCount = triangleCount;

                                    spec.useDrawElements = drawElements;
                                    spec.useIndexBuffer  = useIndexBuffer;
                                    spec.dynamicIndices  = dynamicIndices;

                                    if (spec.useStaticBuffer && spec.staticAttributeCount == 0)
                                        continue;

                                    if (spec.useDynamicBuffer && spec.dynamicAttributeCount == 0)
                                        continue;

                                    attributeCountGroup->addChild(new DrawCallBatchingTest(
                                        m_context, specToName(spec).c_str(), specToDescrpition(spec).c_str(), spec));
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace Performance
} // namespace gles2
} // namespace deqp
