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
 * \brief Default vertex attribute test
 *//*--------------------------------------------------------------------*/

#include "es3fDefaultVertexAttributeTests.hpp"
#include "tcuVector.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuSurface.hpp"
#include "tcuTextureUtil.hpp"
#include "gluRenderContext.hpp"
#include "gluCallLogWrapper.hpp"
#include "gluShaderProgram.hpp"
#include "gluObjectWrapper.hpp"
#include "gluPixelTransfer.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "deMath.h"
#include "deStringUtil.hpp"
#include "deString.h"

#include <limits>

namespace deqp
{
namespace gles3
{
namespace Functional
{
namespace
{

static const int s_valueRange = 10;

static const char *const s_passThroughFragmentShaderSource = "#version 300 es\n"
                                                             "layout(location = 0) out mediump vec4 fragColor;\n"
                                                             "in mediump vec4 v_color;\n"
                                                             "void main (void)\n"
                                                             "{\n"
                                                             "    fragColor = v_color;\n"
                                                             "}\n";

template <typename T1, int S1, typename T2, int S2>
tcu::Vector<T1, S1> convertToTypeVec(const tcu::Vector<T2, S2> &v)
{
    tcu::Vector<T1, S1> retVal;

    for (int ndx = 0; ndx < S1; ++ndx)
        retVal[ndx] = T1(0);

    if (S1 == 4)
        retVal[3] = T1(1);

    for (int ndx = 0; ndx < de::min(S1, S2); ++ndx)
        retVal[ndx] = T1(v[ndx]);

    return retVal;
}

class FloatLoader
{
public:
    virtual ~FloatLoader(void)
    {
    }

    // returns the value loaded
    virtual tcu::Vec4 load(glu::CallLogWrapper &gl, int index, const tcu::Vec4 &v) const = 0;
};

#define GEN_DIRECT_FLOAT_LOADER(TYPE, COMPS, TYPECODE, CASENAME, VALUES)             \
    class LoaderVertexAttrib##COMPS##TYPECODE : public FloatLoader                   \
    {                                                                                \
    public:                                                                          \
        enum                                                                         \
        {                                                                            \
            NORMALIZING = 0,                                                         \
        };                                                                           \
        enum                                                                         \
        {                                                                            \
            COMPONENTS = (COMPS)                                                     \
        };                                                                           \
        typedef TYPE Type;                                                           \
                                                                                     \
        tcu::Vec4 load(glu::CallLogWrapper &gl, int index, const tcu::Vec4 &v) const \
        {                                                                            \
            tcu::Vector<TYPE, COMPONENTS> value;                                     \
            value = convertToTypeVec<Type, COMPONENTS>(v);                           \
                                                                                     \
            gl.glVertexAttrib##COMPS##TYPECODE VALUES;                               \
            return convertToTypeVec<float, 4>(value);                                \
        }                                                                            \
                                                                                     \
        static const char *getCaseName(void)                                         \
        {                                                                            \
            return CASENAME;                                                         \
        }                                                                            \
                                                                                     \
        static const char *getName(void)                                             \
        {                                                                            \
            return "VertexAttrib" #COMPS #TYPECODE;                                  \
        }                                                                            \
    }

#define GEN_INDIRECT_FLOAT_LOADER(TYPE, COMPS, TYPECODE, CASENAME)                   \
    class LoaderVertexAttrib##COMPS##TYPECODE : public FloatLoader                   \
    {                                                                                \
    public:                                                                          \
        enum                                                                         \
        {                                                                            \
            NORMALIZING = 0,                                                         \
        };                                                                           \
        enum                                                                         \
        {                                                                            \
            COMPONENTS = (COMPS)                                                     \
        };                                                                           \
        typedef TYPE Type;                                                           \
                                                                                     \
        tcu::Vec4 load(glu::CallLogWrapper &gl, int index, const tcu::Vec4 &v) const \
        {                                                                            \
            tcu::Vector<TYPE, COMPONENTS> value;                                     \
            value = convertToTypeVec<Type, COMPONENTS>(v);                           \
                                                                                     \
            gl.glVertexAttrib##COMPS##TYPECODE(index, value.getPtr());               \
            return convertToTypeVec<float, 4>(value);                                \
        }                                                                            \
                                                                                     \
        static const char *getCaseName(void)                                         \
        {                                                                            \
            return CASENAME;                                                         \
        }                                                                            \
                                                                                     \
        static const char *getName(void)                                             \
        {                                                                            \
            return "VertexAttrib" #COMPS #TYPECODE;                                  \
        }                                                                            \
    }

#define GEN_DIRECT_INTEGER_LOADER(TYPE, COMPS, TYPECODE, CASENAME, VALUES)           \
    class LoaderVertexAttribI##COMPS##TYPECODE : public FloatLoader                  \
    {                                                                                \
    public:                                                                          \
        enum                                                                         \
        {                                                                            \
            NORMALIZING = 0,                                                         \
        };                                                                           \
        enum                                                                         \
        {                                                                            \
            COMPONENTS = (COMPS)                                                     \
        };                                                                           \
        typedef TYPE Type;                                                           \
                                                                                     \
        tcu::Vec4 load(glu::CallLogWrapper &gl, int index, const tcu::Vec4 &v) const \
        {                                                                            \
            tcu::Vector<TYPE, COMPONENTS> value;                                     \
            value = convertToTypeVec<Type, COMPONENTS>(v);                           \
                                                                                     \
            gl.glVertexAttribI##COMPS##TYPECODE VALUES;                              \
            return convertToTypeVec<float, 4>(value);                                \
        }                                                                            \
                                                                                     \
        static const char *getCaseName(void)                                         \
        {                                                                            \
            return CASENAME;                                                         \
        }                                                                            \
                                                                                     \
        static const char *getName(void)                                             \
        {                                                                            \
            return "VertexAttrib" #COMPS #TYPECODE;                                  \
        }                                                                            \
    }

#define GEN_INDIRECT_INTEGER_LOADER(TYPE, COMPS, TYPECODE, CASENAME)                 \
    class LoaderVertexAttribI##COMPS##TYPECODE : public FloatLoader                  \
    {                                                                                \
    public:                                                                          \
        enum                                                                         \
        {                                                                            \
            NORMALIZING = 0,                                                         \
        };                                                                           \
        enum                                                                         \
        {                                                                            \
            COMPONENTS = (COMPS)                                                     \
        };                                                                           \
        typedef TYPE Type;                                                           \
                                                                                     \
        tcu::Vec4 load(glu::CallLogWrapper &gl, int index, const tcu::Vec4 &v) const \
        {                                                                            \
            tcu::Vector<TYPE, COMPONENTS> value;                                     \
            value = convertToTypeVec<Type, COMPONENTS>(v);                           \
                                                                                     \
            gl.glVertexAttribI##COMPS##TYPECODE(index, value.getPtr());              \
            return convertToTypeVec<float, 4>(value);                                \
        }                                                                            \
                                                                                     \
        static const char *getCaseName(void)                                         \
        {                                                                            \
            return CASENAME;                                                         \
        }                                                                            \
                                                                                     \
        static const char *getName(void)                                             \
        {                                                                            \
            return "VertexAttrib" #COMPS #TYPECODE;                                  \
        }                                                                            \
    }

GEN_DIRECT_FLOAT_LOADER(float, 1, f, "vertex_attrib_1f", (index, value.x()));
GEN_DIRECT_FLOAT_LOADER(float, 2, f, "vertex_attrib_2f", (index, value.x(), value.y()));
GEN_DIRECT_FLOAT_LOADER(float, 3, f, "vertex_attrib_3f", (index, value.x(), value.y(), value.z()));
GEN_DIRECT_FLOAT_LOADER(float, 4, f, "vertex_attrib_4f", (index, value.x(), value.y(), value.z(), value.w()));

GEN_INDIRECT_FLOAT_LOADER(float, 1, fv, "vertex_attrib_1fv");
GEN_INDIRECT_FLOAT_LOADER(float, 2, fv, "vertex_attrib_2fv");
GEN_INDIRECT_FLOAT_LOADER(float, 3, fv, "vertex_attrib_3fv");
GEN_INDIRECT_FLOAT_LOADER(float, 4, fv, "vertex_attrib_4fv");

GEN_DIRECT_INTEGER_LOADER(int32_t, 4, i, "vertex_attribi_4i", (index, value.x(), value.y(), value.z(), value.w()));
GEN_INDIRECT_INTEGER_LOADER(int32_t, 4, iv, "vertex_attribi_4iv");

GEN_DIRECT_INTEGER_LOADER(uint32_t, 4, ui, "vertex_attribi_4ui", (index, value.x(), value.y(), value.z(), value.w()));
GEN_INDIRECT_INTEGER_LOADER(uint32_t, 4, uiv, "vertex_attribi_4uiv");

class AttributeCase : public TestCase
{
    AttributeCase(Context &ctx, const char *name, const char *desc, const char *funcName, bool normalizing,
                  bool useNegative, glu::DataType dataType);

public:
    template <typename LoaderType>
    static AttributeCase *create(Context &ctx, glu::DataType dataType);
    ~AttributeCase(void);

private:
    void init(void);
    void deinit(void);
    IterateResult iterate(void);

    glu::DataType getTargetType(void) const;
    std::string genVertexSource(void) const;
    bool renderWithValue(const tcu::Vec4 &v);
    tcu::Vec4 computeColor(const tcu::Vec4 &value);
    bool verifyUnicoloredBuffer(const tcu::Surface &scene, const tcu::Vec4 &refValue);

    const bool m_normalizing;
    const bool m_useNegativeValues;
    const char *const m_funcName;
    const glu::DataType m_dataType;
    const FloatLoader *m_loader;
    glu::ShaderProgram *m_program;
    uint32_t m_bufID;
    bool m_allIterationsPassed;
    int m_iteration;

    enum
    {
        RENDER_SIZE = 32
    };
};

AttributeCase::AttributeCase(Context &ctx, const char *name, const char *desc, const char *funcName, bool normalizing,
                             bool useNegative, glu::DataType dataType)
    : TestCase(ctx, name, desc)
    , m_normalizing(normalizing)
    , m_useNegativeValues(useNegative)
    , m_funcName(funcName)
    , m_dataType(dataType)
    , m_loader(nullptr)
    , m_program(nullptr)
    , m_bufID(0)
    , m_allIterationsPassed(true)
    , m_iteration(0)
{
}

template <typename LoaderType>
AttributeCase *AttributeCase::create(Context &ctx, glu::DataType dataType)
{
    AttributeCase *retVal = new AttributeCase(
        ctx, LoaderType::getCaseName(), (std::string("Test ") + LoaderType::getName()).c_str(), LoaderType::getName(),
        LoaderType::NORMALIZING != 0, std::numeric_limits<typename LoaderType::Type>::is_signed, dataType);
    retVal->m_loader = new LoaderType();
    return retVal;
}

AttributeCase::~AttributeCase(void)
{
    deinit();
}

void AttributeCase::init(void)
{
    if (m_context.getRenderTarget().getWidth() < RENDER_SIZE || m_context.getRenderTarget().getHeight() < RENDER_SIZE)
        throw tcu::NotSupportedError("Render target must be at least " + de::toString<int>(RENDER_SIZE) + "x" +
                                     de::toString<int>(RENDER_SIZE));

    // log test info

    {
        const float maxRange = (m_normalizing) ? (1.0f) : (s_valueRange);
        const float minRange = (m_useNegativeValues) ? (-maxRange) : (0.0f);

        m_testCtx.getLog() << tcu::TestLog::Message << "Loading attribute values using " << m_funcName << "\n"
                           << "Attribute type: " << glu::getDataTypeName(m_dataType) << "\n"
                           << "Attribute value range: [" << minRange << ", " << maxRange << "]"
                           << tcu::TestLog::EndMessage;
    }

    // gen shader and base quad

    m_program = new glu::ShaderProgram(m_context.getRenderContext(),
                                       glu::ProgramSources() << glu::VertexSource(genVertexSource())
                                                             << glu::FragmentSource(s_passThroughFragmentShaderSource));
    m_testCtx.getLog() << *m_program;
    if (!m_program->isOk())
        throw tcu::TestError("could not build program");

    {
        const tcu::Vec4 fullscreenQuad[] = {
            tcu::Vec4(1.0f, 1.0f, 0.0f, 1.0f),
            tcu::Vec4(1.0f, -1.0f, 0.0f, 1.0f),
            tcu::Vec4(-1.0f, 1.0f, 0.0f, 1.0f),
            tcu::Vec4(-1.0f, -1.0f, 0.0f, 1.0f),
        };

        const glw::Functions &gl = m_context.getRenderContext().getFunctions();

        gl.genBuffers(1, &m_bufID);
        gl.bindBuffer(GL_ARRAY_BUFFER, m_bufID);
        gl.bufferData(GL_ARRAY_BUFFER, sizeof(fullscreenQuad), fullscreenQuad, GL_STATIC_DRAW);
        GLU_EXPECT_NO_ERROR(gl.getError(), "fill buffer");
    }
}

void AttributeCase::deinit(void)
{
    delete m_loader;
    m_loader = nullptr;

    delete m_program;
    m_program = nullptr;

    if (m_bufID)
    {
        m_context.getRenderContext().getFunctions().deleteBuffers(1, &m_bufID);
        m_bufID = 0;
    }
}

AttributeCase::IterateResult AttributeCase::iterate(void)
{
    static const tcu::Vec4 testValues[] = {
        tcu::Vec4(0.0f, 0.5f, 0.2f, 1.0f), tcu::Vec4(0.1f, 0.7f, 1.0f, 0.6f), tcu::Vec4(0.4f, 0.2f, 0.0f, 0.5f),
        tcu::Vec4(0.5f, 0.0f, 0.9f, 0.1f), tcu::Vec4(0.6f, 0.2f, 0.2f, 0.9f), tcu::Vec4(0.9f, 1.0f, 0.0f, 0.0f),
        tcu::Vec4(1.0f, 0.5f, 0.3f, 0.8f),
    };

    const tcu::ScopedLogSection section(m_testCtx.getLog(), "Iteration",
                                        "Iteration " + de::toString(m_iteration + 1) + "/" +
                                            de::toString(DE_LENGTH_OF_ARRAY(testValues)));

    // Test normalizing transfers with whole range, non-normalizing with up to s_valueRange
    const tcu::Vec4 testValue =
        ((m_useNegativeValues) ? (testValues[m_iteration] * 2.0f - tcu::Vec4(1.0f)) : (testValues[m_iteration])) *
        ((m_normalizing) ? (1.0f) : ((float)s_valueRange));

    if (!renderWithValue(testValue))
        m_allIterationsPassed = false;

    // continue

    if (++m_iteration < DE_LENGTH_OF_ARRAY(testValues))
        return CONTINUE;

    if (m_allIterationsPassed)
        m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
    else
        m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Got unexpected values");

    return STOP;
}

std::string AttributeCase::genVertexSource(void) const
{
    const int vectorSize         = (glu::isDataTypeMatrix(m_dataType)) ? (glu::getDataTypeMatrixNumRows(m_dataType)) :
                                   (glu::isDataTypeVector(m_dataType)) ? (glu::getDataTypeScalarSize(m_dataType)) :
                                                                         (-1);
    const char *const vectorType = glu::getDataTypeName(
        (glu::isDataTypeMatrix(m_dataType)) ? (glu::getDataTypeVector(glu::TYPE_FLOAT, vectorSize)) :
        (glu::isDataTypeVector(m_dataType)) ? (glu::getDataTypeVector(glu::TYPE_FLOAT, vectorSize)) :
                                              (glu::TYPE_FLOAT));
    const int components = (glu::isDataTypeMatrix(m_dataType)) ? (glu::getDataTypeMatrixNumRows(m_dataType)) :
                                                                 (glu::getDataTypeScalarSize(m_dataType));
    std::ostringstream buf;

    buf << "#version 300 es\n"
           "in highp vec4 a_position;\n"
           "in highp "
        << glu::getDataTypeName(m_dataType)
        << " a_value;\n"
           "out highp vec4 v_color;\n"
           "void main (void)\n"
           "{\n"
           "    gl_Position = a_position;\n"
           "\n";

    if (m_normalizing)
        buf << "    highp " << vectorType << " normalizedValue = "
            << ((glu::getDataTypeScalarType(m_dataType) == glu::TYPE_FLOAT) ? ("") : (vectorType)) << "(a_value"
            << ((glu::isDataTypeMatrix(m_dataType)) ? ("[1]") : ("")) << ");\n";
    else
        buf << "    highp " << vectorType << " normalizedValue = "
            << ((glu::getDataTypeScalarType(m_dataType) == glu::TYPE_FLOAT) ? ("") : (vectorType)) << "(a_value"
            << ((glu::isDataTypeMatrix(m_dataType)) ? ("[1]") : ("")) << ") / float(" << s_valueRange << ");\n";

    if (m_useNegativeValues)
        buf << "    highp " << vectorType << " positiveNormalizedValue = (normalizedValue + " << vectorType
            << "(1.0)) / 2.0;\n";
    else
        buf << "    highp " << vectorType << " positiveNormalizedValue = normalizedValue;\n";

    if (components == 1)
        buf << "    v_color = vec4(positiveNormalizedValue, 0.0, 0.0, 1.0);\n";
    else if (components == 2)
        buf << "    v_color = vec4(positiveNormalizedValue.xy, 0.0, 1.0);\n";
    else if (components == 3)
        buf << "    v_color = vec4(positiveNormalizedValue.xyz, 1.0);\n";
    else if (components == 4)
        buf << "    v_color = vec4((positiveNormalizedValue.xy + positiveNormalizedValue.zz) / 2.0, "
               "positiveNormalizedValue.w, 1.0);\n";
    else
        DE_ASSERT(false);

    buf << "}\n";

    return buf.str();
}

bool AttributeCase::renderWithValue(const tcu::Vec4 &v)
{
    glu::CallLogWrapper gl(m_context.getRenderContext().getFunctions(), m_testCtx.getLog());

    gl.enableLogging(true);

    const int positionIndex = gl.glGetAttribLocation(m_program->getProgram(), "a_position");
    const int valueIndex    = gl.glGetAttribLocation(m_program->getProgram(), "a_value");
    tcu::Surface dest(RENDER_SIZE, RENDER_SIZE);
    tcu::Vec4 loadedValue;

    gl.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl.glClear(GL_COLOR_BUFFER_BIT);
    gl.glViewport(0, 0, RENDER_SIZE, RENDER_SIZE);
    GLU_EXPECT_NO_ERROR(gl.glGetError(), "setup");

    gl.glBindBuffer(GL_ARRAY_BUFFER, m_bufID);
    gl.glVertexAttribPointer(positionIndex, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
    gl.glEnableVertexAttribArray(positionIndex);
    GLU_EXPECT_NO_ERROR(gl.glGetError(), "position va");

    // transfer test value. Load to the second column in the matrix case
    loadedValue = m_loader->load(gl, (glu::isDataTypeMatrix(m_dataType)) ? (valueIndex + 1) : (valueIndex), v);
    GLU_EXPECT_NO_ERROR(gl.glGetError(), "default va");

    gl.glUseProgram(m_program->getProgram());
    gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl.glUseProgram(0);
    GLU_EXPECT_NO_ERROR(gl.glGetError(), "draw");

    glu::readPixels(m_context.getRenderContext(), 0, 0, dest.getAccess());

    // check whole result is colored correctly
    return verifyUnicoloredBuffer(dest, computeColor(loadedValue));
}

tcu::Vec4 AttributeCase::computeColor(const tcu::Vec4 &value)
{
    const tcu::Vec4 normalizedValue = value / ((m_normalizing) ? (1.0f) : ((float)s_valueRange));
    const tcu::Vec4 positiveNormalizedValue =
        ((m_useNegativeValues) ? ((normalizedValue + tcu::Vec4(1.0f)) / 2.0f) : (normalizedValue));
    const int components = (glu::isDataTypeMatrix(m_dataType)) ? (glu::getDataTypeMatrixNumRows(m_dataType)) :
                                                                 (glu::getDataTypeScalarSize(m_dataType));

    if (components == 1)
        return tcu::Vec4(positiveNormalizedValue.x(), 0.0f, 0.0f, 1.0f);
    else if (components == 2)
        return tcu::Vec4(positiveNormalizedValue.x(), positiveNormalizedValue.y(), 0.0f, 1.0f);
    else if (components == 3)
        return tcu::Vec4(positiveNormalizedValue.x(), positiveNormalizedValue.y(), positiveNormalizedValue.z(), 1.0f);
    else if (components == 4)
        return tcu::Vec4((positiveNormalizedValue.x() + positiveNormalizedValue.z()) / 2.0f,
                         (positiveNormalizedValue.y() + positiveNormalizedValue.z()) / 2.0f,
                         positiveNormalizedValue.w(), 1.0f);
    else
        DE_ASSERT(false);

    return tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

bool AttributeCase::verifyUnicoloredBuffer(const tcu::Surface &scene, const tcu::Vec4 &refValue)
{
    tcu::Surface errorMask(RENDER_SIZE, RENDER_SIZE);
    const tcu::RGBA refColor(refValue);
    const int resultThreshold      = 2;
    const tcu::RGBA colorThreshold = m_context.getRenderTarget().getPixelFormat().getColorThreshold() * resultThreshold;
    bool error                     = false;

    tcu::RGBA exampleColor;
    tcu::IVec2 examplePos;

    tcu::clear(errorMask.getAccess(), tcu::RGBA::green().toIVec());

    m_testCtx.getLog() << tcu::TestLog::Message << "Verifying rendered image. Expecting color " << refColor
                       << ", threshold " << colorThreshold << tcu::TestLog::EndMessage;

    for (int y = 0; y < RENDER_SIZE; ++y)
        for (int x = 0; x < RENDER_SIZE; ++x)
        {
            const tcu::RGBA color = scene.getPixel(x, y);

            if (de::abs(color.getRed() - refColor.getRed()) > colorThreshold.getRed() ||
                de::abs(color.getGreen() - refColor.getGreen()) > colorThreshold.getGreen() ||
                de::abs(color.getBlue() - refColor.getBlue()) > colorThreshold.getBlue())
            {
                // first error
                if (!error)
                {
                    exampleColor = color;
                    examplePos   = tcu::IVec2(x, y);
                }

                error = true;
                errorMask.setPixel(x, y, tcu::RGBA::red());
            }
        }

    if (!error)
        m_testCtx.getLog() << tcu::TestLog::Message << "Rendered image is valid." << tcu::TestLog::EndMessage;
    else
    {
        m_testCtx.getLog() << tcu::TestLog::Message << "Found invalid pixel(s).\n"
                           << "Pixel at (" << examplePos.x() << ", " << examplePos.y() << ") color: " << exampleColor
                           << tcu::TestLog::EndMessage << tcu::TestLog::ImageSet("Result", "Render result")
                           << tcu::TestLog::Image("Result", "Result", scene)
                           << tcu::TestLog::Image("ErrorMask", "Error Mask", errorMask) << tcu::TestLog::EndImageSet;
    }

    return !error;
}

} // namespace

DefaultVertexAttributeTests::DefaultVertexAttributeTests(Context &context)
    : TestCaseGroup(context, "default_vertex_attrib", "Test default vertex attributes")
{
}

DefaultVertexAttributeTests::~DefaultVertexAttributeTests(void)
{
}

void DefaultVertexAttributeTests::init(void)
{
    struct Target
    {
        const char *name;
        glu::DataType dataType;
        bool reducedTestSets; // !< use reduced coverage
    };

    static const Target floatTargets[] = {
        {"float", glu::TYPE_FLOAT, false},        {"vec2", glu::TYPE_FLOAT_VEC2, true},
        {"vec3", glu::TYPE_FLOAT_VEC3, true},     {"vec4", glu::TYPE_FLOAT_VEC4, false},
        {"mat2", glu::TYPE_FLOAT_MAT2, true},     {"mat2x3", glu::TYPE_FLOAT_MAT2X3, true},
        {"mat2x4", glu::TYPE_FLOAT_MAT2X4, true}, {"mat3", glu::TYPE_FLOAT_MAT3, true},
        {"mat3x2", glu::TYPE_FLOAT_MAT3X2, true}, {"mat3x4", glu::TYPE_FLOAT_MAT3X4, true},
        {"mat4", glu::TYPE_FLOAT_MAT4, false},    {"mat4x2", glu::TYPE_FLOAT_MAT4X2, true},
        {"mat4x3", glu::TYPE_FLOAT_MAT4X3, true},
    };

    static const Target intTargets[] = {
        {"int", glu::TYPE_INT, false},
        {"ivec2", glu::TYPE_INT_VEC2, true},
        {"ivec3", glu::TYPE_INT_VEC3, true},
        {"ivec4", glu::TYPE_INT_VEC4, false},
    };

    static const Target uintTargets[] = {
        {"uint", glu::TYPE_UINT, false},
        {"uvec2", glu::TYPE_UINT_VEC2, true},
        {"uvec3", glu::TYPE_UINT_VEC3, true},
        {"uvec4", glu::TYPE_UINT_VEC4, false},
    };

    // float targets

    for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(floatTargets); ++targetNdx)
    {
        tcu::TestCaseGroup *const group =
            new tcu::TestCaseGroup(m_testCtx, floatTargets[targetNdx].name,
                                   (std::string("test with ") + floatTargets[targetNdx].name).c_str());
        const bool fullSet = !floatTargets[targetNdx].reducedTestSets;

#define ADD_CASE(X) group->addChild(AttributeCase::create<X>(m_context, floatTargets[targetNdx].dataType))
#define ADD_REDUCED_CASE(X) \
    if (fullSet)            \
    ADD_CASE(X)

        ADD_CASE(LoaderVertexAttrib1f);
        ADD_REDUCED_CASE(LoaderVertexAttrib2f);
        ADD_REDUCED_CASE(LoaderVertexAttrib3f);
        ADD_CASE(LoaderVertexAttrib4f);

        ADD_CASE(LoaderVertexAttrib1fv);
        ADD_REDUCED_CASE(LoaderVertexAttrib2fv);
        ADD_REDUCED_CASE(LoaderVertexAttrib3fv);
        ADD_CASE(LoaderVertexAttrib4fv);

#undef ADD_CASE
#undef ADD_REDUCED_CASE

        addChild(group);
    }

    // int targets

    for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(intTargets); ++targetNdx)
    {
        tcu::TestCaseGroup *const group = new tcu::TestCaseGroup(
            m_testCtx, intTargets[targetNdx].name, (std::string("test with ") + intTargets[targetNdx].name).c_str());

#define ADD_CASE(X) group->addChild(AttributeCase::create<X>(m_context, intTargets[targetNdx].dataType))

        ADD_CASE(LoaderVertexAttribI4i);
        ADD_CASE(LoaderVertexAttribI4iv);

#undef ADD_CASE

        addChild(group);
    }

    // uint targets

    for (int targetNdx = 0; targetNdx < DE_LENGTH_OF_ARRAY(uintTargets); ++targetNdx)
    {
        tcu::TestCaseGroup *const group = new tcu::TestCaseGroup(
            m_testCtx, uintTargets[targetNdx].name, (std::string("test with ") + uintTargets[targetNdx].name).c_str());

#define ADD_CASE(X) group->addChild(AttributeCase::create<X>(m_context, uintTargets[targetNdx].dataType))

        ADD_CASE(LoaderVertexAttribI4ui);
        ADD_CASE(LoaderVertexAttribI4uiv);

#undef ADD_CASE

        addChild(group);
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
