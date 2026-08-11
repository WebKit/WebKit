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
 * \brief Common built-in function tests.
 *//*--------------------------------------------------------------------*/

#include "es3fShaderCommonFunctionTests.hpp"
#include "glsShaderExecUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuFloat.hpp"
#include "deRandom.hpp"
#include "deMath.h"
#include "deString.h"

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::string;
using std::vector;
using tcu::TestLog;
using namespace gls::ShaderExecUtil;

using tcu::IVec2;
using tcu::IVec3;
using tcu::IVec4;
using tcu::Vec2;
using tcu::Vec3;
using tcu::Vec4;

// Utilities

template <typename T, int Size>
struct VecArrayAccess
{
public:
    VecArrayAccess(const void *ptr) : m_array((tcu::Vector<T, Size> *)ptr)
    {
    }
    ~VecArrayAccess(void)
    {
    }

    const tcu::Vector<T, Size> &operator[](size_t offset) const
    {
        return m_array[offset];
    }
    tcu::Vector<T, Size> &operator[](size_t offset)
    {
        return m_array[offset];
    }

private:
    tcu::Vector<T, Size> *m_array;
};

template <typename T, int Size>
static void fillRandomVectors(de::Random &rnd, const tcu::Vector<T, Size> &minValue,
                              const tcu::Vector<T, Size> &maxValue, void *dst, int numValues, int offset = 0)
{
    VecArrayAccess<T, Size> access(dst);
    for (int ndx = 0; ndx < numValues; ndx++)
        access[offset + ndx] = tcu::randomVector<T, Size>(rnd, minValue, maxValue);
}

template <typename T>
static void fillRandomScalars(de::Random &rnd, T minValue, T maxValue, void *dst, int numValues, int offset = 0)
{
    T *typedPtr = (T *)dst;
    for (int ndx = 0; ndx < numValues; ndx++)
        typedPtr[offset + ndx] = de::randomScalar<T>(rnd, minValue, maxValue);
}

inline int numBitsLostInOp(float input, float output)
{
    const int inExp  = tcu::Float32(input).exponent();
    const int outExp = tcu::Float32(output).exponent();

    return de::max(0, inExp - outExp); // Lost due to mantissa shift.
}

inline uint32_t getUlpDiff(float a, float b)
{
    const uint32_t aBits = tcu::Float32(a).bits();
    const uint32_t bBits = tcu::Float32(b).bits();
    return aBits > bBits ? aBits - bBits : bBits - aBits;
}

inline uint32_t getUlpDiffIgnoreZeroSign(float a, float b)
{
    if (tcu::Float32(a).isZero())
        return getUlpDiff(tcu::Float32::construct(tcu::Float32(b).sign(), 0, 0).asFloat(), b);
    else if (tcu::Float32(b).isZero())
        return getUlpDiff(a, tcu::Float32::construct(tcu::Float32(a).sign(), 0, 0).asFloat());
    else
        return getUlpDiff(a, b);
}

inline bool supportsSignedZero(glu::Precision precision)
{
    // \note GLSL ES 3.0 doesn't really require support for -0, but we require it for highp
    //         as it is very widely supported.
    return precision == glu::PRECISION_HIGHP;
}

inline float getEpsFromMaxUlpDiff(float value, uint32_t ulpDiff)
{
    const int exp = tcu::Float32(value).exponent();
    return tcu::Float32::construct(+1, exp, (1u << 23) | ulpDiff).asFloat() -
           tcu::Float32::construct(+1, exp, 1u << 23).asFloat();
}

inline uint32_t getMaxUlpDiffFromBits(int numAccurateBits)
{
    const int numGarbageBits = 23 - numAccurateBits;
    const uint32_t mask      = (1u << numGarbageBits) - 1u;

    return mask;
}

inline float getEpsFromBits(float value, int numAccurateBits)
{
    return getEpsFromMaxUlpDiff(value, getMaxUlpDiffFromBits(numAccurateBits));
}

static int getMinMantissaBits(glu::Precision precision)
{
    const int bits[] = {
        7,  // lowp
        10, // mediump
        23  // highp
    };
    DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(bits) == glu::PRECISION_LAST);
    DE_ASSERT(de::inBounds<int>(precision, 0, DE_LENGTH_OF_ARRAY(bits)));
    return bits[precision];
}

// CommonFunctionCase

class CommonFunctionCase : public TestCase
{
public:
    CommonFunctionCase(Context &context, const char *name, const char *description, glu::ShaderType shaderType);
    ~CommonFunctionCase(void);

    void init(void);
    void deinit(void);
    IterateResult iterate(void);

protected:
    CommonFunctionCase(const CommonFunctionCase &other);
    CommonFunctionCase &operator=(const CommonFunctionCase &other);

    virtual void getInputValues(int numValues, void *const *values) const       = 0;
    virtual bool compare(const void *const *inputs, const void *const *outputs) = 0;

    glu::ShaderType m_shaderType;
    ShaderSpec m_spec;
    int m_numValues;

    std::ostringstream m_failMsg; //!< Comparison failure help message.

private:
    ShaderExecutor *m_executor;
};

CommonFunctionCase::CommonFunctionCase(Context &context, const char *name, const char *description,
                                       glu::ShaderType shaderType)
    : TestCase(context, name, description)
    , m_shaderType(shaderType)
    , m_numValues(100)
    , m_executor(nullptr)
{
    m_spec.version = glu::GLSL_VERSION_300_ES;
}

CommonFunctionCase::~CommonFunctionCase(void)
{
    CommonFunctionCase::deinit();
}

void CommonFunctionCase::init(void)
{
    DE_ASSERT(!m_executor);

    m_executor = createExecutor(m_context.getRenderContext(), m_shaderType, m_spec);
    m_testCtx.getLog() << m_executor;

    if (!m_executor->isOk())
        throw tcu::TestError("Compile failed");
}

void CommonFunctionCase::deinit(void)
{
    delete m_executor;
    m_executor = nullptr;
}

static vector<int> getScalarSizes(const vector<Symbol> &symbols)
{
    vector<int> sizes(symbols.size());
    for (int ndx = 0; ndx < (int)symbols.size(); ++ndx)
        sizes[ndx] = symbols[ndx].varType.getScalarSize();
    return sizes;
}

static int computeTotalScalarSize(const vector<Symbol> &symbols)
{
    int totalSize = 0;
    for (vector<Symbol>::const_iterator sym = symbols.begin(); sym != symbols.end(); ++sym)
        totalSize += sym->varType.getScalarSize();
    return totalSize;
}

static vector<void *> getInputOutputPointers(const vector<Symbol> &symbols, vector<uint32_t> &data, const int numValues)
{
    vector<void *> pointers(symbols.size());
    int curScalarOffset = 0;

    for (int varNdx = 0; varNdx < (int)symbols.size(); ++varNdx)
    {
        const Symbol &var    = symbols[varNdx];
        const int scalarSize = var.varType.getScalarSize();

        // Uses planar layout as input/output specs do not support strides.
        pointers[varNdx] = &data[curScalarOffset];
        curScalarOffset += scalarSize * numValues;
    }

    DE_ASSERT(curScalarOffset == (int)data.size());

    return pointers;
}

// \todo [2013-08-08 pyry] Make generic utility and move to glu?

struct HexFloat
{
    const float value;
    HexFloat(const float value_) : value(value_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const HexFloat &v)
{
    return str << v.value << " / " << tcu::toHex(tcu::Float32(v.value).bits());
}

struct HexBool
{
    const uint32_t value;
    HexBool(const uint32_t value_) : value(value_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const HexBool &v)
{
    return str << (v.value ? "true" : "false") << " / " << tcu::toHex(v.value);
}

struct VarValue
{
    const glu::VarType &type;
    const void *value;

    VarValue(const glu::VarType &type_, const void *value_) : type(type_), value(value_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const VarValue &varValue)
{
    DE_ASSERT(varValue.type.isBasicType());

    const glu::DataType basicType  = varValue.type.getBasicType();
    const glu::DataType scalarType = glu::getDataTypeScalarType(basicType);
    const int numComponents        = glu::getDataTypeScalarSize(basicType);

    if (numComponents > 1)
        str << glu::getDataTypeName(basicType) << "(";

    for (int compNdx = 0; compNdx < numComponents; compNdx++)
    {
        if (compNdx != 0)
            str << ", ";

        switch (scalarType)
        {
        case glu::TYPE_FLOAT:
            str << HexFloat(((const float *)varValue.value)[compNdx]);
            break;
        case glu::TYPE_INT:
            str << ((const int32_t *)varValue.value)[compNdx];
            break;
        case glu::TYPE_UINT:
            str << tcu::toHex(((const uint32_t *)varValue.value)[compNdx]);
            break;
        case glu::TYPE_BOOL:
            str << HexBool(((const uint32_t *)varValue.value)[compNdx]);
            break;

        default:
            DE_ASSERT(false);
        }
    }

    if (numComponents > 1)
        str << ")";

    return str;
}

CommonFunctionCase::IterateResult CommonFunctionCase::iterate(void)
{
    const int numInputScalars  = computeTotalScalarSize(m_spec.inputs);
    const int numOutputScalars = computeTotalScalarSize(m_spec.outputs);
    vector<uint32_t> inputData(numInputScalars * m_numValues);
    vector<uint32_t> outputData(numOutputScalars * m_numValues);
    const vector<void *> inputPointers  = getInputOutputPointers(m_spec.inputs, inputData, m_numValues);
    const vector<void *> outputPointers = getInputOutputPointers(m_spec.outputs, outputData, m_numValues);

    // Initialize input data.
    getInputValues(m_numValues, &inputPointers[0]);

    // Execute shader.
    m_executor->useProgram();
    m_executor->execute(m_numValues, &inputPointers[0], &outputPointers[0]);

    // Compare results.
    {
        const vector<int> inScalarSizes  = getScalarSizes(m_spec.inputs);
        const vector<int> outScalarSizes = getScalarSizes(m_spec.outputs);
        vector<void *> curInputPtr(inputPointers.size());
        vector<void *> curOutputPtr(outputPointers.size());
        int numFailed = 0;

        for (int valNdx = 0; valNdx < m_numValues; valNdx++)
        {
            // Set up pointers for comparison.
            for (int inNdx = 0; inNdx < (int)curInputPtr.size(); ++inNdx)
                curInputPtr[inNdx] = (uint32_t *)inputPointers[inNdx] + inScalarSizes[inNdx] * valNdx;

            for (int outNdx = 0; outNdx < (int)curOutputPtr.size(); ++outNdx)
                curOutputPtr[outNdx] = (uint32_t *)outputPointers[outNdx] + outScalarSizes[outNdx] * valNdx;

            if (!compare(&curInputPtr[0], &curOutputPtr[0]))
            {
                // \todo [2013-08-08 pyry] We probably want to log reference value as well?

                m_testCtx.getLog() << TestLog::Message << "ERROR: comparison failed for value " << valNdx << ":\n  "
                                   << m_failMsg.str() << TestLog::EndMessage;

                m_testCtx.getLog() << TestLog::Message << "  inputs:" << TestLog::EndMessage;
                for (int inNdx = 0; inNdx < (int)curInputPtr.size(); inNdx++)
                    m_testCtx.getLog() << TestLog::Message << "    " << m_spec.inputs[inNdx].name << " = "
                                       << VarValue(m_spec.inputs[inNdx].varType, curInputPtr[inNdx])
                                       << TestLog::EndMessage;

                m_testCtx.getLog() << TestLog::Message << "  outputs:" << TestLog::EndMessage;
                for (int outNdx = 0; outNdx < (int)curOutputPtr.size(); outNdx++)
                    m_testCtx.getLog() << TestLog::Message << "    " << m_spec.outputs[outNdx].name << " = "
                                       << VarValue(m_spec.outputs[outNdx].varType, curOutputPtr[outNdx])
                                       << TestLog::EndMessage;

                m_failMsg.str("");
                m_failMsg.clear();
                numFailed += 1;
            }
        }

        m_testCtx.getLog() << TestLog::Message << (m_numValues - numFailed) << " / " << m_numValues << " values passed"
                           << TestLog::EndMessage;

        m_testCtx.setTestResult(numFailed == 0 ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL,
                                numFailed == 0 ? "Pass" : "Result comparison failed");
    }

    return STOP;
}

static std::string getCommonFuncCaseName(glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
{
    return string(glu::getDataTypeName(baseType)) + getPrecisionPostfix(precision) + getShaderTypePostfix(shaderType);
}

class AbsCase : public CommonFunctionCase
{
public:
    AbsCase(Context &context, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
        : CommonFunctionCase(context, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "abs", shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(baseType, precision)));
        m_spec.source = "out0 = abs(in0);";
    }

    void getInputValues(int numValues, void *const *values) const
    {
        const Vec2 floatRanges[] = {
            Vec2(-2.0f, 2.0f), // lowp
            Vec2(-1e3f, 1e3f), // mediump
            Vec2(-1e7f, 1e7f)  // highp
        };
        const IVec2 intRanges[] = {IVec2(-(1 << 7) + 1, (1 << 7) - 1), IVec2(-(1 << 15) + 1, (1 << 15) - 1),
                                   IVec2(0x80000001, 0x7fffffff)};

        de::Random rnd(deStringHash(getName()) ^ 0x235facu);
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        if (glu::isDataTypeFloatOrVec(type))
            fillRandomScalars(rnd, floatRanges[precision].x(), floatRanges[precision].y(), values[0],
                              numValues * scalarSize);
        else
            fillRandomScalars(rnd, intRanges[precision].x(), intRanges[precision].y(), values[0],
                              numValues * scalarSize);
    }

    bool compare(const void *const *inputs, const void *const *outputs)
    {
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        if (glu::isDataTypeFloatOrVec(type))
        {
            const int mantissaBits    = getMinMantissaBits(precision);
            const uint32_t maxUlpDiff = (1u << (23 - mantissaBits)) - 1u;

            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0         = ((const float *)inputs[0])[compNdx];
                const float out0        = ((const float *)outputs[0])[compNdx];
                const float ref0        = de::abs(in0);
                const uint32_t ulpDiff0 = getUlpDiff(out0, ref0);

                if (ulpDiff0 > maxUlpDiff)
                {
                    m_failMsg << "Expected [" << compNdx << "] = " << HexFloat(ref0) << " with ULP threshold "
                              << maxUlpDiff << ", got ULP diff " << ulpDiff0;
                    return false;
                }
            }
        }
        else
        {
            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const int in0  = ((const int *)inputs[0])[compNdx];
                const int out0 = ((const int *)outputs[0])[compNdx];
                const int ref0 = de::abs(in0);

                if (out0 != ref0)
                {
                    m_failMsg << "Expected [" << compNdx << "] = " << ref0;
                    return false;
                }
            }
        }

        return true;
    }
};

class SignCase : public CommonFunctionCase
{
public:
    SignCase(Context &context, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
        : CommonFunctionCase(context, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "sign",
                             shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(baseType, precision)));
        m_spec.source = "out0 = sign(in0);";
    }

    void getInputValues(int numValues, void *const *values) const
    {
        const Vec2 floatRanges[] = {
            Vec2(-2.0f, 2.0f), // lowp
            Vec2(-1e4f, 1e4f), // mediump    - note: may end up as inf
            Vec2(-1e8f, 1e8f)  // highp    - note: may end up as inf
        };
        const IVec2 intRanges[] = {IVec2(-(1 << 7), (1 << 7) - 1), IVec2(-(1 << 15), (1 << 15) - 1),
                                   IVec2(0x80000000, 0x7fffffff)};

        de::Random rnd(deStringHash(getName()) ^ 0x324u);
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        if (glu::isDataTypeFloatOrVec(type))
        {
            // Special cases.
            std::fill((float *)values[0], (float *)values[0] + scalarSize, +1.0f);
            std::fill((float *)values[0] + scalarSize * 1, (float *)values[0] + scalarSize * 2, -1.0f);
            std::fill((float *)values[0] + scalarSize * 2, (float *)values[0] + scalarSize * 3, 0.0f);
            fillRandomScalars(rnd, floatRanges[precision].x(), floatRanges[precision].y(),
                              (float *)values[0] + scalarSize * 3, (numValues - 3) * scalarSize);
        }
        else
        {
            std::fill((int *)values[0], (int *)values[0] + scalarSize, +1);
            std::fill((int *)values[0] + scalarSize * 1, (int *)values[0] + scalarSize * 2, -1);
            std::fill((int *)values[0] + scalarSize * 2, (int *)values[0] + scalarSize * 3, 0);
            fillRandomScalars(rnd, intRanges[precision].x(), intRanges[precision].y(),
                              (int *)values[0] + scalarSize * 3, (numValues - 3) * scalarSize);
        }
    }

    bool compare(const void *const *inputs, const void *const *outputs)
    {
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        if (glu::isDataTypeFloatOrVec(type))
        {
            // Both highp and mediump should be able to represent -1, 0, and +1 exactly
            const uint32_t maxUlpDiff =
                precision == glu::PRECISION_LOWP ? getMaxUlpDiffFromBits(getMinMantissaBits(precision)) : 0;

            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0         = ((const float *)inputs[0])[compNdx];
                const float out0        = ((const float *)outputs[0])[compNdx];
                const float ref0        = in0 < 0.0f ? -1.0f : in0 > 0.0f ? +1.0f : 0.0f;
                const uint32_t ulpDiff0 = getUlpDiff(out0, ref0);

                if (ulpDiff0 > maxUlpDiff)
                {
                    m_failMsg << "Expected [" << compNdx << "] = " << HexFloat(ref0) << " with ULP threshold "
                              << maxUlpDiff << ", got ULP diff " << ulpDiff0;
                    return false;
                }
            }
        }
        else
        {
            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const int in0  = ((const int *)inputs[0])[compNdx];
                const int out0 = ((const int *)outputs[0])[compNdx];
                const int ref0 = in0 < 0 ? -1 : in0 > 0 ? +1 : 0;

                if (out0 != ref0)
                {
                    m_failMsg << "Expected [" << compNdx << "] = " << ref0;
                    return false;
                }
            }
        }

        return true;
    }
};

static float roundEven(float v)
{
    const float q       = deFloatFrac(v);
    const int truncated = int(v - q);
    const int        rounded = (q > 0.5f)                            ? (truncated + 1) : // Rounded up
                                    (q == 0.5f && (truncated % 2 != 0))    ? (truncated + 1) : // Round to nearest even at 0.5
                                    truncated;                                                // Rounded down

    return float(rounded);
}

class RoundEvenCase : public CommonFunctionCase
{
public:
    RoundEvenCase(Context &context, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
        : CommonFunctionCase(context, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "roundEven",
                             shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(baseType, precision)));
        m_spec.source = "out0 = roundEven(in0);";
    }

    void getInputValues(int numValues, void *const *values) const
    {
        const Vec2 ranges[] = {
            Vec2(-2.0f, 2.0f), // lowp
            Vec2(-1e3f, 1e3f), // mediump
            Vec2(-1e7f, 1e7f)  // highp
        };

        de::Random rnd(deStringHash(getName()) ^ 0xac23fu);
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);
        int numSpecialCases            = 0;

        // Special cases.
        if (precision != glu::PRECISION_LOWP)
        {
            DE_ASSERT(numValues >= 20);
            for (int ndx = 0; ndx < 20; ndx++)
            {
                const float v = de::clamp(float(ndx) - 10.5f, ranges[precision].x(), ranges[precision].y());
                std::fill((float *)values[0], (float *)values[0] + scalarSize, v);
                numSpecialCases += 1;
            }
        }

        // Random cases.
        fillRandomScalars(rnd, ranges[precision].x(), ranges[precision].y(),
                          (float *)values[0] + numSpecialCases * scalarSize,
                          (numValues - numSpecialCases) * scalarSize);

        // If precision is mediump, make sure values can be represented in fp16 exactly
        if (precision == glu::PRECISION_MEDIUMP)
        {
            for (int ndx = 0; ndx < numValues * scalarSize; ndx++)
                ((float *)values[0])[ndx] = tcu::Float16(((float *)values[0])[ndx]).asFloat();
        }
    }

    bool compare(const void *const *inputs, const void *const *outputs)
    {
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const bool hasSignedZero       = supportsSignedZero(precision);
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        if (precision == glu::PRECISION_HIGHP || precision == glu::PRECISION_MEDIUMP)
        {
            // Require exact rounding result.
            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0  = ((const float *)inputs[0])[compNdx];
                const float out0 = ((const float *)outputs[0])[compNdx];
                const float ref  = roundEven(in0);

                const uint32_t ulpDiff = hasSignedZero ? getUlpDiff(out0, ref) : getUlpDiffIgnoreZeroSign(out0, ref);

                if (ulpDiff > 0)
                {
                    m_failMsg << "Expected [" << compNdx << "] = " << HexFloat(ref) << ", got ULP diff "
                              << tcu::toHex(ulpDiff);
                    return false;
                }
            }
        }
        else
        {
            const int mantissaBits    = getMinMantissaBits(precision);
            const uint32_t maxUlpDiff = getMaxUlpDiffFromBits(mantissaBits); // ULP diff for rounded integer value.
            const float eps           = getEpsFromBits(1.0f, mantissaBits);  // epsilon for rounding bounds

            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0  = ((const float *)inputs[0])[compNdx];
                const float out0 = ((const float *)outputs[0])[compNdx];
                const int minRes = int(roundEven(in0 - eps));
                const int maxRes = int(roundEven(in0 + eps));
                bool anyOk       = false;

                for (int roundedVal = minRes; roundedVal <= maxRes; roundedVal++)
                {
                    const uint32_t ulpDiff = getUlpDiffIgnoreZeroSign(out0, float(roundedVal));

                    if (ulpDiff <= maxUlpDiff)
                    {
                        anyOk = true;
                        break;
                    }
                }

                if (!anyOk)
                {
                    m_failMsg << "Expected [" << compNdx << "] = [" << minRes << ", " << maxRes
                              << "] with ULP threshold " << tcu::toHex(maxUlpDiff);
                    return false;
                }
            }
        }

        return true;
    }
};

class ModfCase : public CommonFunctionCase
{
public:
    ModfCase(Context &context, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
        : CommonFunctionCase(context, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "modf",
                             shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(baseType, precision)));
        m_spec.outputs.push_back(Symbol("out1", glu::VarType(baseType, precision)));
        m_spec.source = "out0 = modf(in0, out1);";
    }

    void getInputValues(int numValues, void *const *values) const
    {
        const Vec2 ranges[] = {
            Vec2(-2.0f, 2.0f), // lowp
            Vec2(-1e3f, 1e3f), // mediump
            Vec2(-1e7f, 1e7f)  // highp
        };

        de::Random rnd(deStringHash(getName()) ^ 0xac23fu);
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        fillRandomScalars(rnd, ranges[precision].x(), ranges[precision].y(), values[0], numValues * scalarSize);
    }

    bool compare(const void *const *inputs, const void *const *outputs)
    {
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const bool hasZeroSign         = supportsSignedZero(precision);
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        const int mantissaBits = getMinMantissaBits(precision);

        for (int compNdx = 0; compNdx < scalarSize; compNdx++)
        {
            const float in0  = ((const float *)inputs[0])[compNdx];
            const float out0 = ((const float *)outputs[0])[compNdx];
            const float out1 = ((const float *)outputs[1])[compNdx];

            const float refOut1 = float(int(in0));
            const float refOut0 = in0 - refOut1;

            const int bitsLost        = precision != glu::PRECISION_HIGHP ? numBitsLostInOp(in0, refOut0) : 0;
            const uint32_t maxUlpDiff = getMaxUlpDiffFromBits(de::max(mantissaBits - bitsLost, 0));

            const float resSum = out0 + out1;

            const uint32_t ulpDiff = hasZeroSign ? getUlpDiff(resSum, in0) : getUlpDiffIgnoreZeroSign(resSum, in0);

            if (ulpDiff > maxUlpDiff)
            {
                m_failMsg << "Expected [" << compNdx << "] = (" << HexFloat(refOut0) << ") + (" << HexFloat(refOut1)
                          << ") = " << HexFloat(in0) << " with ULP threshold " << tcu::toHex(maxUlpDiff)
                          << ", got ULP diff " << tcu::toHex(ulpDiff);
                return false;
            }
        }

        return true;
    }
};

class IsnanCase : public CommonFunctionCase
{
public:
    IsnanCase(Context &context, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
        : CommonFunctionCase(context, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "isnan",
                             shaderType)
    {
        DE_ASSERT(glu::isDataTypeFloatOrVec(baseType));

        const int vecSize            = glu::getDataTypeScalarSize(baseType);
        const glu::DataType boolType = vecSize > 1 ? glu::getDataTypeBoolVec(vecSize) : glu::TYPE_BOOL;

        m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(boolType, glu::PRECISION_LAST)));
        m_spec.source = "out0 = isnan(in0);";
    }

    void getInputValues(int numValues, void *const *values) const
    {
        de::Random rnd(deStringHash(getName()) ^ 0xc2a39fu);
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);
        const int mantissaBits         = getMinMantissaBits(precision);
        const uint32_t mantissaMask    = ~getMaxUlpDiffFromBits(mantissaBits) & ((1u << 23) - 1u);

        for (int valNdx = 0; valNdx < numValues * scalarSize; valNdx++)
        {
            const bool isNan        = rnd.getFloat() > 0.3f;
            const bool isInf        = !isNan && rnd.getFloat() > 0.4f;
            const uint32_t mantissa = !isInf ? ((1u << 22) | (rnd.getUint32() & mantissaMask)) : 0;
            const uint32_t exp      = !isNan && !isInf ? (rnd.getUint32() & 0x7fu) : 0xffu;
            const uint32_t sign     = rnd.getUint32() & 0x1u;
            const uint32_t value    = (sign << 31) | (exp << 23) | mantissa;

            DE_ASSERT(tcu::Float32(value).isInf() == isInf && tcu::Float32(value).isNaN() == isNan);

            ((uint32_t *)values[0])[valNdx] = value;
        }
    }

    bool compare(const void *const *inputs, const void *const *outputs)
    {
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        if (precision == glu::PRECISION_HIGHP)
        {
            // Only highp is required to support inf/nan
            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0     = ((const float *)inputs[0])[compNdx];
                const uint32_t out0 = ((const uint32_t *)outputs[0])[compNdx];
                const uint32_t ref  = tcu::Float32(in0).isNaN() ? 1u : 0u;

                if (out0 != ref)
                {
                    m_failMsg << "Expected [" << compNdx << "] = " << HexBool(ref);
                    return false;
                }
            }
        }
        else
        {
            // Value can be either 0 or 1
            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const int out0 = ((const int *)outputs[0])[compNdx];

                if (out0 != 0 && out0 != 1)
                {
                    m_failMsg << "Expected [" << compNdx << "] = 0 / 1";
                    return false;
                }
            }
        }

        return true;
    }
};

class IsinfCase : public CommonFunctionCase
{
public:
    IsinfCase(Context &context, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
        : CommonFunctionCase(context, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "isinf",
                             shaderType)
    {
        DE_ASSERT(glu::isDataTypeFloatOrVec(baseType));

        const int vecSize            = glu::getDataTypeScalarSize(baseType);
        const glu::DataType boolType = vecSize > 1 ? glu::getDataTypeBoolVec(vecSize) : glu::TYPE_BOOL;

        m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(boolType, glu::PRECISION_LAST)));
        m_spec.source = "out0 = isinf(in0);";
    }

    void getInputValues(int numValues, void *const *values) const
    {
        de::Random rnd(deStringHash(getName()) ^ 0xc2a39fu);
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);
        const int mantissaBits         = getMinMantissaBits(precision);
        const uint32_t mantissaMask    = ~getMaxUlpDiffFromBits(mantissaBits) & ((1u << 23) - 1u);

        for (int valNdx = 0; valNdx < numValues * scalarSize; valNdx++)
        {
            const bool isInf        = rnd.getFloat() > 0.3f;
            const bool isNan        = !isInf && rnd.getFloat() > 0.4f;
            const uint32_t mantissa = !isInf ? ((1u << 22) | (rnd.getUint32() & mantissaMask)) : 0;
            const uint32_t exp      = !isNan && !isInf ? (rnd.getUint32() & 0x7fu) : 0xffu;
            const uint32_t sign     = rnd.getUint32() & 0x1u;
            const uint32_t value    = (sign << 31) | (exp << 23) | mantissa;

            DE_ASSERT(tcu::Float32(value).isInf() == isInf && tcu::Float32(value).isNaN() == isNan);

            ((uint32_t *)values[0])[valNdx] = value;
        }
    }

    bool compare(const void *const *inputs, const void *const *outputs)
    {
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        if (precision == glu::PRECISION_HIGHP)
        {
            // Only highp is required to support inf/nan
            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0     = ((const float *)inputs[0])[compNdx];
                const uint32_t out0 = ((const uint32_t *)outputs[0])[compNdx];
                const uint32_t ref  = tcu::Float32(in0).isInf() ? 1u : 0u;

                if (out0 != ref)
                {
                    m_failMsg << "Expected [" << compNdx << "] = " << HexBool(ref);
                    return false;
                }
            }
        }
        else
        {
            // Value can be either 0 or 1
            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const int out0 = ((const int *)outputs[0])[compNdx];

                if (out0 != 0 && out0 != 1)
                {
                    m_failMsg << "Expected [" << compNdx << "] = 0 / 1";
                    return false;
                }
            }
        }

        return true;
    }
};

class FloatBitsToUintIntCase : public CommonFunctionCase
{
public:
    FloatBitsToUintIntCase(Context &context, glu::DataType baseType, glu::Precision precision,
                           glu::ShaderType shaderType, bool outIsSigned)
        : CommonFunctionCase(context, getCommonFuncCaseName(baseType, precision, shaderType).c_str(),
                             outIsSigned ? "floatBitsToInt" : "floatBitsToUint", shaderType)
    {
        const int vecSize           = glu::getDataTypeScalarSize(baseType);
        const glu::DataType intType = outIsSigned ? (vecSize > 1 ? glu::getDataTypeIntVec(vecSize) : glu::TYPE_INT) :
                                                    (vecSize > 1 ? glu::getDataTypeUintVec(vecSize) : glu::TYPE_UINT);

        m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(intType, glu::PRECISION_HIGHP)));
        m_spec.source = outIsSigned ? "out0 = floatBitsToInt(in0);" : "out0 = floatBitsToUint(in0);";
    }

    void getInputValues(int numValues, void *const *values) const
    {
        const Vec2 ranges[] = {
            Vec2(-2.0f, 2.0f), // lowp
            Vec2(-1e3f, 1e3f), // mediump
            Vec2(-1e7f, 1e7f)  // highp
        };

        de::Random rnd(deStringHash(getName()) ^ 0x2790au);
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        fillRandomScalars(rnd, ranges[precision].x(), ranges[precision].y(), values[0], numValues * scalarSize);
    }

    bool compare(const void *const *inputs, const void *const *outputs)
    {
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        const int mantissaBits = getMinMantissaBits(precision);
        const int maxUlpDiff   = getMaxUlpDiffFromBits(mantissaBits);

        for (int compNdx = 0; compNdx < scalarSize; compNdx++)
        {
            const float in0        = ((const float *)inputs[0])[compNdx];
            const uint32_t out0    = ((const uint32_t *)outputs[0])[compNdx];
            const uint32_t refOut0 = tcu::Float32(in0).bits();
            const int ulpDiff      = de::abs((int)out0 - (int)refOut0);

            if (ulpDiff > maxUlpDiff)
            {
                m_failMsg << "Expected [" << compNdx << "] = " << tcu::toHex(refOut0) << " with threshold "
                          << tcu::toHex(maxUlpDiff) << ", got diff " << tcu::toHex(ulpDiff);
                return false;
            }
        }

        return true;
    }
};

class FloatBitsToIntCase : public FloatBitsToUintIntCase
{
public:
    FloatBitsToIntCase(Context &context, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
        : FloatBitsToUintIntCase(context, baseType, precision, shaderType, true)
    {
    }
};

class FloatBitsToUintCase : public FloatBitsToUintIntCase
{
public:
    FloatBitsToUintCase(Context &context, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
        : FloatBitsToUintIntCase(context, baseType, precision, shaderType, false)
    {
    }
};

class BitsToFloatCase : public CommonFunctionCase
{
public:
    BitsToFloatCase(Context &context, glu::DataType baseType, glu::ShaderType shaderType)
        : CommonFunctionCase(context, getCommonFuncCaseName(baseType, glu::PRECISION_HIGHP, shaderType).c_str(),
                             glu::isDataTypeIntOrIVec(baseType) ? "intBitsToFloat" : "uintBitsToFloat", shaderType)
    {
        const bool inIsSigned         = glu::isDataTypeIntOrIVec(baseType);
        const int vecSize             = glu::getDataTypeScalarSize(baseType);
        const glu::DataType floatType = vecSize > 1 ? glu::getDataTypeFloatVec(vecSize) : glu::TYPE_FLOAT;

        m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, glu::PRECISION_HIGHP)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(floatType, glu::PRECISION_HIGHP)));
        m_spec.source = inIsSigned ? "out0 = intBitsToFloat(in0);" : "out0 = uintBitsToFloat(in0);";
    }

    void getInputValues(int numValues, void *const *values) const
    {
        de::Random rnd(deStringHash(getName()) ^ 0xbbb225u);
        const glu::DataType type = m_spec.inputs[0].varType.getBasicType();
        const int scalarSize     = glu::getDataTypeScalarSize(type);
        const Vec2 range(-1e8f, +1e8f);

        // \note Filled as floats.
        fillRandomScalars(rnd, range.x(), range.y(), values[0], numValues * scalarSize);
    }

    bool compare(const void *const *inputs, const void *const *outputs)
    {
        const glu::DataType type  = m_spec.inputs[0].varType.getBasicType();
        const int scalarSize      = glu::getDataTypeScalarSize(type);
        const uint32_t maxUlpDiff = 0;

        for (int compNdx = 0; compNdx < scalarSize; compNdx++)
        {
            const float in0        = ((const float *)inputs[0])[compNdx];
            const float out0       = ((const float *)outputs[0])[compNdx];
            const uint32_t ulpDiff = getUlpDiff(in0, out0);

            if (ulpDiff > maxUlpDiff)
            {
                m_failMsg << "Expected [" << compNdx << "] = " << tcu::toHex(tcu::Float32(in0).bits())
                          << " with ULP threshold " << tcu::toHex(maxUlpDiff) << ", got ULP diff "
                          << tcu::toHex(ulpDiff);
                return false;
            }
        }

        return true;
    }
};

class FloorCase : public CommonFunctionCase
{
public:
    FloorCase(Context &context, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
        : CommonFunctionCase(context, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "floor",
                             shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(baseType, precision)));
        m_spec.source = "out0 = floor(in0);";
    }

    void getInputValues(int numValues, void *const *values) const
    {
        const Vec2 ranges[] = {
            Vec2(-2.0f, 2.0f), // lowp
            Vec2(-1e3f, 1e3f), // mediump
            Vec2(-1e7f, 1e7f)  // highp
        };

        de::Random rnd(deStringHash(getName()) ^ 0xac23fu);
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);
        // Random cases.
        fillRandomScalars(rnd, ranges[precision].x(), ranges[precision].y(), (float *)values[0],
                          numValues * scalarSize);

        // If precision is mediump, make sure values can be represented in fp16 exactly
        if (precision == glu::PRECISION_MEDIUMP)
        {
            for (int ndx = 0; ndx < numValues * scalarSize; ndx++)
                ((float *)values[0])[ndx] = tcu::Float16(((float *)values[0])[ndx]).asFloat();
        }
    }

    bool compare(const void *const *inputs, const void *const *outputs)
    {
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        if (precision == glu::PRECISION_HIGHP || precision == glu::PRECISION_MEDIUMP)
        {
            // Require exact result.
            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0  = ((const float *)inputs[0])[compNdx];
                const float out0 = ((const float *)outputs[0])[compNdx];
                const float ref  = deFloatFloor(in0);

                const uint32_t ulpDiff = getUlpDiff(out0, ref);

                if (ulpDiff > 0)
                {
                    m_failMsg << "Expected [" << compNdx << "] = " << HexFloat(ref) << ", got ULP diff "
                              << tcu::toHex(ulpDiff);
                    return false;
                }
            }
        }
        else
        {
            const int mantissaBits    = getMinMantissaBits(precision);
            const uint32_t maxUlpDiff = getMaxUlpDiffFromBits(mantissaBits); // ULP diff for rounded integer value.
            const float eps           = getEpsFromBits(1.0f, mantissaBits);  // epsilon for rounding bounds

            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0  = ((const float *)inputs[0])[compNdx];
                const float out0 = ((const float *)outputs[0])[compNdx];
                const int minRes = int(deFloatFloor(in0 - eps));
                const int maxRes = int(deFloatFloor(in0 + eps));
                bool anyOk       = false;

                for (int roundedVal = minRes; roundedVal <= maxRes; roundedVal++)
                {
                    const uint32_t ulpDiff = getUlpDiff(out0, float(roundedVal));

                    if (ulpDiff <= maxUlpDiff)
                    {
                        anyOk = true;
                        break;
                    }
                }

                if (!anyOk)
                {
                    m_failMsg << "Expected [" << compNdx << "] = [" << minRes << ", " << maxRes
                              << "] with ULP threshold " << tcu::toHex(maxUlpDiff);
                    return false;
                }
            }
        }

        return true;
    }
};

class TruncCase : public CommonFunctionCase
{
public:
    TruncCase(Context &context, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
        : CommonFunctionCase(context, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "trunc",
                             shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(baseType, precision)));
        m_spec.source = "out0 = trunc(in0);";
    }

    void getInputValues(int numValues, void *const *values) const
    {
        const Vec2 ranges[] = {
            Vec2(-2.0f, 2.0f), // lowp
            Vec2(-1e3f, 1e3f), // mediump
            Vec2(-1e7f, 1e7f)  // highp
        };

        de::Random rnd(deStringHash(getName()) ^ 0xac23fu);
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);
        const float specialCases[]     = {0.0f, -0.0f, -0.9f, 0.9f, 1.0f, -1.0f};
        const int numSpecialCases      = DE_LENGTH_OF_ARRAY(specialCases);

        // Special cases
        for (int caseNdx = 0; caseNdx < numSpecialCases; caseNdx++)
        {
            for (int scalarNdx = 0; scalarNdx < scalarSize; scalarNdx++)
                ((float *)values[0])[caseNdx * scalarSize + scalarNdx] = specialCases[caseNdx];
        }

        // Random cases.
        fillRandomScalars(rnd, ranges[precision].x(), ranges[precision].y(),
                          (float *)values[0] + scalarSize * numSpecialCases,
                          (numValues - numSpecialCases) * scalarSize);

        // If precision is mediump, make sure values can be represented in fp16 exactly
        if (precision == glu::PRECISION_MEDIUMP)
        {
            for (int ndx = 0; ndx < numValues * scalarSize; ndx++)
                ((float *)values[0])[ndx] = tcu::Float16(((float *)values[0])[ndx]).asFloat();
        }
    }

    bool compare(const void *const *inputs, const void *const *outputs)
    {
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        if (precision == glu::PRECISION_HIGHP || precision == glu::PRECISION_MEDIUMP)
        {
            // Require exact result.
            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0  = ((const float *)inputs[0])[compNdx];
                const float out0 = ((const float *)outputs[0])[compNdx];
                const bool isNeg = tcu::Float32(in0).sign() < 0;
                const float ref  = isNeg ? (-float(int(-in0))) : float(int(in0));

                // \note: trunc() function definition is a bit broad on negative zeros. Ignore result sign if zero.
                const uint32_t ulpDiff = getUlpDiffIgnoreZeroSign(out0, ref);

                if (ulpDiff > 0)
                {
                    m_failMsg << "Expected [" << compNdx << "] = " << HexFloat(ref) << ", got ULP diff "
                              << tcu::toHex(ulpDiff);
                    return false;
                }
            }
        }
        else
        {
            const int mantissaBits    = getMinMantissaBits(precision);
            const uint32_t maxUlpDiff = getMaxUlpDiffFromBits(mantissaBits); // ULP diff for rounded integer value.
            const float eps           = getEpsFromBits(1.0f, mantissaBits);  // epsilon for rounding bounds

            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0  = ((const float *)inputs[0])[compNdx];
                const float out0 = ((const float *)outputs[0])[compNdx];
                const int minRes = int(in0 - eps);
                const int maxRes = int(in0 + eps);
                bool anyOk       = false;

                for (int roundedVal = minRes; roundedVal <= maxRes; roundedVal++)
                {
                    const uint32_t ulpDiff = getUlpDiffIgnoreZeroSign(out0, float(roundedVal));

                    if (ulpDiff <= maxUlpDiff)
                    {
                        anyOk = true;
                        break;
                    }
                }

                if (!anyOk)
                {
                    m_failMsg << "Expected [" << compNdx << "] = [" << minRes << ", " << maxRes
                              << "] with ULP threshold " << tcu::toHex(maxUlpDiff);
                    return false;
                }
            }
        }

        return true;
    }
};

class RoundCase : public CommonFunctionCase
{
public:
    RoundCase(Context &context, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
        : CommonFunctionCase(context, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "round",
                             shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(baseType, precision)));
        m_spec.source = "out0 = round(in0);";
    }

    void getInputValues(int numValues, void *const *values) const
    {
        const Vec2 ranges[] = {
            Vec2(-2.0f, 2.0f), // lowp
            Vec2(-1e3f, 1e3f), // mediump
            Vec2(-1e7f, 1e7f)  // highp
        };

        de::Random rnd(deStringHash(getName()) ^ 0xac23fu);
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);
        int numSpecialCases            = 0;

        // Special cases.
        if (precision != glu::PRECISION_LOWP)
        {
            DE_ASSERT(numValues >= 10);
            for (int ndx = 0; ndx < 10; ndx++)
            {
                const float v = de::clamp(float(ndx) - 5.5f, ranges[precision].x(), ranges[precision].y());
                std::fill((float *)values[0], (float *)values[0] + scalarSize, v);
                numSpecialCases += 1;
            }
        }

        // Random cases.
        fillRandomScalars(rnd, ranges[precision].x(), ranges[precision].y(),
                          (float *)values[0] + numSpecialCases * scalarSize,
                          (numValues - numSpecialCases) * scalarSize);

        // If precision is mediump, make sure values can be represented in fp16 exactly
        if (precision == glu::PRECISION_MEDIUMP)
        {
            for (int ndx = 0; ndx < numValues * scalarSize; ndx++)
                ((float *)values[0])[ndx] = tcu::Float16(((float *)values[0])[ndx]).asFloat();
        }
    }

    bool compare(const void *const *inputs, const void *const *outputs)
    {
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const bool hasZeroSign         = supportsSignedZero(precision);
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        if (precision == glu::PRECISION_HIGHP || precision == glu::PRECISION_MEDIUMP)
        {
            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0  = ((const float *)inputs[0])[compNdx];
                const float out0 = ((const float *)outputs[0])[compNdx];

                if (deFloatFrac(in0) == 0.5f)
                {
                    // Allow both ceil(in) and floor(in)
                    const float ref0 = deFloatFloor(in0);
                    const float ref1 = deFloatCeil(in0);
                    const uint32_t ulpDiff0 =
                        hasZeroSign ? getUlpDiff(out0, ref0) : getUlpDiffIgnoreZeroSign(out0, ref0);
                    const uint32_t ulpDiff1 =
                        hasZeroSign ? getUlpDiff(out0, ref1) : getUlpDiffIgnoreZeroSign(out0, ref1);

                    if (ulpDiff0 > 0 && ulpDiff1 > 0)
                    {
                        m_failMsg << "Expected [" << compNdx << "] = " << HexFloat(ref0) << " or " << HexFloat(ref1)
                                  << ", got ULP diff " << tcu::toHex(de::min(ulpDiff0, ulpDiff1));
                        return false;
                    }
                }
                else
                {
                    // Require exact result
                    const float ref        = roundEven(in0);
                    const uint32_t ulpDiff = hasZeroSign ? getUlpDiff(out0, ref) : getUlpDiffIgnoreZeroSign(out0, ref);

                    if (ulpDiff > 0)
                    {
                        m_failMsg << "Expected [" << compNdx << "] = " << HexFloat(ref) << ", got ULP diff "
                                  << tcu::toHex(ulpDiff);
                        return false;
                    }
                }
            }
        }
        else
        {
            const int mantissaBits    = getMinMantissaBits(precision);
            const uint32_t maxUlpDiff = getMaxUlpDiffFromBits(mantissaBits); // ULP diff for rounded integer value.
            const float eps           = getEpsFromBits(1.0f, mantissaBits);  // epsilon for rounding bounds

            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0  = ((const float *)inputs[0])[compNdx];
                const float out0 = ((const float *)outputs[0])[compNdx];
                const int minRes = int(roundEven(in0 - eps));
                const int maxRes = int(roundEven(in0 + eps));
                bool anyOk       = false;

                for (int roundedVal = minRes; roundedVal <= maxRes; roundedVal++)
                {
                    const uint32_t ulpDiff = getUlpDiffIgnoreZeroSign(out0, float(roundedVal));

                    if (ulpDiff <= maxUlpDiff)
                    {
                        anyOk = true;
                        break;
                    }
                }

                if (!anyOk)
                {
                    m_failMsg << "Expected [" << compNdx << "] = [" << minRes << ", " << maxRes
                              << "] with ULP threshold " << tcu::toHex(maxUlpDiff);
                    return false;
                }
            }
        }

        return true;
    }
};

class CeilCase : public CommonFunctionCase
{
public:
    CeilCase(Context &context, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
        : CommonFunctionCase(context, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "ceil",
                             shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(baseType, precision)));
        m_spec.source = "out0 = ceil(in0);";
    }

    void getInputValues(int numValues, void *const *values) const
    {
        const Vec2 ranges[] = {
            Vec2(-2.0f, 2.0f), // lowp
            Vec2(-1e3f, 1e3f), // mediump
            Vec2(-1e7f, 1e7f)  // highp
        };

        de::Random rnd(deStringHash(getName()) ^ 0xac23fu);
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        // Random cases.
        fillRandomScalars(rnd, ranges[precision].x(), ranges[precision].y(), (float *)values[0],
                          numValues * scalarSize);

        // If precision is mediump, make sure values can be represented in fp16 exactly
        if (precision == glu::PRECISION_MEDIUMP)
        {
            for (int ndx = 0; ndx < numValues * scalarSize; ndx++)
                ((float *)values[0])[ndx] = tcu::Float16(((float *)values[0])[ndx]).asFloat();
        }
    }

    bool compare(const void *const *inputs, const void *const *outputs)
    {
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const bool hasZeroSign         = supportsSignedZero(precision);
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        if (precision == glu::PRECISION_HIGHP || precision == glu::PRECISION_MEDIUMP)
        {
            // Require exact result.
            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0  = ((const float *)inputs[0])[compNdx];
                const float out0 = ((const float *)outputs[0])[compNdx];
                const float ref  = deFloatCeil(in0);

                const uint32_t ulpDiff = hasZeroSign ? getUlpDiff(out0, ref) : getUlpDiffIgnoreZeroSign(out0, ref);

                if (ulpDiff > 0)
                {
                    m_failMsg << "Expected [" << compNdx << "] = " << HexFloat(ref) << ", got ULP diff "
                              << tcu::toHex(ulpDiff);
                    return false;
                }
            }
        }
        else
        {
            const int mantissaBits    = getMinMantissaBits(precision);
            const uint32_t maxUlpDiff = getMaxUlpDiffFromBits(mantissaBits); // ULP diff for rounded integer value.
            const float eps           = getEpsFromBits(1.0f, mantissaBits);  // epsilon for rounding bounds

            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0  = ((const float *)inputs[0])[compNdx];
                const float out0 = ((const float *)outputs[0])[compNdx];
                const int minRes = int(deFloatCeil(in0 - eps));
                const int maxRes = int(deFloatCeil(in0 + eps));
                bool anyOk       = false;

                for (int roundedVal = minRes; roundedVal <= maxRes; roundedVal++)
                {
                    const uint32_t ulpDiff = getUlpDiffIgnoreZeroSign(out0, float(roundedVal));

                    if (ulpDiff <= maxUlpDiff)
                    {
                        anyOk = true;
                        break;
                    }
                }

                if (!anyOk && de::inRange(0, minRes, maxRes))
                {
                    // Allow -0 as well.
                    const int ulpDiff = de::abs((int)tcu::Float32(out0).bits() - (int)0x80000000u);
                    anyOk             = ((uint32_t)ulpDiff <= maxUlpDiff);
                }

                if (!anyOk)
                {
                    m_failMsg << "Expected [" << compNdx << "] = [" << minRes << ", " << maxRes
                              << "] with ULP threshold " << tcu::toHex(maxUlpDiff);
                    return false;
                }
            }
        }

        return true;
    }
};

class FractCase : public CommonFunctionCase
{
public:
    FractCase(Context &context, glu::DataType baseType, glu::Precision precision, glu::ShaderType shaderType)
        : CommonFunctionCase(context, getCommonFuncCaseName(baseType, precision, shaderType).c_str(), "fract",
                             shaderType)
    {
        m_spec.inputs.push_back(Symbol("in0", glu::VarType(baseType, precision)));
        m_spec.outputs.push_back(Symbol("out0", glu::VarType(baseType, precision)));
        m_spec.source = "out0 = fract(in0);";
    }

    void getInputValues(int numValues, void *const *values) const
    {
        const Vec2 ranges[] = {
            Vec2(-2.0f, 2.0f), // lowp
            Vec2(-1e3f, 1e3f), // mediump
            Vec2(-1e7f, 1e7f)  // highp
        };

        de::Random rnd(deStringHash(getName()) ^ 0xac23fu);
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const int scalarSize           = glu::getDataTypeScalarSize(type);
        int numSpecialCases            = 0;

        // Special cases.
        if (precision != glu::PRECISION_LOWP)
        {
            DE_ASSERT(numValues >= 10);
            for (int ndx = 0; ndx < 10; ndx++)
            {
                const float v = de::clamp(float(ndx) - 5.5f, ranges[precision].x(), ranges[precision].y());
                std::fill((float *)values[0], (float *)values[0] + scalarSize, v);
                numSpecialCases += 1;
            }
        }

        // Random cases.
        fillRandomScalars(rnd, ranges[precision].x(), ranges[precision].y(),
                          (float *)values[0] + numSpecialCases * scalarSize,
                          (numValues - numSpecialCases) * scalarSize);

        // If precision is mediump, make sure values can be represented in fp16 exactly
        if (precision == glu::PRECISION_MEDIUMP)
        {
            for (int ndx = 0; ndx < numValues * scalarSize; ndx++)
                ((float *)values[0])[ndx] = tcu::Float16(((float *)values[0])[ndx]).asFloat();
        }
    }

    bool compare(const void *const *inputs, const void *const *outputs)
    {
        const glu::DataType type       = m_spec.inputs[0].varType.getBasicType();
        const glu::Precision precision = m_spec.inputs[0].varType.getPrecision();
        const bool hasZeroSign         = supportsSignedZero(precision);
        const int scalarSize           = glu::getDataTypeScalarSize(type);

        if (precision == glu::PRECISION_HIGHP || precision == glu::PRECISION_MEDIUMP)
        {
            // Require exact result.
            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0  = ((const float *)inputs[0])[compNdx];
                const float out0 = ((const float *)outputs[0])[compNdx];
                const float ref  = deFloatFrac(in0);

                const uint32_t ulpDiff = hasZeroSign ? getUlpDiff(out0, ref) : getUlpDiffIgnoreZeroSign(out0, ref);

                if (ulpDiff > 0)
                {
                    m_failMsg << "Expected [" << compNdx << "] = " << HexFloat(ref) << ", got ULP diff "
                              << tcu::toHex(ulpDiff);
                    return false;
                }
            }
        }
        else
        {
            const int mantissaBits = getMinMantissaBits(precision);
            const float eps        = getEpsFromBits(1.0f, mantissaBits); // epsilon for rounding bounds

            for (int compNdx = 0; compNdx < scalarSize; compNdx++)
            {
                const float in0  = ((const float *)inputs[0])[compNdx];
                const float out0 = ((const float *)outputs[0])[compNdx];

                if (int(deFloatFloor(in0 - eps)) == int(deFloatFloor(in0 + eps)))
                {
                    const float ref           = deFloatFrac(in0);
                    const int bitsLost        = numBitsLostInOp(in0, ref);
                    const uint32_t maxUlpDiff = getMaxUlpDiffFromBits(
                        de::max(0, mantissaBits - bitsLost)); // ULP diff for rounded integer value.
                    const uint32_t ulpDiff = getUlpDiffIgnoreZeroSign(out0, ref);

                    if (ulpDiff > maxUlpDiff)
                    {
                        m_failMsg << "Expected [" << compNdx << "] = " << HexFloat(ref) << " with ULP threshold "
                                  << tcu::toHex(maxUlpDiff) << ", got diff " << tcu::toHex(ulpDiff);
                        return false;
                    }
                }
                else
                {
                    if (out0 >= 1.0f)
                    {
                        m_failMsg << "Expected [" << compNdx << "] < 1.0";
                        return false;
                    }
                }
            }
        }

        return true;
    }
};

ShaderCommonFunctionTests::ShaderCommonFunctionTests(Context &context)
    : TestCaseGroup(context, "common", "Common function tests")
{
}

ShaderCommonFunctionTests::~ShaderCommonFunctionTests(void)
{
}

template <class TestClass>
static void addFunctionCases(TestCaseGroup *parent, const char *functionName, bool floatTypes, bool intTypes,
                             bool uintTypes)
{
    tcu::TestCaseGroup *group = new tcu::TestCaseGroup(parent->getTestContext(), functionName, functionName);
    parent->addChild(group);

    const glu::DataType scalarTypes[] = {glu::TYPE_FLOAT, glu::TYPE_INT, glu::TYPE_UINT};

    for (int scalarTypeNdx = 0; scalarTypeNdx < DE_LENGTH_OF_ARRAY(scalarTypes); scalarTypeNdx++)
    {
        const glu::DataType scalarType = scalarTypes[scalarTypeNdx];

        if ((!floatTypes && scalarType == glu::TYPE_FLOAT) || (!intTypes && scalarType == glu::TYPE_INT) ||
            (!uintTypes && scalarType == glu::TYPE_UINT))
            continue;

        for (int vecSize = 1; vecSize <= 4; vecSize++)
        {
            for (int prec = glu::PRECISION_LOWP; prec <= glu::PRECISION_HIGHP; prec++)
            {
                for (int shaderType = glu::SHADERTYPE_VERTEX; shaderType <= glu::SHADERTYPE_FRAGMENT; shaderType++)
                    group->addChild(new TestClass(parent->getContext(), glu::DataType(scalarType + vecSize - 1),
                                                  glu::Precision(prec), glu::ShaderType(shaderType)));
            }
        }
    }
}

void ShaderCommonFunctionTests::init(void)
{
    //                                                                    Float?    Int?    Uint?
    addFunctionCases<AbsCase>(this, "abs", true, true, false);
    addFunctionCases<SignCase>(this, "sign", true, true, false);
    addFunctionCases<FloorCase>(this, "floor", true, false, false);
    addFunctionCases<TruncCase>(this, "trunc", true, false, false);
    addFunctionCases<RoundCase>(this, "round", true, false, false);
    addFunctionCases<RoundEvenCase>(this, "roundeven", true, false, false);
    addFunctionCases<CeilCase>(this, "ceil", true, false, false);
    addFunctionCases<FractCase>(this, "fract", true, false, false);
    // mod
    addFunctionCases<ModfCase>(this, "modf", true, false, false);
    // min
    // max
    // clamp
    // mix
    // step
    // smoothstep
    addFunctionCases<IsnanCase>(this, "isnan", true, false, false);
    addFunctionCases<IsinfCase>(this, "isinf", true, false, false);
    addFunctionCases<FloatBitsToIntCase>(this, "floatbitstoint", true, false, false);
    addFunctionCases<FloatBitsToUintCase>(this, "floatbitstouint", true, false, false);

    // (u)intBitsToFloat()
    {
        tcu::TestCaseGroup *intGroup  = new tcu::TestCaseGroup(m_testCtx, "intbitstofloat", "intBitsToFloat() Tests");
        tcu::TestCaseGroup *uintGroup = new tcu::TestCaseGroup(m_testCtx, "uintbitstofloat", "uintBitsToFloat() Tests");

        addChild(intGroup);
        addChild(uintGroup);

        for (int vecSize = 1; vecSize < 4; vecSize++)
        {
            const glu::DataType intType  = vecSize > 1 ? glu::getDataTypeIntVec(vecSize) : glu::TYPE_INT;
            const glu::DataType uintType = vecSize > 1 ? glu::getDataTypeUintVec(vecSize) : glu::TYPE_UINT;

            for (int shaderType = glu::SHADERTYPE_VERTEX; shaderType <= glu::SHADERTYPE_FRAGMENT; shaderType++)
            {
                intGroup->addChild(new BitsToFloatCase(m_context, intType, glu::ShaderType(shaderType)));
                uintGroup->addChild(new BitsToFloatCase(m_context, uintType, glu::ShaderType(shaderType)));
            }
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
