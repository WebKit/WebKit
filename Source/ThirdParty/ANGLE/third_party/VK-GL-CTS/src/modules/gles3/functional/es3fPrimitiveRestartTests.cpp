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
 * \brief Primitive restart tests.
 *//*--------------------------------------------------------------------*/

#include "es3fPrimitiveRestartTests.hpp"
#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRenderTarget.hpp"
#include "deRandom.hpp"
#include "deMath.h"
#include "deString.h"

#include "glw.h"

using tcu::Vec4;

namespace deqp
{
namespace gles3
{
namespace Functional
{

static const int MAX_RENDER_WIDTH  = 256;
static const int MAX_RENDER_HEIGHT = 256;

static const uint32_t MAX_UNSIGNED_BYTE  = (1 << 8) - 1;
static const uint32_t MAX_UNSIGNED_SHORT = (1 << 16) - 1;
static const uint32_t MAX_UNSIGNED_INT   = (uint32_t)((1ULL << 32) - 1);

static const uint8_t RESTART_INDEX_UNSIGNED_BYTE   = (uint8_t)MAX_UNSIGNED_BYTE;
static const uint16_t RESTART_INDEX_UNSIGNED_SHORT = (uint16_t)MAX_UNSIGNED_SHORT;
static const uint32_t RESTART_INDEX_UNSIGNED_INT   = MAX_UNSIGNED_INT;

class PrimitiveRestartCase : public TestCase
{
public:
    enum PrimitiveType
    {
        PRIMITIVE_POINTS = 0,
        PRIMITIVE_LINE_STRIP,
        PRIMITIVE_LINE_LOOP,
        PRIMITIVE_LINES,
        PRIMITIVE_TRIANGLE_STRIP,
        PRIMITIVE_TRIANGLE_FAN,
        PRIMITIVE_TRIANGLES,

        PRIMITIVE_LAST
    };

    enum IndexType
    {
        INDEX_UNSIGNED_BYTE = 0,
        INDEX_UNSIGNED_SHORT,
        INDEX_UNSIGNED_INT,

        INDEX_LAST
    };

    enum Function
    {
        FUNCTION_DRAW_ELEMENTS = 0,
        FUNCTION_DRAW_ELEMENTS_INSTANCED,
        FUNCTION_DRAW_RANGE_ELEMENTS,

        FUNCTION_LAST
    };

    PrimitiveRestartCase(Context &context, const char *name, const char *description, PrimitiveType primType,
                         IndexType indexType, Function function, bool beginWithRestart, bool endWithRestart,
                         bool duplicateRestarts);
    ~PrimitiveRestartCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    PrimitiveRestartCase(const PrimitiveRestartCase &other);
    PrimitiveRestartCase &operator=(const PrimitiveRestartCase &other);

    void draw(int startNdx, int count);

    void renderWithRestart(void);
    void renderWithoutRestart(void);

    // Helper functions for handling the appropriate index vector (according to m_indexType).
    void addIndex(uint32_t index);
    uint32_t getIndex(int indexNdx);
    int getNumIndices(void);
    void *getIndexPtr(int indexNdx);

    // \note Only one of the following index vectors is used (according to m_indexType).
    std::vector<uint8_t> m_indicesUB;
    std::vector<uint16_t> m_indicesUS;
    std::vector<uint32_t> m_indicesUI;

    std::vector<float> m_positions;

    PrimitiveType m_primType;
    IndexType m_indexType;
    Function m_function;

    bool m_beginWithRestart;  // Whether there will be restart indices at the beginning of the index array.
    bool m_endWithRestart;    // Whether there will be restart indices at the end of the index array.
    bool m_duplicateRestarts; // Whether two consecutive restarts are used instead of one.

    glu::ShaderProgram *m_program;
};

PrimitiveRestartCase::PrimitiveRestartCase(Context &context, const char *name, const char *description,
                                           PrimitiveType primType, IndexType indexType, Function function,
                                           bool beginWithRestart, bool endWithRestart, bool duplicateRestarts)
    : TestCase(context, name, description)
    , m_primType(primType)
    , m_indexType(indexType)
    , m_function(function)
    , m_beginWithRestart(beginWithRestart)
    , m_endWithRestart(endWithRestart)
    , m_duplicateRestarts(duplicateRestarts)
    , m_program(nullptr)
{
}

PrimitiveRestartCase::~PrimitiveRestartCase(void)
{
    PrimitiveRestartCase::deinit();
}

void PrimitiveRestartCase::deinit(void)
{
    delete m_program;
    m_program = nullptr;
}

void PrimitiveRestartCase::addIndex(uint32_t index)
{
    if (m_indexType == INDEX_UNSIGNED_BYTE)
    {
        DE_ASSERT(de::inRange(index, (uint32_t)0, MAX_UNSIGNED_BYTE));
        m_indicesUB.push_back((uint8_t)index);
    }
    else if (m_indexType == INDEX_UNSIGNED_SHORT)
    {
        DE_ASSERT(de::inRange(index, (uint32_t)0, MAX_UNSIGNED_SHORT));
        m_indicesUS.push_back((uint16_t)index);
    }
    else if (m_indexType == INDEX_UNSIGNED_INT)
    {
        DE_ASSERT(de::inRange(index, (uint32_t)0, MAX_UNSIGNED_INT));
        m_indicesUI.push_back((uint32_t)index);
    }
    else
        DE_ASSERT(false);
}

uint32_t PrimitiveRestartCase::getIndex(int indexNdx)
{
    switch (m_indexType)
    {
    case INDEX_UNSIGNED_BYTE:
        return (uint32_t)m_indicesUB[indexNdx];
    case INDEX_UNSIGNED_SHORT:
        return (uint32_t)m_indicesUS[indexNdx];
    case INDEX_UNSIGNED_INT:
        return m_indicesUI[indexNdx];
    default:
        DE_ASSERT(false);
        return 0;
    }
}

int PrimitiveRestartCase::getNumIndices(void)
{
    switch (m_indexType)
    {
    case INDEX_UNSIGNED_BYTE:
        return (int)m_indicesUB.size();
    case INDEX_UNSIGNED_SHORT:
        return (int)m_indicesUS.size();
    case INDEX_UNSIGNED_INT:
        return (int)m_indicesUI.size();
    default:
        DE_ASSERT(false);
        return 0;
    }
}

// Pointer to the index value at index indexNdx.
void *PrimitiveRestartCase::getIndexPtr(int indexNdx)
{
    switch (m_indexType)
    {
    case INDEX_UNSIGNED_BYTE:
        return (void *)&m_indicesUB[indexNdx];
    case INDEX_UNSIGNED_SHORT:
        return (void *)&m_indicesUS[indexNdx];
    case INDEX_UNSIGNED_INT:
        return (void *)&m_indicesUI[indexNdx];
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

void PrimitiveRestartCase::init(void)
{
    // Create shader program.
    std::string vertShaderSource;
    if (m_primType == PRIMITIVE_POINTS)
    {
        vertShaderSource = "#version 300 es\n"
                           "in highp vec4 a_position;\n"
                           "\n"
                           "void main()\n"
                           "{\n"
                           "    gl_Position = a_position;\n"
                           "    gl_PointSize = 1.0f;\n"
                           "}\n";
    }
    else
    {
        vertShaderSource = "#version 300 es\n"
                           "in highp vec4 a_position;\n"
                           "\n"
                           "void main()\n"
                           "{\n"
                           "    gl_Position = a_position;\n"
                           "}\n";
    }

    static const char *fragShaderSource = "#version 300 es\n"
                                          "layout(location = 0) out mediump vec4 o_color;\n"
                                          "\n"
                                          "void main()\n"
                                          "{\n"
                                          "    o_color = vec4(1.0f);\n"
                                          "}\n";

    DE_ASSERT(!m_program);
    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::makeVtxFragSources(vertShaderSource, fragShaderSource));

    if (!m_program->isOk())
    {
        m_testCtx.getLog() << *m_program;
        TCU_FAIL("Failed to compile shader");
    }

    uint32_t restartIndex = m_indexType == INDEX_UNSIGNED_BYTE  ? RESTART_INDEX_UNSIGNED_BYTE :
                            m_indexType == INDEX_UNSIGNED_SHORT ? RESTART_INDEX_UNSIGNED_SHORT :
                            m_indexType == INDEX_UNSIGNED_INT   ? RESTART_INDEX_UNSIGNED_INT :
                                                                  0;

    DE_ASSERT(restartIndex != 0);

    DE_ASSERT(getNumIndices() == 0);

    // If testing a case with restart at beginning, add it there.
    if (m_beginWithRestart)
    {
        addIndex(restartIndex);
        if (m_duplicateRestarts)
            addIndex(restartIndex);
    }

    // Generate vertex positions and indices depending on primitive type.
    // \note At this point, restarts shall not be added to the start or the end of the index vector. Those are special cases, and are done above and after the following if-else chain, respectively.

    if (m_primType == PRIMITIVE_POINTS)
    {
        // Generate rows with different numbers of points.

        uint32_t curIndex = 0;
        const int numRows = 20;

        for (int row = 0; row < numRows; row++)
        {
            for (int col = 0; col < row + 1; col++)
            {
                float fx = -1.0f + 2.0f * ((float)col + 0.5f) / (float)numRows;
                float fy = -1.0f + 2.0f * ((float)row + 0.5f) / (float)numRows;

                m_positions.push_back(fx);
                m_positions.push_back(fy);

                addIndex(curIndex++);
            }

            if (row < numRows - 1) // Add a restart after all but last row.
            {
                addIndex(restartIndex);
                if (m_duplicateRestarts)
                    addIndex(restartIndex);
            }
        }
    }
    else if (m_primType == PRIMITIVE_LINE_STRIP || m_primType == PRIMITIVE_LINE_LOOP || m_primType == PRIMITIVE_LINES)
    {
        // Generate a numRows x numCols arrangement of line polygons of different vertex counts.

        uint32_t curIndex = 0;
        const int numRows = 4;
        const int numCols = 4;

        for (int row = 0; row < numRows; row++)
        {
            float centerY = -1.0f + 2.0f * ((float)row + 0.5f) / (float)numRows;

            for (int col = 0; col < numCols; col++)
            {
                float centerX   = -1.0f + 2.0f * ((float)col + 0.5f) / (float)numCols;
                int numVertices = row * numCols + col + 1;

                for (int i = 0; i < numVertices; i++)
                {
                    float fx =
                        centerX + 0.9f * deFloatCos((float)i * 2.0f * DE_PI / (float)numVertices) / (float)numCols;
                    float fy =
                        centerY + 0.9f * deFloatSin((float)i * 2.0f * DE_PI / (float)numVertices) / (float)numRows;

                    m_positions.push_back(fx);
                    m_positions.push_back(fy);

                    addIndex(curIndex++);
                }

                if (col < numCols - 1 || row < numRows - 1) // Add a restart after all but last polygon.
                {
                    addIndex(restartIndex);
                    if (m_duplicateRestarts)
                        addIndex(restartIndex);
                }
            }
        }
    }
    else if (m_primType == PRIMITIVE_TRIANGLE_STRIP)
    {
        // Generate a number of horizontal triangle strips of different lengths.

        uint32_t curIndex   = 0;
        const int numStrips = 20;

        for (int stripNdx = 0; stripNdx < numStrips; stripNdx++)
        {
            int numVertices = stripNdx + 1;

            for (int i = 0; i < numVertices; i++)
            {
                float fx = -0.9f + 1.8f * (float)(i / 2 * 2) / numStrips;
                float fy = -0.9f + 1.8f * ((float)stripNdx + (i % 2 == 0 ? 0.0f : 0.8f)) / numStrips;

                m_positions.push_back(fx);
                m_positions.push_back(fy);

                addIndex(curIndex++);
            }

            if (stripNdx < numStrips - 1) // Add a restart after all but last strip.
            {
                addIndex(restartIndex);
                if (m_duplicateRestarts)
                    addIndex(restartIndex);
            }
        }
    }
    else if (m_primType == PRIMITIVE_TRIANGLE_FAN)
    {
        // Generate a numRows x numCols arrangement of triangle fan polygons of different vertex counts.

        uint32_t curIndex = 0;
        const int numRows = 4;
        const int numCols = 4;

        for (int row = 0; row < numRows; row++)
        {
            float centerY = -1.0f + 2.0f * ((float)row + 0.5f) / (float)numRows;

            for (int col = 0; col < numCols; col++)
            {
                float centerX      = -1.0f + 2.0f * ((float)col + 0.5f) / (float)numCols;
                int numArcVertices = row * numCols + col;

                m_positions.push_back(centerX);
                m_positions.push_back(centerY);

                addIndex(curIndex++);

                for (int i = 0; i < numArcVertices; i++)
                {
                    float fx =
                        centerX + 0.9f * deFloatCos((float)i * 2.0f * DE_PI / (float)numArcVertices) / (float)numCols;
                    float fy =
                        centerY + 0.9f * deFloatSin((float)i * 2.0f * DE_PI / (float)numArcVertices) / (float)numRows;

                    m_positions.push_back(fx);
                    m_positions.push_back(fy);

                    addIndex(curIndex++);
                }

                if (col < numCols - 1 || row < numRows - 1) // Add a restart after all but last polygon.
                {
                    addIndex(restartIndex);
                    if (m_duplicateRestarts)
                        addIndex(restartIndex);
                }
            }
        }
    }
    else if (m_primType == PRIMITIVE_TRIANGLES)
    {
        // Generate a number of rows with (potentially incomplete) triangles.

        uint32_t curIndex = 0;
        const int numRows = 3 * 7;

        for (int rowNdx = 0; rowNdx < numRows; rowNdx++)
        {
            int numVertices = rowNdx + 1;

            for (int i = 0; i < numVertices; i++)
            {
                float fx = -0.9f + 1.8f * ((float)(i / 3) + (i % 3 == 2 ? 0.8f : 0.0f)) * 3 / numRows;
                float fy = -0.9f + 1.8f * ((float)rowNdx + (i % 3 == 0 ? 0.0f : 0.8f)) / numRows;

                m_positions.push_back(fx);
                m_positions.push_back(fy);

                addIndex(curIndex++);
            }

            if (rowNdx < numRows - 1) // Add a restart after all but last row.
            {
                addIndex(restartIndex);
                if (m_duplicateRestarts)
                    addIndex(restartIndex);
            }
        }
    }
    else
        DE_ASSERT(false);

    // If testing a case with restart at end, add it there.
    if (m_endWithRestart)
    {
        addIndex(restartIndex);
        if (m_duplicateRestarts)
            addIndex(restartIndex);
    }

    // Special case assertions.

    int numIndices = getNumIndices();

    DE_ASSERT(numIndices > 0);
    DE_ASSERT(m_beginWithRestart ||
              getIndex(0) != restartIndex); // We don't want restarts at beginning unless the case is a special case.
    DE_ASSERT(m_endWithRestart || getIndex(numIndices - 1) !=
                                      restartIndex); // We don't want restarts at end unless the case is a special case.

    if (!m_duplicateRestarts)
        for (int i = 1; i < numIndices; i++)
            DE_ASSERT(getIndex(i) != restartIndex ||
                      getIndex(i - 1) !=
                          restartIndex); // We don't want duplicate restarts unless the case is a special case.
}

PrimitiveRestartCase::IterateResult PrimitiveRestartCase::iterate(void)
{
    int width  = deMin32(m_context.getRenderTarget().getWidth(), MAX_RENDER_WIDTH);
    int height = deMin32(m_context.getRenderTarget().getHeight(), MAX_RENDER_HEIGHT);

    int xOffsetMax = m_context.getRenderTarget().getWidth() - width;
    int yOffsetMax = m_context.getRenderTarget().getHeight() - height;

    de::Random rnd(deStringHash(getName()));

    int xOffset = rnd.getInt(0, xOffsetMax);
    int yOffset = rnd.getInt(0, yOffsetMax);
    tcu::Surface referenceImg(width, height);
    tcu::Surface resultImg(width, height);

    glViewport(xOffset, yOffset, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    uint32_t program = m_program->getProgram();
    glUseProgram(program);

    // Setup position attribute.

    int loc = glGetAttribLocation(program, "a_position");
    glEnableVertexAttribArray(loc);
    glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, &m_positions[0]);

    // Render result.

    renderWithRestart();
    glu::readPixels(m_context.getRenderContext(), xOffset, yOffset, resultImg.getAccess());

    // Render reference (same scene as the real deal, but emulate primitive restart without actually using it).

    renderWithoutRestart();
    glu::readPixels(m_context.getRenderContext(), xOffset, yOffset, referenceImg.getAccess());

    // Compare.

    bool testOk = tcu::pixelThresholdCompare(m_testCtx.getLog(), "ComparisonResult", "Image comparison result",
                                             referenceImg, resultImg, tcu::RGBA(0, 0, 0, 0), tcu::COMPARE_LOG_RESULT);

    m_testCtx.setTestResult(testOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL, testOk ? "Pass" : "Fail");

    glUseProgram(0);

    return STOP;
}

// Draw with the appropriate GLES3 draw function.
void PrimitiveRestartCase::draw(int startNdx, int count)
{
    GLenum primTypeGL;

    switch (m_primType)
    {
    case PRIMITIVE_POINTS:
        primTypeGL = GL_POINTS;
        break;
    case PRIMITIVE_LINE_STRIP:
        primTypeGL = GL_LINE_STRIP;
        break;
    case PRIMITIVE_LINE_LOOP:
        primTypeGL = GL_LINE_LOOP;
        break;
    case PRIMITIVE_LINES:
        primTypeGL = GL_LINES;
        break;
    case PRIMITIVE_TRIANGLE_STRIP:
        primTypeGL = GL_TRIANGLE_STRIP;
        break;
    case PRIMITIVE_TRIANGLE_FAN:
        primTypeGL = GL_TRIANGLE_FAN;
        break;
    case PRIMITIVE_TRIANGLES:
        primTypeGL = GL_TRIANGLES;
        break;
    default:
        DE_ASSERT(false);
        primTypeGL = 0;
    }

    GLenum indexTypeGL;

    switch (m_indexType)
    {
    case INDEX_UNSIGNED_BYTE:
        indexTypeGL = GL_UNSIGNED_BYTE;
        break;
    case INDEX_UNSIGNED_SHORT:
        indexTypeGL = GL_UNSIGNED_SHORT;
        break;
    case INDEX_UNSIGNED_INT:
        indexTypeGL = GL_UNSIGNED_INT;
        break;
    default:
        DE_ASSERT(false);
        indexTypeGL = 0;
    }

    uint32_t restartIndex = m_indexType == INDEX_UNSIGNED_BYTE  ? RESTART_INDEX_UNSIGNED_BYTE :
                            m_indexType == INDEX_UNSIGNED_SHORT ? RESTART_INDEX_UNSIGNED_SHORT :
                            m_indexType == INDEX_UNSIGNED_INT   ? RESTART_INDEX_UNSIGNED_INT :
                                                                  0;

    DE_ASSERT(restartIndex != 0);

    if (m_function == FUNCTION_DRAW_ELEMENTS)
        glDrawElements(primTypeGL, (GLsizei)count, indexTypeGL, (GLvoid *)getIndexPtr(startNdx));
    else if (m_function == FUNCTION_DRAW_ELEMENTS_INSTANCED)
        glDrawElementsInstanced(primTypeGL, (GLsizei)count, indexTypeGL, (GLvoid *)getIndexPtr(startNdx), 1);
    else
    {
        DE_ASSERT(m_function == FUNCTION_DRAW_RANGE_ELEMENTS);

        // Find the largest non-restart index in the index array (for glDrawRangeElements() end parameter).

        uint32_t max = 0;

        int numIndices = getNumIndices();
        for (int i = 0; i < numIndices; i++)
        {
            uint32_t index = getIndex(i);
            if (index != restartIndex && index > max)
                max = index;
        }

        glDrawRangeElements(primTypeGL, 0, (GLuint)max, (GLsizei)count, indexTypeGL, (GLvoid *)getIndexPtr(startNdx));
    }
}

void PrimitiveRestartCase::renderWithRestart(void)
{
    GLU_CHECK_MSG("PrimitiveRestartCase::renderWithRestart() begin");

    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    GLU_CHECK_MSG("Enable primitive restart");
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    GLU_CHECK_MSG("Clear in PrimitiveRestartCase::renderWithRestart()");

    draw(0, getNumIndices());

    GLU_CHECK_MSG("Draw in PrimitiveRestartCase::renderWithRestart()");

    GLU_CHECK_MSG("PrimitiveRestartCase::renderWithRestart() end");
}

void PrimitiveRestartCase::renderWithoutRestart(void)
{
    GLU_CHECK_MSG("PrimitiveRestartCase::renderWithoutRestart() begin");

    uint32_t restartIndex = m_indexType == INDEX_UNSIGNED_BYTE  ? RESTART_INDEX_UNSIGNED_BYTE :
                            m_indexType == INDEX_UNSIGNED_SHORT ? RESTART_INDEX_UNSIGNED_SHORT :
                            m_indexType == INDEX_UNSIGNED_INT   ? RESTART_INDEX_UNSIGNED_INT :
                                                                  0;

    DE_ASSERT(restartIndex != 0);

    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    GLU_CHECK_MSG("Disable primitive restart");
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    GLU_CHECK_MSG("Clear in PrimitiveRestartCase::renderWithoutRestart()");

    // Draw, emulating primitive restart.

    int numIndices = getNumIndices();

    DE_ASSERT(numIndices >= 0);

    int indexArrayStartNdx =
        0; // Keep track of the draw start index - first index after a primitive restart, or initially the first index altogether.

    for (int indexArrayNdx = 0; indexArrayNdx <= numIndices;
         indexArrayNdx++) // \note Goes one "too far" in order to detect end of array as well.
    {
        if (indexArrayNdx >= numIndices ||
            getIndex(indexArrayNdx) ==
                restartIndex) // \note Handle end of array the same way as a restart index encounter.
        {
            if (indexArrayStartNdx < numIndices)
            {
                // Draw from index indexArrayStartNdx to index indexArrayNdx-1 .

                draw(indexArrayStartNdx, indexArrayNdx - indexArrayStartNdx);
                GLU_CHECK_MSG("Draw in PrimitiveRestartCase::renderWithoutRestart()");
            }

            indexArrayStartNdx = indexArrayNdx + 1; // Next draw starts just after this restart index.
        }
    }

    GLU_CHECK_MSG("PrimitiveRestartCase::renderWithoutRestart() end");
}

PrimitiveRestartTests::PrimitiveRestartTests(Context &context)
    : TestCaseGroup(context, "primitive_restart", "Primitive restart tests")
{
}

PrimitiveRestartTests::~PrimitiveRestartTests(void)
{
}

void PrimitiveRestartTests::init(void)
{
    for (int isRestartBeginCaseI = 0; isRestartBeginCaseI <= 1; isRestartBeginCaseI++)
        for (int isRestartEndCaseI = 0; isRestartEndCaseI <= 1; isRestartEndCaseI++)
            for (int isDuplicateRestartCaseI = 0; isDuplicateRestartCaseI <= 1; isDuplicateRestartCaseI++)
            {
                bool isRestartBeginCase     = isRestartBeginCaseI != 0;
                bool isRestartEndCase       = isRestartEndCaseI != 0;
                bool isDuplicateRestartCase = isDuplicateRestartCaseI != 0;

                std::string specialCaseGroupName;

                if (isRestartBeginCase)
                    specialCaseGroupName = "begin_restart";
                if (isRestartEndCase)
                    specialCaseGroupName += std::string(specialCaseGroupName.empty() ? "" : "_") + "end_restart";
                if (isDuplicateRestartCase)
                    specialCaseGroupName += std::string(specialCaseGroupName.empty() ? "" : "_") + "duplicate_restarts";

                if (specialCaseGroupName.empty())
                    specialCaseGroupName = "basic";

                TestCaseGroup *specialCaseGroup = new TestCaseGroup(m_context, specialCaseGroupName.c_str(), "");
                addChild(specialCaseGroup);

                for (int primType = 0; primType < (int)PrimitiveRestartCase::PRIMITIVE_LAST; primType++)
                {
                    const char *primTypeName =
                        primType == (int)PrimitiveRestartCase::PRIMITIVE_POINTS         ? "points" :
                        primType == (int)PrimitiveRestartCase::PRIMITIVE_LINE_STRIP     ? "line_strip" :
                        primType == (int)PrimitiveRestartCase::PRIMITIVE_LINE_LOOP      ? "line_loop" :
                        primType == (int)PrimitiveRestartCase::PRIMITIVE_LINES          ? "lines" :
                        primType == (int)PrimitiveRestartCase::PRIMITIVE_TRIANGLE_STRIP ? "triangle_strip" :
                        primType == (int)PrimitiveRestartCase::PRIMITIVE_TRIANGLE_FAN   ? "triangle_fan" :
                        primType == (int)PrimitiveRestartCase::PRIMITIVE_TRIANGLES      ? "triangles" :
                                                                                          nullptr;

                    DE_ASSERT(primTypeName != nullptr);

                    TestCaseGroup *primTypeGroup = new TestCaseGroup(m_context, primTypeName, "");
                    specialCaseGroup->addChild(primTypeGroup);

                    for (int indexType = 0; indexType < (int)PrimitiveRestartCase::INDEX_LAST; indexType++)
                    {
                        const char *indexTypeName =
                            indexType == (int)PrimitiveRestartCase::INDEX_UNSIGNED_BYTE  ? "unsigned_byte" :
                            indexType == (int)PrimitiveRestartCase::INDEX_UNSIGNED_SHORT ? "unsigned_short" :
                            indexType == (int)PrimitiveRestartCase::INDEX_UNSIGNED_INT   ? "unsigned_int" :
                                                                                           nullptr;

                        DE_ASSERT(indexTypeName != nullptr);

                        TestCaseGroup *indexTypeGroup = new TestCaseGroup(m_context, indexTypeName, "");
                        primTypeGroup->addChild(indexTypeGroup);

                        for (int function = 0; function < (int)PrimitiveRestartCase::FUNCTION_LAST; function++)
                        {
                            const char *functionName =
                                function == (int)PrimitiveRestartCase::FUNCTION_DRAW_ELEMENTS ?
                                    "draw_elements" :
                                function == (int)PrimitiveRestartCase::FUNCTION_DRAW_ELEMENTS_INSTANCED ?
                                    "draw_elements_instanced" :
                                function == (int)PrimitiveRestartCase::FUNCTION_DRAW_RANGE_ELEMENTS ?
                                    "draw_range_elements" :
                                    nullptr;

                            DE_ASSERT(functionName != nullptr);

                            indexTypeGroup->addChild(new PrimitiveRestartCase(
                                m_context, functionName, "", (PrimitiveRestartCase::PrimitiveType)primType,
                                (PrimitiveRestartCase::IndexType)indexType, (PrimitiveRestartCase::Function)function,
                                isRestartBeginCase, isRestartEndCase, isDuplicateRestartCase));
                        }
                    }
                }
            }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
