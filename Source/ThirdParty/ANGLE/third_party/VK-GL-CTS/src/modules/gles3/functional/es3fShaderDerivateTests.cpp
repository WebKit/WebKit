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
 * \brief Shader derivate function tests.
 *
 * \todo [2013-06-25 pyry] Missing features:
 *  - lines and points
 *  - projected coordinates
 *  - continous non-trivial functions (sin, exp)
 *  - non-continous functions (step)
 *//*--------------------------------------------------------------------*/

#include "es3fShaderDerivateTests.hpp"
#include "gluShaderProgram.hpp"
#include "gluRenderContext.hpp"
#include "gluDrawUtil.hpp"
#include "gluPixelTransfer.hpp"
#include "gluShaderUtil.hpp"
#include "gluStrUtil.hpp"
#include "gluTextureUtil.hpp"
#include "gluTexture.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuSurface.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuRGBA.hpp"
#include "tcuFloat.hpp"
#include "tcuInterval.hpp"
#include "deRandom.hpp"
#include "deUniquePtr.hpp"
#include "deString.h"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "glsShaderRenderCase.hpp" // gls::setupDefaultUniforms()

#include <sstream>

namespace deqp
{
namespace gles3
{
namespace Functional
{

using std::map;
using std::ostringstream;
using std::string;
using std::vector;
using tcu::TestLog;

enum
{
    VIEWPORT_WIDTH      = 167,
    VIEWPORT_HEIGHT     = 103,
    FBO_WIDTH           = 99,
    FBO_HEIGHT          = 133,
    MAX_FAILED_MESSAGES = 10
};

enum DerivateFunc
{
    DERIVATE_DFDX = 0,
    DERIVATE_DFDY,
    DERIVATE_FWIDTH,

    DERIVATE_LAST
};

enum SurfaceType
{
    SURFACETYPE_DEFAULT_FRAMEBUFFER = 0,
    SURFACETYPE_UNORM_FBO,
    SURFACETYPE_FLOAT_FBO, // \note Uses RGBA32UI fbo actually, since FP rendertargets are not in core spec.

    SURFACETYPE_LAST
};

// Utilities

namespace
{

class AutoFbo
{
public:
    AutoFbo(const glw::Functions &gl) : m_gl(gl), m_fbo(0)
    {
    }

    ~AutoFbo(void)
    {
        if (m_fbo)
            m_gl.deleteFramebuffers(1, &m_fbo);
    }

    void gen(void)
    {
        DE_ASSERT(!m_fbo);
        m_gl.genFramebuffers(1, &m_fbo);
    }

    uint32_t operator*(void) const
    {
        return m_fbo;
    }

private:
    const glw::Functions &m_gl;
    uint32_t m_fbo;
};

class AutoRbo
{
public:
    AutoRbo(const glw::Functions &gl) : m_gl(gl), m_rbo(0)
    {
    }

    ~AutoRbo(void)
    {
        if (m_rbo)
            m_gl.deleteRenderbuffers(1, &m_rbo);
    }

    void gen(void)
    {
        DE_ASSERT(!m_rbo);
        m_gl.genRenderbuffers(1, &m_rbo);
    }

    uint32_t operator*(void) const
    {
        return m_rbo;
    }

private:
    const glw::Functions &m_gl;
    uint32_t m_rbo;
};

} // namespace

static const char *getDerivateFuncName(DerivateFunc func)
{
    switch (func)
    {
    case DERIVATE_DFDX:
        return "dFdx";
    case DERIVATE_DFDY:
        return "dFdy";
    case DERIVATE_FWIDTH:
        return "fwidth";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

static const char *getDerivateFuncCaseName(DerivateFunc func)
{
    switch (func)
    {
    case DERIVATE_DFDX:
        return "dfdx";
    case DERIVATE_DFDY:
        return "dfdy";
    case DERIVATE_FWIDTH:
        return "fwidth";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

static inline tcu::BVec4 getDerivateMask(glu::DataType type)
{
    switch (type)
    {
    case glu::TYPE_FLOAT:
        return tcu::BVec4(true, false, false, false);
    case glu::TYPE_FLOAT_VEC2:
        return tcu::BVec4(true, true, false, false);
    case glu::TYPE_FLOAT_VEC3:
        return tcu::BVec4(true, true, true, false);
    case glu::TYPE_FLOAT_VEC4:
        return tcu::BVec4(true, true, true, true);
    default:
        DE_ASSERT(false);
        return tcu::BVec4(true);
    }
}

static inline tcu::Vec4 readDerivate(const tcu::ConstPixelBufferAccess &surface, const tcu::Vec4 &derivScale,
                                     const tcu::Vec4 &derivBias, int x, int y)
{
    return (surface.getPixel(x, y) - derivBias) / derivScale;
}

static inline tcu::UVec4 getCompExpBits(const tcu::Vec4 &v)
{
    return tcu::UVec4(tcu::Float32(v[0]).exponentBits(), tcu::Float32(v[1]).exponentBits(),
                      tcu::Float32(v[2]).exponentBits(), tcu::Float32(v[3]).exponentBits());
}

float computeFloatingPointError(const float value, const int numAccurateBits)
{
    const int numGarbageBits = 23 - numAccurateBits;
    const uint32_t mask      = (1u << numGarbageBits) - 1u;
    const int exp            = (tcu::Float32(value).exponent() < -3) ? -3 : tcu::Float32(value).exponent();

    return tcu::Float32::construct(+1, exp, (1u << 23) | mask).asFloat() -
           tcu::Float32::construct(+1, exp, 1u << 23).asFloat();
}

static int getNumMantissaBits(const glu::Precision precision)
{
    switch (precision)
    {
    case glu::PRECISION_HIGHP:
        return 23;
    case glu::PRECISION_MEDIUMP:
        return 10;
    case glu::PRECISION_LOWP:
        return 6;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

static int getMinExponent(const glu::Precision precision)
{
    switch (precision)
    {
    case glu::PRECISION_HIGHP:
        return -126;
    case glu::PRECISION_MEDIUMP:
        return -14;
    case glu::PRECISION_LOWP:
        return -8;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

static float getSingleULPForExponent(int exp, int numMantissaBits)
{
    if (numMantissaBits > 0)
    {
        DE_ASSERT(numMantissaBits <= 23);

        const int ulpBitNdx = 23 - numMantissaBits;
        return tcu::Float32::construct(+1, exp, (1 << 23) | (1 << ulpBitNdx)).asFloat() -
               tcu::Float32::construct(+1, exp, (1 << 23)).asFloat();
    }
    else
    {
        DE_ASSERT(numMantissaBits == 0);
        return tcu::Float32::construct(+1, exp, (1 << 23)).asFloat();
    }
}

static float getSingleULPForValue(float value, int numMantissaBits)
{
    const int exp = tcu::Float32(value).exponent();
    return getSingleULPForExponent(exp, numMantissaBits);
}

static float convertFloatFlushToZeroRtn(float value, int minExponent, int numAccurateBits)
{
    if (value == 0.0f)
    {
        return 0.0f;
    }
    else
    {
        const tcu::Float32 inputFloat = tcu::Float32(value);
        const int numTruncatedBits    = 23 - numAccurateBits;
        const uint32_t truncMask      = (1u << numTruncatedBits) - 1u;

        if (value > 0.0f)
        {
            if (value > 0.0f && tcu::Float32(value).exponent() < minExponent)
            {
                // flush to zero if possible
                return 0.0f;
            }
            else
            {
                // just mask away non-representable bits
                return tcu::Float32::construct(+1, inputFloat.exponent(), inputFloat.mantissa() & ~truncMask).asFloat();
            }
        }
        else
        {
            if (inputFloat.mantissa() & truncMask)
            {
                // decrement one ulp if truncated bits are non-zero (i.e. if value is not representable)
                return tcu::Float32::construct(-1, inputFloat.exponent(), inputFloat.mantissa() & ~truncMask)
                           .asFloat() -
                       getSingleULPForExponent(inputFloat.exponent(), numAccurateBits);
            }
            else
            {
                // value is representable, no need to do anything
                return value;
            }
        }
    }
}

static float convertFloatFlushToZeroRtp(float value, int minExponent, int numAccurateBits)
{
    return -convertFloatFlushToZeroRtn(-value, minExponent, numAccurateBits);
}

static float addErrorUlp(float value, float numUlps, int numMantissaBits)
{
    return value + numUlps * getSingleULPForValue(value, numMantissaBits);
}

enum
{
    INTERPOLATION_LOST_BITS = 3, // number mantissa of bits allowed to be lost in varying interpolation
};

static int getInterpolationLostBitsWarning(const glu::Precision precision)
{
    // number mantissa of bits allowed to be lost in varying interpolation
    switch (precision)
    {
    case glu::PRECISION_HIGHP:
        return 9;
    case glu::PRECISION_MEDIUMP:
        return 3;
    case glu::PRECISION_LOWP:
        return 3;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

static inline tcu::Vec4 getDerivateThreshold(const glu::Precision precision, const tcu::Vec4 &valueMin,
                                             const tcu::Vec4 &valueMax, const tcu::Vec4 &expectedDerivate)
{
    const int baseBits           = getNumMantissaBits(precision);
    const tcu::UVec4 derivExp    = getCompExpBits(expectedDerivate);
    const tcu::UVec4 maxValueExp = max(getCompExpBits(valueMin), getCompExpBits(valueMax));
    const tcu::UVec4 numBitsLost = maxValueExp - min(maxValueExp, derivExp);
    const tcu::IVec4 numAccurateBits =
        max(baseBits - numBitsLost.asInt() - (int)INTERPOLATION_LOST_BITS, tcu::IVec4(0));

    return tcu::Vec4(computeFloatingPointError(expectedDerivate[0], numAccurateBits[0]),
                     computeFloatingPointError(expectedDerivate[1], numAccurateBits[1]),
                     computeFloatingPointError(expectedDerivate[2], numAccurateBits[2]),
                     computeFloatingPointError(expectedDerivate[3], numAccurateBits[3]));
}

static inline tcu::Vec4 getDerivateThresholdWarning(const glu::Precision precision, const tcu::Vec4 &valueMin,
                                                    const tcu::Vec4 &valueMax, const tcu::Vec4 &expectedDerivate)
{
    const int baseBits           = getNumMantissaBits(precision);
    const tcu::UVec4 derivExp    = getCompExpBits(expectedDerivate);
    const tcu::UVec4 maxValueExp = max(getCompExpBits(valueMin), getCompExpBits(valueMax));
    const tcu::UVec4 numBitsLost = maxValueExp - min(maxValueExp, derivExp);
    const tcu::IVec4 numAccurateBits =
        max(baseBits - numBitsLost.asInt() - getInterpolationLostBitsWarning(precision), tcu::IVec4(0));

    return tcu::Vec4(computeFloatingPointError(expectedDerivate[0], numAccurateBits[0]),
                     computeFloatingPointError(expectedDerivate[1], numAccurateBits[1]),
                     computeFloatingPointError(expectedDerivate[2], numAccurateBits[2]),
                     computeFloatingPointError(expectedDerivate[3], numAccurateBits[3]));
}

namespace
{

struct LogVecComps
{
    const tcu::Vec4 &v;
    int numComps;

    LogVecComps(const tcu::Vec4 &v_, int numComps_) : v(v_), numComps(numComps_)
    {
    }
};

std::ostream &operator<<(std::ostream &str, const LogVecComps &v)
{
    DE_ASSERT(de::inRange(v.numComps, 1, 4));
    if (v.numComps == 1)
        return str << v.v[0];
    else if (v.numComps == 2)
        return str << v.v.toWidth<2>();
    else if (v.numComps == 3)
        return str << v.v.toWidth<3>();
    else
        return str << v.v;
}

} // namespace

enum VerificationLogging
{
    LOG_ALL = 0,
    LOG_NOTHING
};

static qpTestResult verifyConstantDerivate(tcu::TestLog &log, const tcu::ConstPixelBufferAccess &result,
                                           const tcu::PixelBufferAccess &errorMask, glu::DataType dataType,
                                           const tcu::Vec4 &reference, const tcu::Vec4 &threshold,
                                           const tcu::Vec4 &scale, const tcu::Vec4 &bias,
                                           VerificationLogging logPolicy = LOG_ALL)
{
    const int numComps    = glu::getDataTypeFloatScalars(dataType);
    const tcu::BVec4 mask = tcu::logicalNot(getDerivateMask(dataType));
    int numFailedPixels   = 0;

    if (logPolicy == LOG_ALL)
        log << TestLog::Message << "Expecting " << LogVecComps(reference, numComps) << " with threshold "
            << LogVecComps(threshold, numComps) << TestLog::EndMessage;

    for (int y = 0; y < result.getHeight(); y++)
    {
        for (int x = 0; x < result.getWidth(); x++)
        {
            const tcu::Vec4 resDerivate = readDerivate(result, scale, bias, x, y);
            const bool isOk =
                tcu::allEqual(tcu::logicalOr(tcu::lessThanEqual(tcu::abs(reference - resDerivate), threshold), mask),
                              tcu::BVec4(true));

            if (!isOk)
            {
                if (numFailedPixels < MAX_FAILED_MESSAGES && logPolicy == LOG_ALL)
                    log << TestLog::Message << "FAIL: got " << LogVecComps(resDerivate, numComps)
                        << ", diff = " << LogVecComps(tcu::abs(reference - resDerivate), numComps) << ", at x = " << x
                        << ", y = " << y << TestLog::EndMessage;
                numFailedPixels += 1;
                errorMask.setPixel(tcu::RGBA::red().toVec(), x, y);
            }
        }
    }

    if (numFailedPixels >= MAX_FAILED_MESSAGES && logPolicy == LOG_ALL)
        log << TestLog::Message << "..." << TestLog::EndMessage;

    if (numFailedPixels > 0 && logPolicy == LOG_ALL)
        log << TestLog::Message << "FAIL: found " << numFailedPixels << " failed pixels" << TestLog::EndMessage;

    return (numFailedPixels == 0) ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL;
}

struct Linear2DFunctionEvaluator
{
    tcu::Matrix<float, 4, 3> matrix;

    //      .-----.
    //      | s_x |
    //  M x | s_y |
    //      | 1.0 |
    //      '-----'
    tcu::Vec4 evaluateAt(float screenX, float screenY) const;
};

tcu::Vec4 Linear2DFunctionEvaluator::evaluateAt(float screenX, float screenY) const
{
    const tcu::Vec3 position(screenX, screenY, 1.0f);
    return matrix * position;
}

static qpTestResult reverifyConstantDerivateWithFlushRelaxations(
    tcu::TestLog &log, const tcu::ConstPixelBufferAccess &result, const tcu::PixelBufferAccess &errorMask,
    glu::DataType dataType, glu::Precision precision, const tcu::Vec4 &derivScale, const tcu::Vec4 &derivBias,
    const tcu::Vec4 &surfaceThreshold, DerivateFunc derivateFunc, const Linear2DFunctionEvaluator &function)
{
    DE_ASSERT(result.getWidth() == errorMask.getWidth());
    DE_ASSERT(result.getHeight() == errorMask.getHeight());
    DE_ASSERT(derivateFunc == DERIVATE_DFDX || derivateFunc == DERIVATE_DFDY);

    const tcu::IVec4 red(255, 0, 0, 255);
    const tcu::IVec4 green(0, 255, 0, 255);
    const float divisionErrorUlps = 2.5f;

    const int numComponents = glu::getDataTypeFloatScalars(dataType);
    const int numBits       = getNumMantissaBits(precision);
    const int minExponent   = getMinExponent(precision);

    const int numVaryingSampleBits = numBits - INTERPOLATION_LOST_BITS;
    int numFailedPixels            = 0;

    tcu::clear(errorMask, green);

    // search for failed pixels
    for (int y = 0; y < result.getHeight(); ++y)
        for (int x = 0; x < result.getWidth(); ++x)
        {
            //                 flushToZero?(f2z?(functionValueCurrent) - f2z?(functionValueBefore))
            // flushToZero? ( ------------------------------------------------------------------------ +- 2.5 ULP )
            //                                                  dx

            const tcu::Vec4 resultDerivative = readDerivate(result, derivScale, derivBias, x, y);

            // sample at the front of the back pixel and the back of the front pixel to cover the whole area of
            // legal sample positions. In general case this is NOT OK, but we know that the target function is
            // (mostly*) linear which allows us to take the sample points at arbitrary points. This gets us the
            // maximum difference possible in exponents which are used in error bound calculations.
            // * non-linearity may happen around zero or with very high function values due to subnorms not
            //   behaving well.
            const tcu::Vec4 functionValueForward  = (derivateFunc == DERIVATE_DFDX) ?
                                                        (function.evaluateAt((float)x + 2.0f, (float)y + 0.5f)) :
                                                        (function.evaluateAt((float)x + 0.5f, (float)y + 2.0f));
            const tcu::Vec4 functionValueBackward = (derivateFunc == DERIVATE_DFDX) ?
                                                        (function.evaluateAt((float)x - 1.0f, (float)y + 0.5f)) :
                                                        (function.evaluateAt((float)x + 0.5f, (float)y - 1.0f));

            bool anyComponentFailed = false;

            // check components separately
            for (int c = 0; c < numComponents; ++c)
            {
                // Simulate interpolation. Add allowed interpolation error and round to target precision. Allow one half ULP (i.e. correct rounding)
                const tcu::Interval forwardComponent(
                    convertFloatFlushToZeroRtn(addErrorUlp((float)functionValueForward[c], -0.5f, numVaryingSampleBits),
                                               minExponent, numBits),
                    convertFloatFlushToZeroRtp(addErrorUlp((float)functionValueForward[c], +0.5f, numVaryingSampleBits),
                                               minExponent, numBits));
                const tcu::Interval backwardComponent(
                    convertFloatFlushToZeroRtn(
                        addErrorUlp((float)functionValueBackward[c], -0.5f, numVaryingSampleBits), minExponent,
                        numBits),
                    convertFloatFlushToZeroRtp(
                        addErrorUlp((float)functionValueBackward[c], +0.5f, numVaryingSampleBits), minExponent,
                        numBits));
                const int maxValueExp = de::max(de::max(tcu::Float32(forwardComponent.lo()).exponent(),
                                                        tcu::Float32(forwardComponent.hi()).exponent()),
                                                de::max(tcu::Float32(backwardComponent.lo()).exponent(),
                                                        tcu::Float32(backwardComponent.hi()).exponent()));

                // subtraction in numerator will likely cause a cancellation of the most
                // significant bits. Apply error bounds.

                const tcu::Interval numerator(forwardComponent - backwardComponent);
                const int numeratorLoExp      = tcu::Float32(numerator.lo()).exponent();
                const int numeratorHiExp      = tcu::Float32(numerator.hi()).exponent();
                const int numeratorLoBitsLost = de::max(
                    0,
                    maxValueExp -
                        numeratorLoExp); //!< must clamp to zero since if forward and backward components have different
                const int numeratorHiBitsLost = de::max(
                    0, maxValueExp - numeratorHiExp); //!< sign, numerator might have larger exponent than its operands.
                const int numeratorLoBits = de::max(0, numBits - numeratorLoBitsLost);
                const int numeratorHiBits = de::max(0, numBits - numeratorHiBitsLost);

                const tcu::Interval numeratorRange(
                    convertFloatFlushToZeroRtn((float)numerator.lo(), minExponent, numeratorLoBits),
                    convertFloatFlushToZeroRtp((float)numerator.hi(), minExponent, numeratorHiBits));

                const tcu::Interval divisionRange =
                    numeratorRange /
                    3.0f; // legal sample area is anywhere within this and neighboring pixels (i.e. size = 3)
                const tcu::Interval divisionResultRange(
                    convertFloatFlushToZeroRtn(addErrorUlp((float)divisionRange.lo(), -divisionErrorUlps, numBits),
                                               minExponent, numBits),
                    convertFloatFlushToZeroRtp(addErrorUlp((float)divisionRange.hi(), +divisionErrorUlps, numBits),
                                               minExponent, numBits));
                const tcu::Interval finalResultRange(divisionResultRange.lo() - surfaceThreshold[c],
                                                     divisionResultRange.hi() + surfaceThreshold[c]);

                if (resultDerivative[c] >= finalResultRange.lo() && resultDerivative[c] <= finalResultRange.hi())
                {
                    // value ok
                }
                else
                {
                    if (numFailedPixels < MAX_FAILED_MESSAGES)
                        log << tcu::TestLog::Message << "Error in pixel at " << x << ", " << y << " with component "
                            << c << " (channel " << ("rgba"[c]) << ")\n"
                            << "\tGot pixel value " << result.getPixelInt(x, y) << "\n"
                            << "\t\tdFd" << ((derivateFunc == DERIVATE_DFDX) ? ('x') : ('y'))
                            << " ~= " << resultDerivative[c] << "\n"
                            << "\t\tdifference to a valid range: "
                            << ((resultDerivative[c] < finalResultRange.lo()) ? ("-") : ("+"))
                            << ((resultDerivative[c] < finalResultRange.lo()) ?
                                    (finalResultRange.lo() - resultDerivative[c]) :
                                    (resultDerivative[c] - finalResultRange.hi()))
                            << "\n"
                            << "\tDerivative value range:\n"
                            << "\t\tMin: " << finalResultRange.lo() << "\n"
                            << "\t\tMax: " << finalResultRange.hi() << "\n"
                            << tcu::TestLog::EndMessage;

                    ++numFailedPixels;
                    anyComponentFailed = true;
                }
            }

            if (anyComponentFailed)
                errorMask.setPixel(red, x, y);
        }

    if (numFailedPixels >= MAX_FAILED_MESSAGES)
        log << TestLog::Message << "..." << TestLog::EndMessage;

    if (numFailedPixels > 0)
        log << TestLog::Message << "FAIL: found " << numFailedPixels << " failed pixels" << TestLog::EndMessage;

    return (numFailedPixels == 0) ? QP_TEST_RESULT_PASS : QP_TEST_RESULT_FAIL;
}

// TriangleDerivateCase

class TriangleDerivateCase : public TestCase
{
public:
    TriangleDerivateCase(Context &context, const char *name, const char *description);
    ~TriangleDerivateCase(void);

    IterateResult iterate(void);

protected:
    virtual void setupRenderState(uint32_t program)
    {
        DE_UNREF(program);
    }
    virtual qpTestResult verify(const tcu::ConstPixelBufferAccess &result, const tcu::PixelBufferAccess &errorMask) = 0;

    tcu::IVec2 getViewportSize(void) const;
    tcu::Vec4 getSurfaceThreshold(void) const;

    glu::DataType m_dataType;
    glu::Precision m_precision;

    glu::DataType m_coordDataType;
    glu::Precision m_coordPrecision;

    std::string m_fragmentSrc;

    tcu::Vec4 m_coordMin;
    tcu::Vec4 m_coordMax;
    tcu::Vec4 m_derivScale;
    tcu::Vec4 m_derivBias;

    SurfaceType m_surfaceType;
    int m_numSamples;
    uint32_t m_hint;

    bool m_useAsymmetricCoords;
};

TriangleDerivateCase::TriangleDerivateCase(Context &context, const char *name, const char *description)
    : TestCase(context, name, description)
    , m_dataType(glu::TYPE_LAST)
    , m_precision(glu::PRECISION_LAST)
    , m_coordDataType(glu::TYPE_LAST)
    , m_coordPrecision(glu::PRECISION_LAST)
    , m_surfaceType(SURFACETYPE_DEFAULT_FRAMEBUFFER)
    , m_numSamples(0)
    , m_hint(GL_DONT_CARE)
    , m_useAsymmetricCoords(false)
{
    DE_ASSERT(m_surfaceType != SURFACETYPE_DEFAULT_FRAMEBUFFER || m_numSamples == 0);
}

TriangleDerivateCase::~TriangleDerivateCase(void)
{
    TriangleDerivateCase::deinit();
}

static std::string genVertexSource(glu::DataType coordType, glu::Precision precision)
{
    DE_ASSERT(glu::isDataTypeFloatOrVec(coordType));

    const char *vertexTmpl = "#version 300 es\n"
                             "in highp vec4 a_position;\n"
                             "in ${PRECISION} ${DATATYPE} a_coord;\n"
                             "out ${PRECISION} ${DATATYPE} v_coord;\n"
                             "void main (void)\n"
                             "{\n"
                             "    gl_Position = a_position;\n"
                             "    v_coord = a_coord;\n"
                             "}\n";

    map<string, string> vertexParams;

    vertexParams["PRECISION"] = glu::getPrecisionName(precision);
    vertexParams["DATATYPE"]  = glu::getDataTypeName(coordType);

    return tcu::StringTemplate(vertexTmpl).specialize(vertexParams);
}

inline tcu::IVec2 TriangleDerivateCase::getViewportSize(void) const
{
    if (m_surfaceType == SURFACETYPE_DEFAULT_FRAMEBUFFER)
    {
        const int width  = de::min<int>(m_context.getRenderTarget().getWidth(), VIEWPORT_WIDTH);
        const int height = de::min<int>(m_context.getRenderTarget().getHeight(), VIEWPORT_HEIGHT);
        return tcu::IVec2(width, height);
    }
    else
        return tcu::IVec2(FBO_WIDTH, FBO_HEIGHT);
}

TriangleDerivateCase::IterateResult TriangleDerivateCase::iterate(void)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const glu::ShaderProgram program(
        m_context.getRenderContext(),
        glu::makeVtxFragSources(genVertexSource(m_coordDataType, m_coordPrecision), m_fragmentSrc));
    de::Random rnd(deStringHash(getName()) ^ 0xbbc24);
    const bool useFbo             = m_surfaceType != SURFACETYPE_DEFAULT_FRAMEBUFFER;
    const uint32_t fboFormat      = m_surfaceType == SURFACETYPE_FLOAT_FBO ? GL_RGBA32UI : GL_RGBA8;
    const tcu::IVec2 viewportSize = getViewportSize();
    const int viewportX = useFbo ? 0 : rnd.getInt(0, m_context.getRenderTarget().getWidth() - viewportSize.x());
    const int viewportY = useFbo ? 0 : rnd.getInt(0, m_context.getRenderTarget().getHeight() - viewportSize.y());
    AutoFbo fbo(gl);
    AutoRbo rbo(gl);
    tcu::TextureLevel result;

    m_testCtx.getLog() << program;

    if (!program.isOk())
        TCU_FAIL("Compile failed");

    if (useFbo)
    {
        m_testCtx.getLog() << TestLog::Message << "Rendering to FBO, format = " << glu::getTextureFormatStr(fboFormat)
                           << ", samples = " << m_numSamples << TestLog::EndMessage;

        fbo.gen();
        rbo.gen();

        gl.bindRenderbuffer(GL_RENDERBUFFER, *rbo);
        gl.renderbufferStorageMultisample(GL_RENDERBUFFER, m_numSamples, fboFormat, viewportSize.x(), viewportSize.y());
        gl.bindFramebuffer(GL_FRAMEBUFFER, *fbo);
        gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *rbo);
        TCU_CHECK(gl.checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }
    else
    {
        const tcu::PixelFormat pixelFormat = m_context.getRenderTarget().getPixelFormat();

        m_testCtx.getLog() << TestLog::Message << "Rendering to default framebuffer\n"
                           << "\tColor depth: R=" << pixelFormat.redBits << ", G=" << pixelFormat.greenBits
                           << ", B=" << pixelFormat.blueBits << ", A=" << pixelFormat.alphaBits << TestLog::EndMessage;
    }

    m_testCtx.getLog() << TestLog::Message << "in: " << m_coordMin << " -> " << m_coordMax << "\n"
                       << (m_useAsymmetricCoords ? "v_coord.x = in.x * (x+y)/2\n" : "v_coord.x = in.x * x\n")
                       << (m_useAsymmetricCoords ? "v_coord.y = in.y * (x+y)/2\n" : "v_coord.y = in.y * y\n")
                       << "v_coord.z = in.z * (x+y)/2\n"
                       << "v_coord.w = in.w * (1 - (x+y)/2)\n"
                       << TestLog::EndMessage << TestLog::Message << "u_scale: " << m_derivScale
                       << ", u_bias: " << m_derivBias << " (displayed values have scale/bias removed)"
                       << TestLog::EndMessage << TestLog::Message << "Viewport: " << viewportSize.x() << "x"
                       << viewportSize.y() << TestLog::EndMessage << TestLog::Message
                       << "GL_FRAGMENT_SHADER_DERIVATE_HINT: " << glu::getHintModeStr(m_hint) << TestLog::EndMessage;

    // Draw
    {
        const float positions[] = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f,
                                   1.0f,  -1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 0.0f, 1.0f};
        float coords[]          = {m_coordMin.x(),
                                   m_coordMin.y(),
                                   m_coordMin.z(),
                                   m_coordMax.w(),
                                   m_coordMin.x(),
                                   m_coordMax.y(),
                                   (m_coordMin.z() + m_coordMax.z()) * 0.5f,
                                   (m_coordMin.w() + m_coordMax.w()) * 0.5f,
                                   m_coordMax.x(),
                                   m_coordMin.y(),
                                   (m_coordMin.z() + m_coordMax.z()) * 0.5f,
                                   (m_coordMin.w() + m_coordMax.w()) * 0.5f,
                                   m_coordMax.x(),
                                   m_coordMax.y(),
                                   m_coordMax.z(),
                                   m_coordMin.w()};

        // For linear tests we want varying data x and y to vary along both axes
        // to get nonzero x for dfdy and nonzero y for dfdx. To make the gradient
        // the same for both triangles we set vertices 2 and 3 to middle values.
        // This way the values go from min -> (max+min) / 2 or (max+min) / 2 -> max
        // depending on the triangle, but the derivative is the same for both.
        if (m_useAsymmetricCoords)
        {
            coords[4] = coords[8] = (m_coordMin.x() + m_coordMax.x()) * 0.5f;
            coords[5] = coords[9] = (m_coordMin.y() + m_coordMax.y()) * 0.5f;
        }

        const glu::VertexArrayBinding vertexArrays[] = {glu::va::Float("a_position", 4, 4, 0, &positions[0]),
                                                        glu::va::Float("a_coord", 4, 4, 0, &coords[0])};
        const uint16_t indices[]                     = {0, 2, 1, 2, 3, 1};

        gl.clearColor(0.125f, 0.25f, 0.5f, 1.0f);
        gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        gl.disable(GL_DITHER);

        gl.useProgram(program.getProgram());

        {
            const int scaleLoc = gl.getUniformLocation(program.getProgram(), "u_scale");
            const int biasLoc  = gl.getUniformLocation(program.getProgram(), "u_bias");

            switch (m_dataType)
            {
            case glu::TYPE_FLOAT:
                gl.uniform1f(scaleLoc, m_derivScale.x());
                gl.uniform1f(biasLoc, m_derivBias.x());
                break;

            case glu::TYPE_FLOAT_VEC2:
                gl.uniform2fv(scaleLoc, 1, m_derivScale.getPtr());
                gl.uniform2fv(biasLoc, 1, m_derivBias.getPtr());
                break;

            case glu::TYPE_FLOAT_VEC3:
                gl.uniform3fv(scaleLoc, 1, m_derivScale.getPtr());
                gl.uniform3fv(biasLoc, 1, m_derivBias.getPtr());
                break;

            case glu::TYPE_FLOAT_VEC4:
                gl.uniform4fv(scaleLoc, 1, m_derivScale.getPtr());
                gl.uniform4fv(biasLoc, 1, m_derivBias.getPtr());
                break;

            default:
                DE_ASSERT(false);
            }
        }

        gls::setupDefaultUniforms(m_context.getRenderContext(), program.getProgram());
        setupRenderState(program.getProgram());

        gl.hint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, m_hint);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Setup program state");

        gl.viewport(viewportX, viewportY, viewportSize.x(), viewportSize.y());
        glu::draw(m_context.getRenderContext(), program.getProgram(), DE_LENGTH_OF_ARRAY(vertexArrays),
                  &vertexArrays[0], glu::pr::Triangles(DE_LENGTH_OF_ARRAY(indices), &indices[0]));
        GLU_EXPECT_NO_ERROR(gl.getError(), "Draw");
    }

    // Read back results
    {
        const bool isMSAA = useFbo && m_numSamples > 0;
        AutoFbo resFbo(gl);
        AutoRbo resRbo(gl);

        // Resolve if necessary
        if (isMSAA)
        {
            resFbo.gen();
            resRbo.gen();

            gl.bindRenderbuffer(GL_RENDERBUFFER, *resRbo);
            gl.renderbufferStorageMultisample(GL_RENDERBUFFER, 0, fboFormat, viewportSize.x(), viewportSize.y());
            gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, *resFbo);
            gl.framebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *resRbo);
            TCU_CHECK(gl.checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

            gl.blitFramebuffer(0, 0, viewportSize.x(), viewportSize.y(), 0, 0, viewportSize.x(), viewportSize.y(),
                               GL_COLOR_BUFFER_BIT, GL_NEAREST);
            GLU_EXPECT_NO_ERROR(gl.getError(), "Resolve blit");

            gl.bindFramebuffer(GL_READ_FRAMEBUFFER, *resFbo);
        }

        switch (m_surfaceType)
        {
        case SURFACETYPE_DEFAULT_FRAMEBUFFER:
        case SURFACETYPE_UNORM_FBO:
            result.setStorage(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8),
                              viewportSize.x(), viewportSize.y());
            glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, result);
            break;

        case SURFACETYPE_FLOAT_FBO:
        {
            const tcu::TextureFormat dataFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT);
            const tcu::TextureFormat transferFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNSIGNED_INT32);

            result.setStorage(dataFormat, viewportSize.x(), viewportSize.y());
            glu::readPixels(m_context.getRenderContext(), viewportX, viewportY,
                            tcu::PixelBufferAccess(transferFormat, result.getWidth(), result.getHeight(),
                                                   result.getDepth(), result.getAccess().getDataPtr()));
            break;
        }

        default:
            DE_ASSERT(false);
        }

        GLU_EXPECT_NO_ERROR(gl.getError(), "Read pixels");
    }

    // Verify
    {
        tcu::Surface errorMask(result.getWidth(), result.getHeight());
        tcu::clear(errorMask.getAccess(), tcu::RGBA::green().toVec());

        const qpTestResult testResult = verify(result.getAccess(), errorMask.getAccess());
        const char *failStr           = "Fail";

        m_testCtx.getLog() << TestLog::ImageSet("Result", "Result images")
                           << TestLog::Image("Rendered", "Rendered image", result);

        if (testResult != QP_TEST_RESULT_PASS)
            m_testCtx.getLog() << TestLog::Image("ErrorMask", "Error mask", errorMask);

        m_testCtx.getLog() << TestLog::EndImageSet;

        if (testResult == QP_TEST_RESULT_PASS)
            failStr = "Pass";
        else if (testResult == QP_TEST_RESULT_QUALITY_WARNING)
            failStr = "QualityWarning";

        m_testCtx.setTestResult(testResult, failStr);
    }

    return STOP;
}

tcu::Vec4 TriangleDerivateCase::getSurfaceThreshold(void) const
{
    switch (m_surfaceType)
    {
    case SURFACETYPE_DEFAULT_FRAMEBUFFER:
    {
        const tcu::PixelFormat pixelFormat = m_context.getRenderTarget().getPixelFormat();
        const tcu::IVec4 channelBits(pixelFormat.redBits, pixelFormat.greenBits, pixelFormat.blueBits,
                                     pixelFormat.alphaBits);
        const tcu::IVec4 intThreshold = tcu::IVec4(1) << (8 - channelBits);
        const tcu::Vec4 normThreshold = intThreshold.asFloat() / 255.0f;

        return normThreshold;
    }

    case SURFACETYPE_UNORM_FBO:
        return tcu::IVec4(1).asFloat() / 255.0f;
    case SURFACETYPE_FLOAT_FBO:
        return tcu::Vec4(0.0f);
    default:
        DE_ASSERT(false);
        return tcu::Vec4(0.0f);
    }
}

// ConstantDerivateCase

class ConstantDerivateCase : public TriangleDerivateCase
{
public:
    ConstantDerivateCase(Context &context, const char *name, const char *description, DerivateFunc func,
                         glu::DataType type);
    ~ConstantDerivateCase(void)
    {
    }

    void init(void);

protected:
    qpTestResult verify(const tcu::ConstPixelBufferAccess &result, const tcu::PixelBufferAccess &errorMask);

private:
    DerivateFunc m_func;
};

ConstantDerivateCase::ConstantDerivateCase(Context &context, const char *name, const char *description,
                                           DerivateFunc func, glu::DataType type)
    : TriangleDerivateCase(context, name, description)
    , m_func(func)
{
    m_dataType       = type;
    m_precision      = glu::PRECISION_HIGHP;
    m_coordDataType  = m_dataType;
    m_coordPrecision = m_precision;
}

void ConstantDerivateCase::init(void)
{
    const char *fragmentTmpl = "#version 300 es\n"
                               "layout(location = 0) out mediump vec4 o_color;\n"
                               "uniform ${PRECISION} ${DATATYPE} u_scale;\n"
                               "uniform ${PRECISION} ${DATATYPE} u_bias;\n"
                               "void main (void)\n"
                               "{\n"
                               "    ${PRECISION} ${DATATYPE} res = ${FUNC}(${VALUE}) * u_scale + u_bias;\n"
                               "    o_color = ${CAST_TO_OUTPUT};\n"
                               "}\n";
    map<string, string> fragmentParams;
    fragmentParams["PRECISION"]      = glu::getPrecisionName(m_precision);
    fragmentParams["DATATYPE"]       = glu::getDataTypeName(m_dataType);
    fragmentParams["FUNC"]           = getDerivateFuncName(m_func);
    fragmentParams["VALUE"]          = m_dataType == glu::TYPE_FLOAT_VEC4 ? "vec4(1.0, 7.2, -1e5, 0.0)" :
                                       m_dataType == glu::TYPE_FLOAT_VEC3 ? "vec3(1e2, 8.0, 0.01)" :
                                       m_dataType == glu::TYPE_FLOAT_VEC2 ? "vec2(-0.0, 2.7)" :
                                                                            /* TYPE_FLOAT */ "7.7";
    fragmentParams["CAST_TO_OUTPUT"] = m_dataType == glu::TYPE_FLOAT_VEC4 ? "res" :
                                       m_dataType == glu::TYPE_FLOAT_VEC3 ? "vec4(res, 1.0)" :
                                       m_dataType == glu::TYPE_FLOAT_VEC2 ? "vec4(res, 0.0, 1.0)" :
                                                                            /* TYPE_FLOAT */ "vec4(res, 0.0, 0.0, 1.0)";

    m_fragmentSrc = tcu::StringTemplate(fragmentTmpl).specialize(fragmentParams);

    m_derivScale = tcu::Vec4(1e3f, 1e3f, 1e3f, 1e3f);
    m_derivBias  = tcu::Vec4(0.5f, 0.5f, 0.5f, 0.5f);
}

qpTestResult ConstantDerivateCase::verify(const tcu::ConstPixelBufferAccess &result,
                                          const tcu::PixelBufferAccess &errorMask)
{
    const tcu::Vec4 reference(0.0f); // Derivate of constant argument should always be 0
    const tcu::Vec4 threshold = getSurfaceThreshold() / abs(m_derivScale);

    return verifyConstantDerivate(m_testCtx.getLog(), result, errorMask, m_dataType, reference, threshold, m_derivScale,
                                  m_derivBias);
}

// LinearDerivateCase

class LinearDerivateCase : public TriangleDerivateCase
{
public:
    LinearDerivateCase(Context &context, const char *name, const char *description, DerivateFunc func,
                       glu::DataType type, glu::Precision precision, uint32_t hint, SurfaceType surfaceType,
                       int numSamples, const char *fragmentSrcTmpl);
    ~LinearDerivateCase(void)
    {
    }

    void init(void);

protected:
    qpTestResult verify(const tcu::ConstPixelBufferAccess &result, const tcu::PixelBufferAccess &errorMask);

private:
    DerivateFunc m_func;
    std::string m_fragmentTmpl;
};

LinearDerivateCase::LinearDerivateCase(Context &context, const char *name, const char *description, DerivateFunc func,
                                       glu::DataType type, glu::Precision precision, uint32_t hint,
                                       SurfaceType surfaceType, int numSamples, const char *fragmentSrcTmpl)
    : TriangleDerivateCase(context, name, description)
    , m_func(func)
    , m_fragmentTmpl(fragmentSrcTmpl)
{
    m_dataType            = type;
    m_precision           = precision;
    m_coordDataType       = m_dataType;
    m_coordPrecision      = m_precision;
    m_hint                = hint;
    m_surfaceType         = surfaceType;
    m_numSamples          = numSamples;
    m_useAsymmetricCoords = true;
}

void LinearDerivateCase::init(void)
{
    const tcu::IVec2 viewportSize = getViewportSize();
    const float w                 = float(viewportSize.x());
    const float h                 = float(viewportSize.y());
    const bool packToInt          = m_surfaceType == SURFACETYPE_FLOAT_FBO;
    map<string, string> fragmentParams;

    fragmentParams["OUTPUT_TYPE"] = glu::getDataTypeName(packToInt ? glu::TYPE_UINT_VEC4 : glu::TYPE_FLOAT_VEC4);
    fragmentParams["OUTPUT_PREC"] = glu::getPrecisionName(packToInt ? glu::PRECISION_HIGHP : m_precision);
    fragmentParams["PRECISION"]   = glu::getPrecisionName(m_precision);
    fragmentParams["DATATYPE"]    = glu::getDataTypeName(m_dataType);
    fragmentParams["FUNC"]        = getDerivateFuncName(m_func);

    if (packToInt)
    {
        fragmentParams["CAST_TO_OUTPUT"] =
            m_dataType == glu::TYPE_FLOAT_VEC4 ? "floatBitsToUint(res)" :
            m_dataType == glu::TYPE_FLOAT_VEC3 ? "floatBitsToUint(vec4(res, 1.0))" :
            m_dataType == glu::TYPE_FLOAT_VEC2 ? "floatBitsToUint(vec4(res, 0.0, 1.0))" :
                                                 /* TYPE_FLOAT */ "floatBitsToUint(vec4(res, 0.0, 0.0, 1.0))";
    }
    else
    {
        fragmentParams["CAST_TO_OUTPUT"] =
            m_dataType == glu::TYPE_FLOAT_VEC4 ? "res" :
            m_dataType == glu::TYPE_FLOAT_VEC3 ? "vec4(res, 1.0)" :
            m_dataType == glu::TYPE_FLOAT_VEC2 ? "vec4(res, 0.0, 1.0)" :
                                                 /* TYPE_FLOAT */ "vec4(res, 0.0, 0.0, 1.0)";
    }

    m_fragmentSrc = tcu::StringTemplate(m_fragmentTmpl.c_str()).specialize(fragmentParams);

    switch (m_precision)
    {
    case glu::PRECISION_HIGHP:
        m_coordMin = tcu::Vec4(-97.f, 0.2f, 71.f, 74.f);
        m_coordMax = tcu::Vec4(-13.2f, -77.f, 44.f, 76.f);
        break;

    case glu::PRECISION_MEDIUMP:
        m_coordMin = tcu::Vec4(-37.0f, 47.f, -7.f, 0.0f);
        m_coordMax = tcu::Vec4(-1.0f, 12.f, 7.f, 19.f);
        break;

    case glu::PRECISION_LOWP:
        m_coordMin = tcu::Vec4(0.0f, -1.0f, 0.0f, 1.0f);
        m_coordMax = tcu::Vec4(1.0f, 1.0f, -1.0f, -1.0f);
        break;

    default:
        DE_ASSERT(false);
    }

    if (m_surfaceType == SURFACETYPE_FLOAT_FBO)
    {
        // No scale or bias used for accuracy.
        m_derivScale = tcu::Vec4(1.0f);
        m_derivBias  = tcu::Vec4(0.0f);
    }
    else
    {
        // Compute scale - bias that normalizes to 0..1 range.
        const tcu::Vec4 dx = (m_coordMax - m_coordMin) / tcu::Vec4(w, w, w * 0.5f, -w * 0.5f);
        const tcu::Vec4 dy = (m_coordMax - m_coordMin) / tcu::Vec4(h, h, h * 0.5f, -h * 0.5f);

        switch (m_func)
        {
        case DERIVATE_DFDX:
            m_derivScale = 0.5f / dx;
            break;

        case DERIVATE_DFDY:
            m_derivScale = 0.5f / dy;
            break;

        case DERIVATE_FWIDTH:
            m_derivScale = 0.5f / (tcu::abs(dx) + tcu::abs(dy));
            break;

        default:
            DE_ASSERT(false);
        }

        m_derivBias = tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

qpTestResult LinearDerivateCase::verify(const tcu::ConstPixelBufferAccess &result,
                                        const tcu::PixelBufferAccess &errorMask)
{
    const tcu::Vec4 xScale = tcu::Vec4(0.5f, 0.5f, 0.5f, -0.5f);
    const tcu::Vec4 yScale = tcu::Vec4(0.5f, 0.5f, 0.5f, -0.5f);

    const tcu::Vec4 surfaceThreshold = getSurfaceThreshold() / abs(m_derivScale);

    if (m_func == DERIVATE_DFDX || m_func == DERIVATE_DFDY)
    {
        const bool isX               = m_func == DERIVATE_DFDX;
        const float div              = isX ? float(result.getWidth()) : float(result.getHeight());
        const tcu::Vec4 scale        = isX ? xScale : yScale;
        tcu::Vec4 reference          = ((m_coordMax - m_coordMin) / div);
        const tcu::Vec4 opThreshold  = getDerivateThreshold(m_precision, m_coordMin, m_coordMax, reference);
        const tcu::Vec4 opThresholdW = getDerivateThresholdWarning(m_precision, m_coordMin, m_coordMax, reference);
        const tcu::Vec4 threshold    = max(surfaceThreshold, opThreshold);
        const tcu::Vec4 thresholdW   = max(surfaceThreshold, opThresholdW);
        const int numComps           = glu::getDataTypeFloatScalars(m_dataType);

        /* adjust the reference value for the correct dfdx or dfdy sample adjacency */
        reference = reference * scale;

        m_testCtx.getLog() << tcu::TestLog::Message << "Verifying result image.\n"
                           << "\tValid derivative is " << LogVecComps(reference, numComps) << " with threshold "
                           << LogVecComps(threshold, numComps) << tcu::TestLog::EndMessage;

        // short circuit if result is strictly within the normal value error bounds.
        // This improves performance significantly.
        if (verifyConstantDerivate(m_testCtx.getLog(), result, errorMask, m_dataType, reference, threshold,
                                   m_derivScale, m_derivBias, LOG_NOTHING) == QP_TEST_RESULT_PASS)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "No incorrect derivatives found, result valid."
                               << tcu::TestLog::EndMessage;

            return QP_TEST_RESULT_PASS;
        }

        // Check with relaxed threshold value
        if (verifyConstantDerivate(m_testCtx.getLog(), result, errorMask, m_dataType, reference, thresholdW,
                                   m_derivScale, m_derivBias, LOG_NOTHING) == QP_TEST_RESULT_PASS)
        {
            m_testCtx.getLog() << tcu::TestLog::Message
                               << "No incorrect derivatives found, result valid with quality warning."
                               << tcu::TestLog::EndMessage;

            return QP_TEST_RESULT_QUALITY_WARNING;
        }

        // some pixels exceed error bounds calculated for normal values. Verify that these
        // potentially invalid pixels are in fact valid due to (for example) subnorm flushing.

        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Initial verification failed, verifying image by calculating accurate error bounds for "
                              "each result pixel.\n"
                           << "\tVerifying each result derivative is within its range of legal result values."
                           << tcu::TestLog::EndMessage;

        {
            const tcu::IVec2 viewportSize = getViewportSize();
            const float w                 = float(viewportSize.x());
            const float h                 = float(viewportSize.y());
            const tcu::Vec4 valueRamp     = (m_coordMax - m_coordMin);
            Linear2DFunctionEvaluator function;

            function.matrix.setRow(0,
                                   tcu::Vec3((valueRamp.x() / w) / 2.0f, (valueRamp.x() / h) / 2.0f, m_coordMin.x()));
            function.matrix.setRow(1,
                                   tcu::Vec3((valueRamp.y() / w) / 2.0f, (valueRamp.y() / h) / 2.0f, m_coordMin.y()));
            function.matrix.setRow(2, tcu::Vec3(valueRamp.z() / w, valueRamp.z() / h, m_coordMin.z() + m_coordMin.z()) /
                                          2.0f);
            function.matrix.setRow(
                3, tcu::Vec3(-valueRamp.w() / w, -valueRamp.w() / h, m_coordMax.w() + m_coordMax.w()) / 2.0f);

            return reverifyConstantDerivateWithFlushRelaxations(m_testCtx.getLog(), result, errorMask, m_dataType,
                                                                m_precision, m_derivScale, m_derivBias,
                                                                surfaceThreshold, m_func, function);
        }
    }
    else
    {
        DE_ASSERT(m_func == DERIVATE_FWIDTH);
        const float w = float(result.getWidth());
        const float h = float(result.getHeight());

        const tcu::Vec4 dx          = ((m_coordMax - m_coordMin) / w) * xScale;
        const tcu::Vec4 dy          = ((m_coordMax - m_coordMin) / h) * yScale;
        const tcu::Vec4 reference   = tcu::abs(dx) + tcu::abs(dy);
        const tcu::Vec4 dxThreshold = getDerivateThreshold(m_precision, m_coordMin * xScale, m_coordMax * xScale, dx);
        const tcu::Vec4 dyThreshold = getDerivateThreshold(m_precision, m_coordMin * yScale, m_coordMax * yScale, dy);
        const tcu::Vec4 dxThresholdW =
            getDerivateThresholdWarning(m_precision, m_coordMin * xScale, m_coordMax * xScale, dx);
        const tcu::Vec4 dyThresholdW =
            getDerivateThresholdWarning(m_precision, m_coordMin * yScale, m_coordMax * yScale, dy);
        const tcu::Vec4 threshold  = max(surfaceThreshold, max(dxThreshold, dyThreshold));
        const tcu::Vec4 thresholdW = max(surfaceThreshold, max(dxThresholdW, dyThresholdW));
        qpTestResult testResult    = QP_TEST_RESULT_FAIL;

        testResult = verifyConstantDerivate(m_testCtx.getLog(), result, errorMask, m_dataType, reference, threshold,
                                            m_derivScale, m_derivBias);

        // return if result is pass
        if (testResult == QP_TEST_RESULT_PASS)
            return testResult;

        // re-check with relaxed threshold
        testResult = verifyConstantDerivate(m_testCtx.getLog(), result, errorMask, m_dataType, reference, thresholdW,
                                            m_derivScale, m_derivBias);

        // if with relaxed threshold test is passing then mark the result with quality warning.
        if (testResult == QP_TEST_RESULT_PASS)
            testResult = QP_TEST_RESULT_QUALITY_WARNING;

        return testResult;
    }
}

// TextureDerivateCase

class TextureDerivateCase : public TriangleDerivateCase
{
public:
    TextureDerivateCase(Context &context, const char *name, const char *description, DerivateFunc func,
                        glu::DataType type, glu::Precision precision, uint32_t hint, SurfaceType surfaceType,
                        int numSamples);
    ~TextureDerivateCase(void);

    void init(void);
    void deinit(void);

protected:
    void setupRenderState(uint32_t program);
    qpTestResult verify(const tcu::ConstPixelBufferAccess &result, const tcu::PixelBufferAccess &errorMask);

private:
    DerivateFunc m_func;

    tcu::Vec4 m_texValueMin;
    tcu::Vec4 m_texValueMax;
    glu::Texture2D *m_texture;
};

TextureDerivateCase::TextureDerivateCase(Context &context, const char *name, const char *description, DerivateFunc func,
                                         glu::DataType type, glu::Precision precision, uint32_t hint,
                                         SurfaceType surfaceType, int numSamples)
    : TriangleDerivateCase(context, name, description)
    , m_func(func)
    , m_texture(nullptr)
{
    m_dataType       = type;
    m_precision      = precision;
    m_coordDataType  = glu::TYPE_FLOAT_VEC2;
    m_coordPrecision = glu::PRECISION_HIGHP;
    m_hint           = hint;
    m_surfaceType    = surfaceType;
    m_numSamples     = numSamples;
}

TextureDerivateCase::~TextureDerivateCase(void)
{
    delete m_texture;
}

void TextureDerivateCase::init(void)
{
    // Generate shader
    {
        const char *fragmentTmpl = "#version 300 es\n"
                                   "in highp vec2 v_coord;\n"
                                   "layout(location = 0) out ${OUTPUT_PREC} ${OUTPUT_TYPE} o_color;\n"
                                   "uniform ${PRECISION} sampler2D u_sampler;\n"
                                   "uniform ${PRECISION} ${DATATYPE} u_scale;\n"
                                   "uniform ${PRECISION} ${DATATYPE} u_bias;\n"
                                   "void main (void)\n"
                                   "{\n"
                                   "    ${PRECISION} vec4 tex = texture(u_sampler, v_coord);\n"
                                   "    ${PRECISION} ${DATATYPE} res = ${FUNC}(tex${SWIZZLE}) * u_scale + u_bias;\n"
                                   "    o_color = ${CAST_TO_OUTPUT};\n"
                                   "}\n";

        const bool packToInt = m_surfaceType == SURFACETYPE_FLOAT_FBO;
        map<string, string> fragmentParams;

        fragmentParams["OUTPUT_TYPE"] = glu::getDataTypeName(packToInt ? glu::TYPE_UINT_VEC4 : glu::TYPE_FLOAT_VEC4);
        fragmentParams["OUTPUT_PREC"] = glu::getPrecisionName(packToInt ? glu::PRECISION_HIGHP : m_precision);
        fragmentParams["PRECISION"]   = glu::getPrecisionName(m_precision);
        fragmentParams["DATATYPE"]    = glu::getDataTypeName(m_dataType);
        fragmentParams["FUNC"]        = getDerivateFuncName(m_func);
        fragmentParams["SWIZZLE"]     = m_dataType == glu::TYPE_FLOAT_VEC4 ? "" :
                                        m_dataType == glu::TYPE_FLOAT_VEC3 ? ".xyz" :
                                        m_dataType == glu::TYPE_FLOAT_VEC2 ? ".xy" :
                                                                             /* TYPE_FLOAT */ ".x";

        if (packToInt)
        {
            fragmentParams["CAST_TO_OUTPUT"] =
                m_dataType == glu::TYPE_FLOAT_VEC4 ? "floatBitsToUint(res)" :
                m_dataType == glu::TYPE_FLOAT_VEC3 ? "floatBitsToUint(vec4(res, 1.0))" :
                m_dataType == glu::TYPE_FLOAT_VEC2 ? "floatBitsToUint(vec4(res, 0.0, 1.0))" :
                                                     /* TYPE_FLOAT */ "floatBitsToUint(vec4(res, 0.0, 0.0, 1.0))";
        }
        else
        {
            fragmentParams["CAST_TO_OUTPUT"] =
                m_dataType == glu::TYPE_FLOAT_VEC4 ? "res" :
                m_dataType == glu::TYPE_FLOAT_VEC3 ? "vec4(res, 1.0)" :
                m_dataType == glu::TYPE_FLOAT_VEC2 ? "vec4(res, 0.0, 1.0)" :
                                                     /* TYPE_FLOAT */ "vec4(res, 0.0, 0.0, 1.0)";
        }

        m_fragmentSrc = tcu::StringTemplate(fragmentTmpl).specialize(fragmentParams);
    }

    // Texture size matches viewport and nearest sampling is used. Thus texture sampling
    // is equal to just interpolating the texture value range.

    // Determine value range for texture.

    switch (m_precision)
    {
    case glu::PRECISION_HIGHP:
        m_texValueMin = tcu::Vec4(-97.f, 0.2f, 71.f, 74.f);
        m_texValueMax = tcu::Vec4(-13.2f, -77.f, 44.f, 76.f);
        break;

    case glu::PRECISION_MEDIUMP:
        m_texValueMin = tcu::Vec4(-37.0f, 47.f, -7.f, 0.0f);
        m_texValueMax = tcu::Vec4(-1.0f, 12.f, 7.f, 19.f);
        break;

    case glu::PRECISION_LOWP:
        m_texValueMin = tcu::Vec4(0.0f, -1.0f, 0.0f, 1.0f);
        m_texValueMax = tcu::Vec4(1.0f, 1.0f, -1.0f, -1.0f);
        break;

    default:
        DE_ASSERT(false);
    }

    // Lowp and mediump cases use RGBA16F format, while highp uses RGBA32F.
    {
        const tcu::IVec2 viewportSize = getViewportSize();
        DE_ASSERT(!m_texture);
        m_texture = new glu::Texture2D(m_context.getRenderContext(),
                                       m_precision == glu::PRECISION_HIGHP ? GL_RGBA32F : GL_RGBA16F, viewportSize.x(),
                                       viewportSize.y());
        m_texture->getRefTexture().allocLevel(0);
    }

    // Texture coordinates
    m_coordMin = tcu::Vec4(0.0f);
    m_coordMax = tcu::Vec4(1.0f);

    // Fill with gradients.
    {
        const tcu::PixelBufferAccess level0 = m_texture->getRefTexture().getLevel(0);
        for (int y = 0; y < level0.getHeight(); y++)
        {
            for (int x = 0; x < level0.getWidth(); x++)
            {
                const float xf = (float(x) + 0.5f) / float(level0.getWidth());
                const float yf = (float(y) + 0.5f) / float(level0.getHeight());
                // Make x and y data to have dependency to both axes so that dfdx(tex).y and dfdy(tex).x are nonzero.
                const tcu::Vec4 s =
                    tcu::Vec4(xf + yf / 2.0f, yf + xf / 2.0f, (xf + yf) / 2.0f, 1.0f - (xf + yf) / 2.0f);

                level0.setPixel(m_texValueMin + (m_texValueMax - m_texValueMin) * s, x, y);
            }
        }
    }

    m_texture->upload();

    if (m_surfaceType == SURFACETYPE_FLOAT_FBO)
    {
        // No scale or bias used for accuracy.
        m_derivScale = tcu::Vec4(1.0f);
        m_derivBias  = tcu::Vec4(0.0f);
    }
    else
    {
        // Compute scale - bias that normalizes to 0..1 range.
        const tcu::IVec2 viewportSize = getViewportSize();
        const float w                 = float(viewportSize.x());
        const float h                 = float(viewportSize.y());
        const tcu::Vec4 dx            = (m_texValueMax - m_texValueMin) / tcu::Vec4(w, w, w * 0.5f, -w * 0.5f);
        const tcu::Vec4 dy            = (m_texValueMax - m_texValueMin) / tcu::Vec4(h, h, h * 0.5f, -h * 0.5f);

        switch (m_func)
        {
        case DERIVATE_DFDX:
            m_derivScale = 0.5f / dx;
            break;

        case DERIVATE_DFDY:
            m_derivScale = 0.5f / dy;
            break;

        case DERIVATE_FWIDTH:
            m_derivScale = 0.5f / (tcu::abs(dx) + tcu::abs(dy));
            break;

        default:
            DE_ASSERT(false);
        }

        m_derivBias = tcu::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

void TextureDerivateCase::deinit(void)
{
    delete m_texture;
    m_texture = nullptr;
}

void TextureDerivateCase::setupRenderState(uint32_t program)
{
    const glw::Functions &gl = m_context.getRenderContext().getFunctions();
    const int texUnit        = 1;

    gl.activeTexture(GL_TEXTURE0 + texUnit);
    gl.bindTexture(GL_TEXTURE_2D, m_texture->getGLTexture());
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl.uniform1i(gl.getUniformLocation(program, "u_sampler"), texUnit);
}

qpTestResult TextureDerivateCase::verify(const tcu::ConstPixelBufferAccess &result,
                                         const tcu::PixelBufferAccess &errorMask)
{
    // \note Edges are ignored in comparison
    if (result.getWidth() < 2 || result.getHeight() < 2)
        throw tcu::NotSupportedError("Too small viewport");

    tcu::ConstPixelBufferAccess compareArea =
        tcu::getSubregion(result, 1, 1, result.getWidth() - 2, result.getHeight() - 2);
    tcu::PixelBufferAccess maskArea =
        tcu::getSubregion(errorMask, 1, 1, errorMask.getWidth() - 2, errorMask.getHeight() - 2);
    const tcu::Vec4 xScale = tcu::Vec4(1.0f, 0.5f, 0.5f, -0.5f);
    const tcu::Vec4 yScale = tcu::Vec4(0.5f, 1.0f, 0.5f, -0.5f);
    const float w          = float(result.getWidth());
    const float h          = float(result.getHeight());

    const tcu::Vec4 surfaceThreshold = getSurfaceThreshold() / abs(m_derivScale);

    if (m_func == DERIVATE_DFDX || m_func == DERIVATE_DFDY)
    {
        const bool isX              = m_func == DERIVATE_DFDX;
        const float div             = isX ? w : h;
        const tcu::Vec4 scale       = isX ? xScale : yScale;
        tcu::Vec4 reference         = ((m_texValueMax - m_texValueMin) / div);
        const tcu::Vec4 opThreshold = getDerivateThreshold(m_precision, m_texValueMin, m_texValueMax, reference);
        const tcu::Vec4 opThresholdW =
            getDerivateThresholdWarning(m_precision, m_texValueMin, m_texValueMax, reference);
        const tcu::Vec4 threshold  = max(surfaceThreshold, opThreshold);
        const tcu::Vec4 thresholdW = max(surfaceThreshold, opThresholdW);
        const int numComps         = glu::getDataTypeFloatScalars(m_dataType);

        /* adjust the reference value for the correct dfdx or dfdy sample adjacency */
        reference = reference * scale;

        m_testCtx.getLog() << tcu::TestLog::Message << "Verifying result image.\n"
                           << "\tValid derivative is " << LogVecComps(reference, numComps) << " with threshold "
                           << LogVecComps(threshold, numComps) << tcu::TestLog::EndMessage;

        // short circuit if result is strictly within the normal value error bounds.
        // This improves performance significantly.
        if (verifyConstantDerivate(m_testCtx.getLog(), compareArea, maskArea, m_dataType, reference, threshold,
                                   m_derivScale, m_derivBias, LOG_NOTHING) == QP_TEST_RESULT_PASS)
        {
            m_testCtx.getLog() << tcu::TestLog::Message << "No incorrect derivatives found, result valid."
                               << tcu::TestLog::EndMessage;

            return QP_TEST_RESULT_PASS;
        }

        m_testCtx.getLog() << tcu::TestLog::Message << "Verifying result image.\n"
                           << "\tValid derivative is " << LogVecComps(reference, numComps) << " with Warning threshold "
                           << LogVecComps(thresholdW, numComps) << tcu::TestLog::EndMessage;

        // Re-check with relaxed threshold
        if (verifyConstantDerivate(m_testCtx.getLog(), compareArea, maskArea, m_dataType, reference, thresholdW,
                                   m_derivScale, m_derivBias, LOG_NOTHING) == QP_TEST_RESULT_PASS)
        {
            m_testCtx.getLog() << tcu::TestLog::Message
                               << "No incorrect derivatives found, result valid with quality warning."
                               << tcu::TestLog::EndMessage;

            return QP_TEST_RESULT_QUALITY_WARNING;
        }

        // some pixels exceed error bounds calculated for normal values. Verify that these
        // potentially invalid pixels are in fact valid due to (for example) subnorm flushing.

        m_testCtx.getLog() << tcu::TestLog::Message
                           << "Initial verification failed, verifying image by calculating accurate error bounds for "
                              "each result pixel.\n"
                           << "\tVerifying each result derivative is within its range of legal result values."
                           << tcu::TestLog::EndMessage;

        {
            const tcu::Vec4 valueRamp = (m_texValueMax - m_texValueMin);
            Linear2DFunctionEvaluator function;

            function.matrix.setRow(0, tcu::Vec3(valueRamp.x() / w, (valueRamp.x() / h) / 2.0f, m_texValueMin.x()));
            function.matrix.setRow(1, tcu::Vec3((valueRamp.y() / w) / 2.0f, valueRamp.y() / h, m_texValueMin.y()));
            function.matrix.setRow(
                2, tcu::Vec3(valueRamp.z() / w, valueRamp.z() / h, m_texValueMin.z() + m_texValueMin.z()) / 2.0f);
            function.matrix.setRow(
                3, tcu::Vec3(-valueRamp.w() / w, -valueRamp.w() / h, m_texValueMax.w() + m_texValueMax.w()) / 2.0f);

            return reverifyConstantDerivateWithFlushRelaxations(m_testCtx.getLog(), compareArea, maskArea, m_dataType,
                                                                m_precision, m_derivScale, m_derivBias,
                                                                surfaceThreshold, m_func, function);
        }
    }
    else
    {
        DE_ASSERT(m_func == DERIVATE_FWIDTH);
        const tcu::Vec4 dx        = ((m_texValueMax - m_texValueMin) / w) * xScale;
        const tcu::Vec4 dy        = ((m_texValueMax - m_texValueMin) / h) * yScale;
        const tcu::Vec4 reference = tcu::abs(dx) + tcu::abs(dy);
        const tcu::Vec4 dxThreshold =
            getDerivateThreshold(m_precision, m_texValueMin * xScale, m_texValueMax * xScale, dx);
        const tcu::Vec4 dyThreshold =
            getDerivateThreshold(m_precision, m_texValueMin * yScale, m_texValueMax * yScale, dy);
        const tcu::Vec4 dxThresholdW =
            getDerivateThresholdWarning(m_precision, m_texValueMin * xScale, m_texValueMax * xScale, dx);
        const tcu::Vec4 dyThresholdW =
            getDerivateThresholdWarning(m_precision, m_texValueMin * yScale, m_texValueMax * yScale, dy);
        const tcu::Vec4 threshold  = max(surfaceThreshold, max(dxThreshold, dyThreshold));
        const tcu::Vec4 thresholdW = max(surfaceThreshold, max(dxThresholdW, dyThresholdW));
        qpTestResult testResult    = QP_TEST_RESULT_FAIL;

        testResult = verifyConstantDerivate(m_testCtx.getLog(), compareArea, maskArea, m_dataType, reference, threshold,
                                            m_derivScale, m_derivBias);

        if (testResult == QP_TEST_RESULT_PASS)
            return testResult;

        // Re-Check with relaxed threshold
        testResult = verifyConstantDerivate(m_testCtx.getLog(), compareArea, maskArea, m_dataType, reference,
                                            thresholdW, m_derivScale, m_derivBias);

        // If test is passing with relaxed threshold then mark quality warning
        if (testResult == QP_TEST_RESULT_PASS)
            testResult = QP_TEST_RESULT_QUALITY_WARNING;

        return testResult;
    }
}

ShaderDerivateTests::ShaderDerivateTests(Context &context)
    : TestCaseGroup(context, "derivate", "Derivate Function Tests")
{
}

ShaderDerivateTests::~ShaderDerivateTests(void)
{
}

struct FunctionSpec
{
    std::string name;
    DerivateFunc function;
    glu::DataType dataType;
    glu::Precision precision;

    FunctionSpec(const std::string &name_, DerivateFunc function_, glu::DataType dataType_, glu::Precision precision_)
        : name(name_)
        , function(function_)
        , dataType(dataType_)
        , precision(precision_)
    {
    }
};

void ShaderDerivateTests::init(void)
{
    static const struct
    {
        const char *name;
        const char *description;
        const char *source;
    } s_linearDerivateCases[] = {
        {"linear", "Basic derivate of linearly interpolated argument",

         "#version 300 es\n"
         "in ${PRECISION} ${DATATYPE} v_coord;\n"
         "layout(location = 0) out ${OUTPUT_PREC} ${OUTPUT_TYPE} o_color;\n"
         "uniform ${PRECISION} ${DATATYPE} u_scale;\n"
         "uniform ${PRECISION} ${DATATYPE} u_bias;\n"
         "void main (void)\n"
         "{\n"
         "    ${PRECISION} ${DATATYPE} res = ${FUNC}(v_coord) * u_scale + u_bias;\n"
         "    o_color = ${CAST_TO_OUTPUT};\n"
         "}\n"},
        {"in_function", "Derivate of linear function argument",

         "#version 300 es\n"
         "in ${PRECISION} ${DATATYPE} v_coord;\n"
         "layout(location = 0) out ${OUTPUT_PREC} ${OUTPUT_TYPE} o_color;\n"
         "uniform ${PRECISION} ${DATATYPE} u_scale;\n"
         "uniform ${PRECISION} ${DATATYPE} u_bias;\n"
         "\n"
         "${PRECISION} ${DATATYPE} computeRes (${PRECISION} ${DATATYPE} value)\n"
         "{\n"
         "    return ${FUNC}(v_coord) * u_scale + u_bias;\n"
         "}\n"
         "\n"
         "void main (void)\n"
         "{\n"
         "    ${PRECISION} ${DATATYPE} res = computeRes(v_coord);\n"
         "    o_color = ${CAST_TO_OUTPUT};\n"
         "}\n"},
        {"static_if", "Derivate of linearly interpolated value in static if",

         "#version 300 es\n"
         "in ${PRECISION} ${DATATYPE} v_coord;\n"
         "layout(location = 0) out ${OUTPUT_PREC} ${OUTPUT_TYPE} o_color;\n"
         "uniform ${PRECISION} ${DATATYPE} u_scale;\n"
         "uniform ${PRECISION} ${DATATYPE} u_bias;\n"
         "void main (void)\n"
         "{\n"
         "    ${PRECISION} ${DATATYPE} res;\n"
         "    if (false)\n"
         "        res = ${FUNC}(-v_coord) * u_scale + u_bias;\n"
         "    else\n"
         "        res = ${FUNC}(v_coord) * u_scale + u_bias;\n"
         "    o_color = ${CAST_TO_OUTPUT};\n"
         "}\n"},
        {"static_loop", "Derivate of linearly interpolated value in static loop",

         "#version 300 es\n"
         "in ${PRECISION} ${DATATYPE} v_coord;\n"
         "layout(location = 0) out ${OUTPUT_PREC} ${OUTPUT_TYPE} o_color;\n"
         "uniform ${PRECISION} ${DATATYPE} u_scale;\n"
         "uniform ${PRECISION} ${DATATYPE} u_bias;\n"
         "void main (void)\n"
         "{\n"
         "    ${PRECISION} ${DATATYPE} res = ${DATATYPE}(0.0);\n"
         "    for (int i = 0; i < 2; i++)\n"
         "        res += ${FUNC}(v_coord * float(i));\n"
         "    res = res * u_scale + u_bias;\n"
         "    o_color = ${CAST_TO_OUTPUT};\n"
         "}\n"},
        {"static_switch", "Derivate of linearly interpolated value in static switch",

         "#version 300 es\n"
         "in ${PRECISION} ${DATATYPE} v_coord;\n"
         "layout(location = 0) out ${OUTPUT_PREC} ${OUTPUT_TYPE} o_color;\n"
         "uniform ${PRECISION} ${DATATYPE} u_scale;\n"
         "uniform ${PRECISION} ${DATATYPE} u_bias;\n"
         "void main (void)\n"
         "{\n"
         "    ${PRECISION} ${DATATYPE} res;\n"
         "    switch (1)\n"
         "    {\n"
         "        case 0: res = ${FUNC}(-v_coord) * u_scale + u_bias;    break;\n"
         "        case 1: res = ${FUNC}(v_coord) * u_scale + u_bias;    break;\n"
         "    }\n"
         "    o_color = ${CAST_TO_OUTPUT};\n"
         "}\n"},
        {"uniform_if", "Derivate of linearly interpolated value in uniform if",

         "#version 300 es\n"
         "in ${PRECISION} ${DATATYPE} v_coord;\n"
         "layout(location = 0) out ${OUTPUT_PREC} ${OUTPUT_TYPE} o_color;\n"
         "uniform ${PRECISION} ${DATATYPE} u_scale;\n"
         "uniform ${PRECISION} ${DATATYPE} u_bias;\n"
         "uniform bool ub_true;\n"
         "void main (void)\n"
         "{\n"
         "    ${PRECISION} ${DATATYPE} res;\n"
         "    if (ub_true)"
         "        res = ${FUNC}(v_coord) * u_scale + u_bias;\n"
         "    else\n"
         "        res = ${FUNC}(-v_coord) * u_scale + u_bias;\n"
         "    o_color = ${CAST_TO_OUTPUT};\n"
         "}\n"},
        {"uniform_loop", "Derivate of linearly interpolated value in uniform loop",

         "#version 300 es\n"
         "in ${PRECISION} ${DATATYPE} v_coord;\n"
         "layout(location = 0) out ${OUTPUT_PREC} ${OUTPUT_TYPE} o_color;\n"
         "uniform ${PRECISION} ${DATATYPE} u_scale;\n"
         "uniform ${PRECISION} ${DATATYPE} u_bias;\n"
         "uniform int ui_two;\n"
         "void main (void)\n"
         "{\n"
         "    ${PRECISION} ${DATATYPE} res = ${DATATYPE}(0.0);\n"
         "    for (int i = 0; i < ui_two; i++)\n"
         "        res += ${FUNC}(v_coord * float(i));\n"
         "    res = res * u_scale + u_bias;\n"
         "    o_color = ${CAST_TO_OUTPUT};\n"
         "}\n"},
        {"uniform_switch", "Derivate of linearly interpolated value in uniform switch",

         "#version 300 es\n"
         "in ${PRECISION} ${DATATYPE} v_coord;\n"
         "layout(location = 0) out ${OUTPUT_PREC} ${OUTPUT_TYPE} o_color;\n"
         "uniform ${PRECISION} ${DATATYPE} u_scale;\n"
         "uniform ${PRECISION} ${DATATYPE} u_bias;\n"
         "uniform int ui_one;\n"
         "void main (void)\n"
         "{\n"
         "    ${PRECISION} ${DATATYPE} res;\n"
         "    switch (ui_one)\n"
         "    {\n"
         "        case 0: res = ${FUNC}(-v_coord) * u_scale + u_bias;    break;\n"
         "        case 1: res = ${FUNC}(v_coord) * u_scale + u_bias;    break;\n"
         "    }\n"
         "    o_color = ${CAST_TO_OUTPUT};\n"
         "}\n"},
    };

    static const struct
    {
        const char *name;
        SurfaceType surfaceType;
        int numSamples;
    } s_fboConfigs[] = {
        {"fbo", SURFACETYPE_DEFAULT_FRAMEBUFFER, 0},
        {"fbo_msaa2", SURFACETYPE_UNORM_FBO, 2},
        {"fbo_msaa4", SURFACETYPE_UNORM_FBO, 4},
        {"fbo_float", SURFACETYPE_FLOAT_FBO, 0},
    };

    static const struct
    {
        const char *name;
        uint32_t hint;
    } s_hints[] = {
        {"fastest", GL_FASTEST},
        {"nicest", GL_NICEST},
    };

    static const struct
    {
        const char *name;
        SurfaceType surfaceType;
        int numSamples;
    } s_hintFboConfigs[] = {{"default", SURFACETYPE_DEFAULT_FRAMEBUFFER, 0},
                            {"fbo_msaa4", SURFACETYPE_UNORM_FBO, 4},
                            {"fbo_float", SURFACETYPE_FLOAT_FBO, 0}};

    static const struct
    {
        const char *name;
        SurfaceType surfaceType;
        int numSamples;
        uint32_t hint;
    } s_textureConfigs[] = {
        {"basic", SURFACETYPE_DEFAULT_FRAMEBUFFER, 0, GL_DONT_CARE},
        {"msaa4", SURFACETYPE_UNORM_FBO, 4, GL_DONT_CARE},
        {"float_fastest", SURFACETYPE_FLOAT_FBO, 0, GL_FASTEST},
        {"float_nicest", SURFACETYPE_FLOAT_FBO, 0, GL_NICEST},
    };

    // .dfdx, .dfdy, .fwidth
    for (int funcNdx = 0; funcNdx < DERIVATE_LAST; funcNdx++)
    {
        const DerivateFunc function = DerivateFunc(funcNdx);
        tcu::TestCaseGroup *const functionGroup =
            new tcu::TestCaseGroup(m_testCtx, getDerivateFuncCaseName(function), getDerivateFuncName(function));
        addChild(functionGroup);

        // .constant - no precision variants, checks that derivate of constant arguments is 0
        {
            tcu::TestCaseGroup *const constantGroup =
                new tcu::TestCaseGroup(m_testCtx, "constant", "Derivate of constant argument");
            functionGroup->addChild(constantGroup);

            for (int vecSize = 1; vecSize <= 4; vecSize++)
            {
                const glu::DataType dataType = vecSize > 1 ? glu::getDataTypeFloatVec(vecSize) : glu::TYPE_FLOAT;
                constantGroup->addChild(
                    new ConstantDerivateCase(m_context, glu::getDataTypeName(dataType), "", function, dataType));
            }
        }

        // Cases based on LinearDerivateCase
        for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(s_linearDerivateCases); caseNdx++)
        {
            tcu::TestCaseGroup *const linearCaseGroup = new tcu::TestCaseGroup(
                m_testCtx, s_linearDerivateCases[caseNdx].name, s_linearDerivateCases[caseNdx].description);
            const char *source = s_linearDerivateCases[caseNdx].source;
            functionGroup->addChild(linearCaseGroup);

            for (int vecSize = 1; vecSize <= 4; vecSize++)
            {
                for (int precNdx = 0; precNdx < glu::PRECISION_LAST; precNdx++)
                {
                    const glu::DataType dataType   = vecSize > 1 ? glu::getDataTypeFloatVec(vecSize) : glu::TYPE_FLOAT;
                    const glu::Precision precision = glu::Precision(precNdx);
                    const SurfaceType surfaceType  = SURFACETYPE_DEFAULT_FRAMEBUFFER;
                    const int numSamples           = 0;
                    const uint32_t hint            = GL_DONT_CARE;
                    ostringstream caseName;

                    if (caseNdx != 0 && precision == glu::PRECISION_LOWP)
                        continue; // Skip as lowp doesn't actually produce any bits when rendered to default FB.

                    caseName << glu::getDataTypeName(dataType) << "_" << glu::getPrecisionName(precision);

                    linearCaseGroup->addChild(new LinearDerivateCase(m_context, caseName.str().c_str(), "", function,
                                                                     dataType, precision, hint, surfaceType, numSamples,
                                                                     source));
                }
            }
        }

        // Fbo cases
        for (int caseNdx = 0; caseNdx < DE_LENGTH_OF_ARRAY(s_fboConfigs); caseNdx++)
        {
            tcu::TestCaseGroup *const fboGroup =
                new tcu::TestCaseGroup(m_testCtx, s_fboConfigs[caseNdx].name, "Derivate usage when rendering into FBO");
            const char *source            = s_linearDerivateCases[0].source; // use source from .linear group
            const SurfaceType surfaceType = s_fboConfigs[caseNdx].surfaceType;
            const int numSamples          = s_fboConfigs[caseNdx].numSamples;
            functionGroup->addChild(fboGroup);

            for (int vecSize = 1; vecSize <= 4; vecSize++)
            {
                for (int precNdx = 0; precNdx < glu::PRECISION_LAST; precNdx++)
                {
                    const glu::DataType dataType   = vecSize > 1 ? glu::getDataTypeFloatVec(vecSize) : glu::TYPE_FLOAT;
                    const glu::Precision precision = glu::Precision(precNdx);
                    const uint32_t hint            = GL_DONT_CARE;
                    ostringstream caseName;

                    if (surfaceType != SURFACETYPE_FLOAT_FBO && precision == glu::PRECISION_LOWP)
                        continue; // Skip as lowp doesn't actually produce any bits when rendered to U8 RT.

                    caseName << glu::getDataTypeName(dataType) << "_" << glu::getPrecisionName(precision);

                    fboGroup->addChild(new LinearDerivateCase(m_context, caseName.str().c_str(), "", function, dataType,
                                                              precision, hint, surfaceType, numSamples, source));
                }
            }
        }

        // .fastest, .nicest
        for (int hintCaseNdx = 0; hintCaseNdx < DE_LENGTH_OF_ARRAY(s_hints); hintCaseNdx++)
        {
            tcu::TestCaseGroup *const hintGroup =
                new tcu::TestCaseGroup(m_testCtx, s_hints[hintCaseNdx].name, "Shader derivate hints");
            const char *source  = s_linearDerivateCases[0].source; // use source from .linear group
            const uint32_t hint = s_hints[hintCaseNdx].hint;
            functionGroup->addChild(hintGroup);

            for (int fboCaseNdx = 0; fboCaseNdx < DE_LENGTH_OF_ARRAY(s_hintFboConfigs); fboCaseNdx++)
            {
                tcu::TestCaseGroup *const fboGroup =
                    new tcu::TestCaseGroup(m_testCtx, s_hintFboConfigs[fboCaseNdx].name, "");
                const SurfaceType surfaceType = s_hintFboConfigs[fboCaseNdx].surfaceType;
                const int numSamples          = s_hintFboConfigs[fboCaseNdx].numSamples;
                hintGroup->addChild(fboGroup);

                for (int vecSize = 1; vecSize <= 4; vecSize++)
                {
                    for (int precNdx = 0; precNdx < glu::PRECISION_LAST; precNdx++)
                    {
                        const glu::DataType dataType =
                            vecSize > 1 ? glu::getDataTypeFloatVec(vecSize) : glu::TYPE_FLOAT;
                        const glu::Precision precision = glu::Precision(precNdx);
                        ostringstream caseName;

                        if (surfaceType != SURFACETYPE_FLOAT_FBO && precision == glu::PRECISION_LOWP)
                            continue; // Skip as lowp doesn't actually produce any bits when rendered to U8 RT.

                        caseName << glu::getDataTypeName(dataType) << "_" << glu::getPrecisionName(precision);

                        fboGroup->addChild(new LinearDerivateCase(m_context, caseName.str().c_str(), "", function,
                                                                  dataType, precision, hint, surfaceType, numSamples,
                                                                  source));
                    }
                }
            }
        }

        // .texture
        {
            tcu::TestCaseGroup *const textureGroup =
                new tcu::TestCaseGroup(m_testCtx, "texture", "Derivate of texture lookup result");
            functionGroup->addChild(textureGroup);

            for (int texCaseNdx = 0; texCaseNdx < DE_LENGTH_OF_ARRAY(s_textureConfigs); texCaseNdx++)
            {
                tcu::TestCaseGroup *const caseGroup =
                    new tcu::TestCaseGroup(m_testCtx, s_textureConfigs[texCaseNdx].name, "");
                const SurfaceType surfaceType = s_textureConfigs[texCaseNdx].surfaceType;
                const int numSamples          = s_textureConfigs[texCaseNdx].numSamples;
                const uint32_t hint           = s_textureConfigs[texCaseNdx].hint;
                textureGroup->addChild(caseGroup);

                for (int vecSize = 1; vecSize <= 4; vecSize++)
                {
                    for (int precNdx = 0; precNdx < glu::PRECISION_LAST; precNdx++)
                    {
                        const glu::DataType dataType =
                            vecSize > 1 ? glu::getDataTypeFloatVec(vecSize) : glu::TYPE_FLOAT;
                        const glu::Precision precision = glu::Precision(precNdx);
                        ostringstream caseName;

                        if (surfaceType != SURFACETYPE_FLOAT_FBO && precision == glu::PRECISION_LOWP)
                            continue; // Skip as lowp doesn't actually produce any bits when rendered to U8 RT.

                        caseName << glu::getDataTypeName(dataType) << "_" << glu::getPrecisionName(precision);

                        caseGroup->addChild(new TextureDerivateCase(m_context, caseName.str().c_str(), "", function,
                                                                    dataType, precision, hint, surfaceType,
                                                                    numSamples));
                    }
                }
            }
        }
    }
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
