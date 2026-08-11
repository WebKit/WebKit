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
 * \brief Instanced rendering tests.
 *//*--------------------------------------------------------------------*/

#include "es3fInstancedRenderingTests.hpp"
#include "gluPixelTransfer.hpp"
#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuImageCompare.hpp"
#include "tcuVector.hpp"
#include "tcuRenderTarget.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deString.h"

#include "glw.h"

using std::string;
using std::vector;

namespace deqp
{
namespace gles3
{
namespace Functional
{

static const int MAX_RENDER_WIDTH  = 128;
static const int MAX_RENDER_HEIGHT = 128;

static const int QUAD_GRID_SIZE = 127;

// Attribute divisors for the attributes defining the color's RGB components.
static const int ATTRIB_DIVISOR_R = 3;
static const int ATTRIB_DIVISOR_G = 2;
static const int ATTRIB_DIVISOR_B = 1;

static const int OFFSET_COMPONENTS =
    3; // \note Affects whether a float or a vecN is used in shader, but only first component is non-zero.

// Scale and bias values when converting float to integer, when attribute is of integer type.
static const float FLOAT_INT_SCALE  = 100.0f;
static const float FLOAT_INT_BIAS   = -50.0f;
static const float FLOAT_UINT_SCALE = 100.0f;
static const float FLOAT_UINT_BIAS  = 0.0f;

// \note Non-anonymous namespace needed; VarComp is used as a template parameter.
namespace vcns
{

union VarComp
{
    float f32;
    uint32_t u32;
    int32_t i32;

    VarComp(float v) : f32(v)
    {
    }
    VarComp(uint32_t v) : u32(v)
    {
    }
    VarComp(int32_t v) : i32(v)
    {
    }
};
DE_STATIC_ASSERT(sizeof(VarComp) == sizeof(uint32_t));

} // namespace vcns

using namespace vcns;

class InstancedRenderingCase : public TestCase
{
public:
    enum DrawFunction
    {
        FUNCTION_DRAW_ARRAYS_INSTANCED = 0,
        FUNCTION_DRAW_ELEMENTS_INSTANCED,

        FUNCTION_LAST
    };

    enum InstancingType
    {
        TYPE_INSTANCE_ID = 0,
        TYPE_ATTRIB_DIVISOR,
        TYPE_MIXED,

        TYPE_LAST
    };

    InstancedRenderingCase(Context &context, const char *name, const char *description, DrawFunction function,
                           InstancingType instancingType, glu::DataType rgbAttrType, int numInstances);
    ~InstancedRenderingCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

private:
    InstancedRenderingCase(const InstancedRenderingCase &other);
    InstancedRenderingCase &operator=(const InstancedRenderingCase &other);

    void pushVarCompAttrib(vector<VarComp> &vec, float val);

    void setupVarAttribPointer(const void *attrPtr, int startLocation, int divisor);
    void setupAndRender(void);
    void computeReference(tcu::Surface &dst);

    DrawFunction m_function;
    InstancingType m_instancingType;
    glu::DataType
        m_rgbAttrType; // \note Instance attribute types, color components only. Position offset attribute is always float/vecN.
    int m_numInstances;

    vector<float> m_gridVertexPositions; // X and Y components per vertex.
    vector<uint16_t> m_gridIndices;      // \note Only used if m_function is FUNCTION_DRAW_ELEMENTS_INSTANCED.

    // \note Some or all of the following instance attribute parameters may be unused with TYPE_INSTANCE_ID or TYPE_MIXED.
    vector<float> m_instanceOffsets; // Position offsets. OFFSET_COMPONENTS components per offset.
    // Attribute data for float, int or uint (or respective vector types) color components.
    vector<VarComp> m_instanceColorR;
    vector<VarComp> m_instanceColorG;
    vector<VarComp> m_instanceColorB;

    glu::ShaderProgram *m_program;
};

InstancedRenderingCase::InstancedRenderingCase(Context &context, const char *name, const char *description,
                                               DrawFunction function, InstancingType instancingType,
                                               glu::DataType rgbAttrType, int numInstances)
    : TestCase(context, name, description)
    , m_function(function)
    , m_instancingType(instancingType)
    , m_rgbAttrType(rgbAttrType)
    , m_numInstances(numInstances)
    , m_program(nullptr)
{
}

InstancedRenderingCase::~InstancedRenderingCase(void)
{
    InstancedRenderingCase::deinit();
}

// Helper function that does biasing and scaling when converting float to integer.
void InstancedRenderingCase::pushVarCompAttrib(vector<VarComp> &vec, float val)
{
    bool isFloatCase = glu::isDataTypeFloatOrVec(m_rgbAttrType);
    bool isIntCase   = glu::isDataTypeIntOrIVec(m_rgbAttrType);
    bool isUintCase  = glu::isDataTypeUintOrUVec(m_rgbAttrType);
    bool isMatCase   = glu::isDataTypeMatrix(m_rgbAttrType);

    if (isFloatCase || isMatCase)
        vec.push_back(VarComp(val));
    else if (isIntCase)
        vec.push_back(VarComp((int32_t)(val * FLOAT_INT_SCALE + FLOAT_INT_BIAS)));
    else if (isUintCase)
        vec.push_back(VarComp((uint32_t)(val * FLOAT_UINT_SCALE + FLOAT_UINT_BIAS)));
    else
        DE_ASSERT(false);
}

void InstancedRenderingCase::init(void)
{
    bool isFloatCase    = glu::isDataTypeFloatOrVec(m_rgbAttrType);
    bool isIntCase      = glu::isDataTypeIntOrIVec(m_rgbAttrType);
    bool isUintCase     = glu::isDataTypeUintOrUVec(m_rgbAttrType);
    bool isMatCase      = glu::isDataTypeMatrix(m_rgbAttrType);
    int typeSize        = glu::getDataTypeScalarSize(m_rgbAttrType);
    bool isScalarCase   = typeSize == 1;
    string swizzleFirst = isScalarCase ? "" : ".x";
    string typeName     = glu::getDataTypeName(m_rgbAttrType);

    string floatIntScaleStr  = "(" + de::floatToString(FLOAT_INT_SCALE, 3) + ")";
    string floatIntBiasStr   = "(" + de::floatToString(FLOAT_INT_BIAS, 3) + ")";
    string floatUintScaleStr = "(" + de::floatToString(FLOAT_UINT_SCALE, 3) + ")";
    string floatUintBiasStr  = "(" + de::floatToString(FLOAT_UINT_BIAS, 3) + ")";

    DE_ASSERT(isFloatCase || isIntCase || isUintCase || isMatCase);

    // Generate shader.
    // \note For case TYPE_MIXED, vertex position offset and color red component get their values from instance id, while green and blue get their values from instanced attributes.

    string numInstancesStr = de::toString(m_numInstances) + ".0";

    string instanceAttribs;
    string posExpression;
    string colorRExpression;
    string colorGExpression;
    string colorBExpression;

    if (m_instancingType == TYPE_INSTANCE_ID || m_instancingType == TYPE_MIXED)
    {
        posExpression    = "a_position + vec4(float(gl_InstanceID) * 2.0 / " + numInstancesStr + ", 0.0, 0.0, 0.0)";
        colorRExpression = "float(gl_InstanceID)/" + numInstancesStr;

        if (m_instancingType == TYPE_INSTANCE_ID)
        {
            colorGExpression = "float(gl_InstanceID)*2.0/" + numInstancesStr;
            colorBExpression = "1.0 - float(gl_InstanceID)/" + numInstancesStr;
        }
    }

    if (m_instancingType == TYPE_ATTRIB_DIVISOR || m_instancingType == TYPE_MIXED)
    {
        if (m_instancingType == TYPE_ATTRIB_DIVISOR)
        {
            posExpression = "a_position + vec4(a_instanceOffset";

            DE_STATIC_ASSERT(OFFSET_COMPONENTS >= 1 && OFFSET_COMPONENTS <= 4);

            for (int i = 0; i < 4 - OFFSET_COMPONENTS; i++)
                posExpression += ", 0.0";
            posExpression += ")";

            if (isFloatCase)
                colorRExpression = "a_instanceR" + swizzleFirst;
            else if (isIntCase)
                colorRExpression =
                    "(float(a_instanceR" + swizzleFirst + ") - " + floatIntBiasStr + ") / " + floatIntScaleStr;
            else if (isUintCase)
                colorRExpression =
                    "(float(a_instanceR" + swizzleFirst + ") - " + floatUintBiasStr + ") / " + floatUintScaleStr;
            else if (isMatCase)
                colorRExpression = "a_instanceR[0][0]";
            else
                DE_ASSERT(false);

            instanceAttribs += "in highp " +
                               (OFFSET_COMPONENTS == 1 ? string("float") : "vec" + de::toString(OFFSET_COMPONENTS)) +
                               " a_instanceOffset;\n";
            instanceAttribs += "in mediump " + typeName + " a_instanceR;\n";
        }

        if (isFloatCase)
        {
            colorGExpression = "a_instanceG" + swizzleFirst;
            colorBExpression = "a_instanceB" + swizzleFirst;
        }
        else if (isIntCase)
        {
            colorGExpression =
                "(float(a_instanceG" + swizzleFirst + ") - " + floatIntBiasStr + ") / " + floatIntScaleStr;
            colorBExpression =
                "(float(a_instanceB" + swizzleFirst + ") - " + floatIntBiasStr + ") / " + floatIntScaleStr;
        }
        else if (isUintCase)
        {
            colorGExpression =
                "(float(a_instanceG" + swizzleFirst + ") - " + floatUintBiasStr + ") / " + floatUintScaleStr;
            colorBExpression =
                "(float(a_instanceB" + swizzleFirst + ") - " + floatUintBiasStr + ") / " + floatUintScaleStr;
        }
        else if (isMatCase)
        {
            colorGExpression = "a_instanceG[0][0]";
            colorBExpression = "a_instanceB[0][0]";
        }
        else
            DE_ASSERT(false);

        instanceAttribs += "in mediump " + typeName + " a_instanceG;\n";
        instanceAttribs += "in mediump " + typeName + " a_instanceB;\n";
    }

    DE_ASSERT(!posExpression.empty());
    DE_ASSERT(!colorRExpression.empty());
    DE_ASSERT(!colorGExpression.empty());
    DE_ASSERT(!colorBExpression.empty());

    std::string vertShaderSourceStr = "#version 300 es\n"
                                      "in highp vec4 a_position;\n" +
                                      instanceAttribs +
                                      "out mediump vec4 v_color;\n"
                                      "\n"
                                      "void main()\n"
                                      "{\n"
                                      "    gl_Position = " +
                                      posExpression +
                                      ";\n"
                                      "    v_color.r = " +
                                      colorRExpression +
                                      ";\n"
                                      "    v_color.g = " +
                                      colorGExpression +
                                      ";\n"
                                      "    v_color.b = " +
                                      colorBExpression +
                                      ";\n"
                                      "    v_color.a = 1.0;\n"
                                      "}\n";

    static const char *fragShaderSource = "#version 300 es\n"
                                          "layout(location = 0) out mediump vec4 o_color;\n"
                                          "in mediump vec4 v_color;\n"
                                          "\n"
                                          "void main()\n"
                                          "{\n"
                                          "    o_color = v_color;\n"
                                          "}\n";

    // Create shader program and log it.

    DE_ASSERT(!m_program);
    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::makeVtxFragSources(vertShaderSourceStr, fragShaderSource));

    tcu::TestLog &log = m_testCtx.getLog();

    log << *m_program;

    if (!m_program->isOk())
        TCU_FAIL("Failed to compile shader");

    // Vertex shader attributes.

    if (m_function == FUNCTION_DRAW_ELEMENTS_INSTANCED)
    {
        // Vertex positions. Positions form a vertical bar of width <screen width>/<number of instances>.

        for (int y = 0; y < QUAD_GRID_SIZE + 1; y++)
            for (int x = 0; x < QUAD_GRID_SIZE + 1; x++)
            {
                float fx = -1.0f + (float)x / (float)QUAD_GRID_SIZE * 2.0f / (float)m_numInstances;
                float fy = -1.0f + (float)y / (float)QUAD_GRID_SIZE * 2.0f;

                m_gridVertexPositions.push_back(fx);
                m_gridVertexPositions.push_back(fy);
            }

        // Indices.

        for (int y = 0; y < QUAD_GRID_SIZE; y++)
            for (int x = 0; x < QUAD_GRID_SIZE; x++)
            {
                int ndx00 = y * (QUAD_GRID_SIZE + 1) + x;
                int ndx10 = y * (QUAD_GRID_SIZE + 1) + x + 1;
                int ndx01 = (y + 1) * (QUAD_GRID_SIZE + 1) + x;
                int ndx11 = (y + 1) * (QUAD_GRID_SIZE + 1) + x + 1;

                // Lower-left triangle of a quad.
                m_gridIndices.push_back((uint16_t)ndx00);
                m_gridIndices.push_back((uint16_t)ndx10);
                m_gridIndices.push_back((uint16_t)ndx01);

                // Upper-right triangle of a quad.
                m_gridIndices.push_back((uint16_t)ndx11);
                m_gridIndices.push_back((uint16_t)ndx01);
                m_gridIndices.push_back((uint16_t)ndx10);
            }
    }
    else
    {
        DE_ASSERT(m_function == FUNCTION_DRAW_ARRAYS_INSTANCED);

        // Vertex positions. Positions form a vertical bar of width <screen width>/<number of instances>.

        for (int y = 0; y < QUAD_GRID_SIZE; y++)
            for (int x = 0; x < QUAD_GRID_SIZE; x++)
            {
                float fx0 = -1.0f + (float)(x + 0) / (float)QUAD_GRID_SIZE * 2.0f / (float)m_numInstances;
                float fx1 = -1.0f + (float)(x + 1) / (float)QUAD_GRID_SIZE * 2.0f / (float)m_numInstances;
                float fy0 = -1.0f + (float)(y + 0) / (float)QUAD_GRID_SIZE * 2.0f;
                float fy1 = -1.0f + (float)(y + 1) / (float)QUAD_GRID_SIZE * 2.0f;

                // Vertices of a quad's lower-left triangle: (fx0, fy0), (fx1, fy0) and (fx0, fy1)
                m_gridVertexPositions.push_back(fx0);
                m_gridVertexPositions.push_back(fy0);
                m_gridVertexPositions.push_back(fx1);
                m_gridVertexPositions.push_back(fy0);
                m_gridVertexPositions.push_back(fx0);
                m_gridVertexPositions.push_back(fy1);

                // Vertices of a quad's upper-right triangle: (fx1, fy1), (fx0, fy1) and (fx1, fy0)
                m_gridVertexPositions.push_back(fx1);
                m_gridVertexPositions.push_back(fy1);
                m_gridVertexPositions.push_back(fx0);
                m_gridVertexPositions.push_back(fy1);
                m_gridVertexPositions.push_back(fx1);
                m_gridVertexPositions.push_back(fy0);
            }
    }

    // Instanced attributes: position offset and color RGB components.

    if (m_instancingType == TYPE_ATTRIB_DIVISOR || m_instancingType == TYPE_MIXED)
    {
        if (m_instancingType == TYPE_ATTRIB_DIVISOR)
        {
            // Offsets are such that the vertical bars are drawn next to each other.
            for (int i = 0; i < m_numInstances; i++)
            {
                m_instanceOffsets.push_back((float)i * 2.0f / (float)m_numInstances);

                DE_STATIC_ASSERT(OFFSET_COMPONENTS >= 1 && OFFSET_COMPONENTS <= 4);

                for (int j = 0; j < OFFSET_COMPONENTS - 1; j++)
                    m_instanceOffsets.push_back(0.0f);
            }

            int rInstances = m_numInstances / ATTRIB_DIVISOR_R + (m_numInstances % ATTRIB_DIVISOR_R == 0 ? 0 : 1);
            for (int i = 0; i < rInstances; i++)
            {
                pushVarCompAttrib(m_instanceColorR, (float)i / (float)rInstances);

                for (int j = 0; j < typeSize - 1; j++)
                    pushVarCompAttrib(m_instanceColorR, 0.0f);
            }
        }

        int gInstances = m_numInstances / ATTRIB_DIVISOR_G + (m_numInstances % ATTRIB_DIVISOR_G == 0 ? 0 : 1);
        for (int i = 0; i < gInstances; i++)
        {
            pushVarCompAttrib(m_instanceColorG, (float)i * 2.0f / (float)gInstances);

            for (int j = 0; j < typeSize - 1; j++)
                pushVarCompAttrib(m_instanceColorG, 0.0f);
        }

        int bInstances = m_numInstances / ATTRIB_DIVISOR_B + (m_numInstances % ATTRIB_DIVISOR_B == 0 ? 0 : 1);
        for (int i = 0; i < bInstances; i++)
        {
            pushVarCompAttrib(m_instanceColorB, 1.0f - (float)i / (float)bInstances);

            for (int j = 0; j < typeSize - 1; j++)
                pushVarCompAttrib(m_instanceColorB, 0.0f);
        }
    }
}

void InstancedRenderingCase::deinit(void)
{
    delete m_program;
    m_program = nullptr;
}

InstancedRenderingCase::IterateResult InstancedRenderingCase::iterate(void)
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

    // Draw result.

    glViewport(xOffset, yOffset, width, height);

    setupAndRender();

    glu::readPixels(m_context.getRenderContext(), xOffset, yOffset, resultImg.getAccess());

    // Compute reference.

    computeReference(referenceImg);

    // Compare.

    bool testOk = tcu::fuzzyCompare(m_testCtx.getLog(), "ComparisonResult", "Image comparison result", referenceImg,
                                    resultImg, 0.05f, tcu::COMPARE_LOG_RESULT);

    m_testCtx.setTestResult(testOk ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL, testOk ? "Pass" : "Fail");

    return STOP;
}

void InstancedRenderingCase::setupVarAttribPointer(const void *attrPtr, int location, int divisor)
{
    bool isFloatCase = glu::isDataTypeFloatOrVec(m_rgbAttrType);
    bool isIntCase   = glu::isDataTypeIntOrIVec(m_rgbAttrType);
    bool isUintCase  = glu::isDataTypeUintOrUVec(m_rgbAttrType);
    bool isMatCase   = glu::isDataTypeMatrix(m_rgbAttrType);
    int typeSize     = glu::getDataTypeScalarSize(m_rgbAttrType);
    int numSlots     = isMatCase ? glu::getDataTypeMatrixNumColumns(m_rgbAttrType) :
                                   1; // Matrix uses as many attribute slots as it has columns.

    for (int slotNdx = 0; slotNdx < numSlots; slotNdx++)
    {
        int curLoc = location + slotNdx;

        glEnableVertexAttribArray(curLoc);
        glVertexAttribDivisor(curLoc, divisor);

        if (isFloatCase)
            glVertexAttribPointer(curLoc, typeSize, GL_FLOAT, GL_FALSE, 0, attrPtr);
        else if (isIntCase)
            glVertexAttribIPointer(curLoc, typeSize, GL_INT, 0, attrPtr);
        else if (isUintCase)
            glVertexAttribIPointer(curLoc, typeSize, GL_UNSIGNED_INT, 0, attrPtr);
        else if (isMatCase)
        {
            int numRows = glu::getDataTypeMatrixNumRows(m_rgbAttrType);
            int numCols = glu::getDataTypeMatrixNumColumns(m_rgbAttrType);

            glVertexAttribPointer(curLoc, numRows, GL_FLOAT, GL_FALSE, numCols * numRows * (int)sizeof(float), attrPtr);
        }
        else
            DE_ASSERT(false);
    }
}

void InstancedRenderingCase::setupAndRender(void)
{
    uint32_t program = m_program->getProgram();

    glUseProgram(program);

    {
        // Setup attributes.

        // Position attribute is non-instanced.
        int positionLoc = glGetAttribLocation(program, "a_position");
        glEnableVertexAttribArray(positionLoc);
        glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, &m_gridVertexPositions[0]);

        if (m_instancingType == TYPE_ATTRIB_DIVISOR || m_instancingType == TYPE_MIXED)
        {
            if (m_instancingType == TYPE_ATTRIB_DIVISOR)
            {
                // Position offset attribute is instanced with separate offset for every instance.
                int offsetLoc = glGetAttribLocation(program, "a_instanceOffset");
                glEnableVertexAttribArray(offsetLoc);
                glVertexAttribDivisor(offsetLoc, 1);
                glVertexAttribPointer(offsetLoc, OFFSET_COMPONENTS, GL_FLOAT, GL_FALSE, 0, &m_instanceOffsets[0]);

                int rLoc = glGetAttribLocation(program, "a_instanceR");
                setupVarAttribPointer((void *)&m_instanceColorR[0].u32, rLoc, ATTRIB_DIVISOR_R);
            }

            int gLoc = glGetAttribLocation(program, "a_instanceG");
            setupVarAttribPointer((void *)&m_instanceColorG[0].u32, gLoc, ATTRIB_DIVISOR_G);

            int bLoc = glGetAttribLocation(program, "a_instanceB");
            setupVarAttribPointer((void *)&m_instanceColorB[0].u32, bLoc, ATTRIB_DIVISOR_B);
        }
    }

    // Draw using appropriate function.

    if (m_function == FUNCTION_DRAW_ARRAYS_INSTANCED)
    {
        const int numPositionComponents = 2;
        glDrawArraysInstanced(GL_TRIANGLES, 0, ((int)m_gridVertexPositions.size() / numPositionComponents),
                              m_numInstances);
    }
    else
        glDrawElementsInstanced(GL_TRIANGLES, (int)m_gridIndices.size(), GL_UNSIGNED_SHORT, &m_gridIndices[0],
                                m_numInstances);

    glUseProgram(0);
}

void InstancedRenderingCase::computeReference(tcu::Surface &dst)
{
    int wid = dst.getWidth();
    int hei = dst.getHeight();

    // Draw a rectangle (vertical bar) for each instance.

    for (int instanceNdx = 0; instanceNdx < m_numInstances; instanceNdx++)
    {
        int xStart = instanceNdx * wid / m_numInstances;
        int xEnd   = (instanceNdx + 1) * wid / m_numInstances;

        // Emulate attribute divisors if that is the case.

        int clrNdxR = m_instancingType == TYPE_ATTRIB_DIVISOR ? instanceNdx / ATTRIB_DIVISOR_R : instanceNdx;
        int clrNdxG = m_instancingType == TYPE_ATTRIB_DIVISOR || m_instancingType == TYPE_MIXED ?
                          instanceNdx / ATTRIB_DIVISOR_G :
                          instanceNdx;
        int clrNdxB = m_instancingType == TYPE_ATTRIB_DIVISOR || m_instancingType == TYPE_MIXED ?
                          instanceNdx / ATTRIB_DIVISOR_B :
                          instanceNdx;

        int rInstances = m_instancingType == TYPE_ATTRIB_DIVISOR ?
                             m_numInstances / ATTRIB_DIVISOR_R + (m_numInstances % ATTRIB_DIVISOR_R == 0 ? 0 : 1) :
                             m_numInstances;
        int gInstances = m_instancingType == TYPE_ATTRIB_DIVISOR || m_instancingType == TYPE_MIXED ?
                             m_numInstances / ATTRIB_DIVISOR_G + (m_numInstances % ATTRIB_DIVISOR_G == 0 ? 0 : 1) :
                             m_numInstances;
        int bInstances = m_instancingType == TYPE_ATTRIB_DIVISOR || m_instancingType == TYPE_MIXED ?
                             m_numInstances / ATTRIB_DIVISOR_B + (m_numInstances % ATTRIB_DIVISOR_B == 0 ? 0 : 1) :
                             m_numInstances;

        // Calculate colors.

        float r = (float)clrNdxR / (float)rInstances;
        float g = (float)clrNdxG * 2.0f / (float)gInstances;
        float b = 1.0f - (float)clrNdxB / (float)bInstances;

        // Convert to integer and back if shader inputs are integers.

        if (glu::isDataTypeIntOrIVec(m_rgbAttrType))
        {
            int32_t intR = (int32_t)(r * FLOAT_INT_SCALE + FLOAT_INT_BIAS);
            int32_t intG = (int32_t)(g * FLOAT_INT_SCALE + FLOAT_INT_BIAS);
            int32_t intB = (int32_t)(b * FLOAT_INT_SCALE + FLOAT_INT_BIAS);
            r            = ((float)intR - FLOAT_INT_BIAS) / FLOAT_INT_SCALE;
            g            = ((float)intG - FLOAT_INT_BIAS) / FLOAT_INT_SCALE;
            b            = ((float)intB - FLOAT_INT_BIAS) / FLOAT_INT_SCALE;
        }
        else if (glu::isDataTypeUintOrUVec(m_rgbAttrType))
        {
            uint32_t uintR = (int32_t)(r * FLOAT_UINT_SCALE + FLOAT_UINT_BIAS);
            uint32_t uintG = (int32_t)(g * FLOAT_UINT_SCALE + FLOAT_UINT_BIAS);
            uint32_t uintB = (int32_t)(b * FLOAT_UINT_SCALE + FLOAT_UINT_BIAS);
            r              = ((float)uintR - FLOAT_UINT_BIAS) / FLOAT_UINT_SCALE;
            g              = ((float)uintG - FLOAT_UINT_BIAS) / FLOAT_UINT_SCALE;
            b              = ((float)uintB - FLOAT_UINT_BIAS) / FLOAT_UINT_SCALE;
        }

        // Draw rectangle.

        for (int y = 0; y < hei; y++)
            for (int x = xStart; x < xEnd; x++)
                dst.setPixel(x, y, tcu::RGBA(tcu::Vec4(r, g, b, 1.0f)));
    }
}

InstancedRenderingTests::InstancedRenderingTests(Context &context)
    : TestCaseGroup(context, "instanced", "Instanced rendering tests")
{
}

InstancedRenderingTests::~InstancedRenderingTests(void)
{
}

void InstancedRenderingTests::init(void)
{
    // Cases testing function, instancing method and instance count.

    static const int instanceCounts[] = {1, 2, 4, 20};

    for (int function = 0; function < (int)InstancedRenderingCase::FUNCTION_LAST; function++)
    {
        const char *functionName =
            function == (int)InstancedRenderingCase::FUNCTION_DRAW_ARRAYS_INSTANCED   ? "draw_arrays_instanced" :
            function == (int)InstancedRenderingCase::FUNCTION_DRAW_ELEMENTS_INSTANCED ? "draw_elements_instanced" :
                                                                                        nullptr;

        const char *functionDesc = function == (int)InstancedRenderingCase::FUNCTION_DRAW_ARRAYS_INSTANCED ?
                                       "Use glDrawArraysInstanced()" :
                                   function == (int)InstancedRenderingCase::FUNCTION_DRAW_ELEMENTS_INSTANCED ?
                                       "Use glDrawElementsInstanced()" :
                                       nullptr;

        DE_ASSERT(functionName != nullptr);
        DE_ASSERT(functionDesc != nullptr);

        TestCaseGroup *functionGroup = new TestCaseGroup(m_context, functionName, functionDesc);
        addChild(functionGroup);

        for (int instancingType = 0; instancingType < (int)InstancedRenderingCase::TYPE_LAST; instancingType++)
        {
            const char *instancingTypeName =
                instancingType == (int)InstancedRenderingCase::TYPE_INSTANCE_ID    ? "instance_id" :
                instancingType == (int)InstancedRenderingCase::TYPE_ATTRIB_DIVISOR ? "attribute_divisor" :
                instancingType == (int)InstancedRenderingCase::TYPE_MIXED          ? "mixed" :
                                                                                     nullptr;

            const char *instancingTypeDesc = instancingType == (int)InstancedRenderingCase::TYPE_INSTANCE_ID ?
                                                 "Use gl_InstanceID for instancing" :
                                             instancingType == (int)InstancedRenderingCase::TYPE_ATTRIB_DIVISOR ?
                                                 "Use vertex attribute divisors for instancing" :
                                             instancingType == (int)InstancedRenderingCase::TYPE_MIXED ?
                                                 "Use both gl_InstanceID and vertex attribute divisors for instancing" :
                                                 nullptr;

            DE_ASSERT(instancingTypeName != nullptr);
            DE_ASSERT(instancingTypeDesc != nullptr);

            TestCaseGroup *instancingTypeGroup = new TestCaseGroup(m_context, instancingTypeName, instancingTypeDesc);
            functionGroup->addChild(instancingTypeGroup);

            for (int countNdx = 0; countNdx < DE_LENGTH_OF_ARRAY(instanceCounts); countNdx++)
            {
                std::string countName = de::toString(instanceCounts[countNdx]) + "_instances";

                instancingTypeGroup->addChild(new InstancedRenderingCase(
                    m_context, countName.c_str(), "", (InstancedRenderingCase::DrawFunction)function,
                    (InstancedRenderingCase::InstancingType)instancingType, glu::TYPE_FLOAT, instanceCounts[countNdx]));
            }
        }
    }

    // Data type specific cases.

    static const glu::DataType s_testTypes[] = {
        glu::TYPE_FLOAT,      glu::TYPE_FLOAT_VEC2,   glu::TYPE_FLOAT_VEC3,   glu::TYPE_FLOAT_VEC4,
        glu::TYPE_FLOAT_MAT2, glu::TYPE_FLOAT_MAT2X3, glu::TYPE_FLOAT_MAT2X4, glu::TYPE_FLOAT_MAT3X2,
        glu::TYPE_FLOAT_MAT3, glu::TYPE_FLOAT_MAT3X4, glu::TYPE_FLOAT_MAT4X2, glu::TYPE_FLOAT_MAT4X3,
        glu::TYPE_FLOAT_MAT4,

        glu::TYPE_INT,        glu::TYPE_INT_VEC2,     glu::TYPE_INT_VEC3,     glu::TYPE_INT_VEC4,

        glu::TYPE_UINT,       glu::TYPE_UINT_VEC2,    glu::TYPE_UINT_VEC3,    glu::TYPE_UINT_VEC4};

    const int typeTestNumInstances = 4;

    TestCaseGroup *typesGroup =
        new TestCaseGroup(m_context, "types", "Tests for instanced attributes of particular data types");
    addChild(typesGroup);

    for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(s_testTypes); typeNdx++)
    {
        glu::DataType type = s_testTypes[typeNdx];

        typesGroup->addChild(new InstancedRenderingCase(
            m_context, glu::getDataTypeName(type), "", InstancedRenderingCase::FUNCTION_DRAW_ARRAYS_INSTANCED,
            InstancedRenderingCase::TYPE_ATTRIB_DIVISOR, type, typeTestNumInstances));
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
