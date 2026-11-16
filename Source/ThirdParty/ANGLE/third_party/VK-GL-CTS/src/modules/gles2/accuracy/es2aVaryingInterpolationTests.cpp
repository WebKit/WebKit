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
 * \brief Varying interpolation accuracy tests.
 *//*--------------------------------------------------------------------*/

#include "es2aVaryingInterpolationTests.hpp"
#include "gluPixelTransfer.hpp"
#include "gluShaderProgram.hpp"
#include "gluShaderUtil.hpp"
#include "tcuStringTemplate.hpp"
#include "gluContextInfo.hpp"
#include "glsTextureTestUtil.hpp"
#include "tcuVector.hpp"
#include "tcuVectorUtil.hpp"
#include "tcuTestLog.hpp"
#include "tcuFloat.hpp"
#include "tcuImageCompare.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuSurfaceAccess.hpp"
#include "deRandom.hpp"
#include "deStringUtil.hpp"
#include "deString.h"

#include "glw.h"

using std::map;
using std::string;
using std::vector;
using tcu::SurfaceAccess;
using tcu::TestLog;
using tcu::Vec3;
using tcu::Vec4;

namespace deqp
{
namespace gles2
{
namespace Accuracy
{

static inline float projectedTriInterpolate(const tcu::Vec3 &s, const tcu::Vec3 &w, float nx, float ny)
{
    return (s[0] * (1.0f - nx - ny) / w[0] + s[1] * ny / w[1] + s[2] * nx / w[2]) /
           ((1.0f - nx - ny) / w[0] + ny / w[1] + nx / w[2]);
}

static void renderReference(const SurfaceAccess &dst, const float coords[4 * 3], const Vec4 &wCoord, const Vec3 &scale,
                            const Vec3 &bias)
{
    float dstW = (float)dst.getWidth();
    float dstH = (float)dst.getHeight();

    Vec3 triR[2]      = {Vec3(coords[0 * 3 + 0], coords[1 * 3 + 0], coords[2 * 3 + 0]),
                         Vec3(coords[3 * 3 + 0], coords[2 * 3 + 0], coords[1 * 3 + 0])};
    Vec3 triG[2]      = {Vec3(coords[0 * 3 + 1], coords[1 * 3 + 1], coords[2 * 3 + 1]),
                         Vec3(coords[3 * 3 + 1], coords[2 * 3 + 1], coords[1 * 3 + 1])};
    Vec3 triB[2]      = {Vec3(coords[0 * 3 + 2], coords[1 * 3 + 2], coords[2 * 3 + 2]),
                         Vec3(coords[3 * 3 + 2], coords[2 * 3 + 2], coords[1 * 3 + 2])};
    tcu::Vec3 triW[2] = {wCoord.swizzle(0, 1, 2), wCoord.swizzle(3, 2, 1)};

    for (int py = 0; py < dst.getHeight(); py++)
    {
        for (int px = 0; px < dst.getWidth(); px++)
        {
            float wx = (float)px + 0.5f;
            float wy = (float)py + 0.5f;
            float nx = wx / dstW;
            float ny = wy / dstH;

            int triNdx  = nx + ny >= 1.0f ? 1 : 0;
            float triNx = triNdx ? 1.0f - nx : nx;
            float triNy = triNdx ? 1.0f - ny : ny;

            float r = projectedTriInterpolate(triR[triNdx], triW[triNdx], triNx, triNy) * scale[0] + bias[0];
            float g = projectedTriInterpolate(triG[triNdx], triW[triNdx], triNx, triNy) * scale[1] + bias[1];
            float b = projectedTriInterpolate(triB[triNdx], triW[triNdx], triNx, triNy) * scale[2] + bias[2];

            Vec4 color = Vec4(r, g, b, 1.0f);

            dst.setPixel(color, px, py);
        }
    }
}

class InterpolationCase : public TestCase
{
public:
    InterpolationCase(Context &context, const char *name, const char *desc, glu::Precision precision,
                      const tcu::Vec3 &minVal, const tcu::Vec3 &maxVal, bool projective);
    ~InterpolationCase(void);

    IterateResult iterate(void);

private:
    glu::Precision m_precision;
    tcu::Vec3 m_min;
    tcu::Vec3 m_max;
    bool m_projective;
};

InterpolationCase::InterpolationCase(Context &context, const char *name, const char *desc, glu::Precision precision,
                                     const tcu::Vec3 &minVal, const tcu::Vec3 &maxVal, bool projective)
    : TestCase(context, tcu::NODETYPE_ACCURACY, name, desc)
    , m_precision(precision)
    , m_min(minVal)
    , m_max(maxVal)
    , m_projective(projective)
{
}

InterpolationCase::~InterpolationCase(void)
{
}

static bool isValidFloat(glu::Precision precision, float val)
{
    if (precision == glu::PRECISION_MEDIUMP)
    {
        tcu::Float16 fp16(val);
        return !fp16.isDenorm() && !fp16.isInf() && !fp16.isNaN();
    }
    else
    {
        tcu::Float32 fp32(val);
        return !fp32.isDenorm() && !fp32.isInf() && !fp32.isNaN();
    }
}

template <int Size>
static bool isValidFloatVec(glu::Precision precision, const tcu::Vector<float, Size> &vec)
{
    for (int ndx = 0; ndx < Size; ndx++)
    {
        if (!isValidFloat(precision, vec[ndx]))
            return false;
    }
    return true;
}

InterpolationCase::IterateResult InterpolationCase::iterate(void)
{
    TestLog &log = m_testCtx.getLog();
    de::Random rnd(deStringHash(getName()));
    int viewportWidth  = 128;
    int viewportHeight = 128;

    if (m_context.getRenderTarget().getWidth() < viewportWidth ||
        m_context.getRenderTarget().getHeight() < viewportHeight)
        throw tcu::NotSupportedError("Too small viewport", "", __FILE__, __LINE__);

    int viewportX = rnd.getInt(0, m_context.getRenderTarget().getWidth() - viewportWidth);
    int viewportY = rnd.getInt(0, m_context.getRenderTarget().getHeight() - viewportHeight);

    static const char *s_vertShaderTemplate = "attribute highp vec4 a_position;\n"
                                              "attribute ${PRECISION} vec3 a_coords;\n"
                                              "varying ${PRECISION} vec3 v_coords;\n"
                                              "\n"
                                              "void main (void)\n"
                                              "{\n"
                                              "    gl_Position = a_position;\n"
                                              "    v_coords = a_coords;\n"
                                              "}\n";
    static const char *s_fragShaderTemplate = "varying ${PRECISION} vec3 v_coords;\n"
                                              "uniform ${PRECISION} vec3 u_scale;\n"
                                              "uniform ${PRECISION} vec3 u_bias;\n"
                                              "\n"
                                              "void main (void)\n"
                                              "{\n"
                                              "    gl_FragColor = vec4(v_coords * u_scale + u_bias, 1.0);\n"
                                              "}\n";

    map<string, string> templateParams;
    templateParams["PRECISION"] = glu::getPrecisionName(m_precision);

    glu::ShaderProgram program(
        m_context.getRenderContext(),
        glu::makeVtxFragSources(tcu::StringTemplate(s_vertShaderTemplate).specialize(templateParams),
                                tcu::StringTemplate(s_fragShaderTemplate).specialize(templateParams)));
    log << program;
    if (!program.isOk())
    {
        if (m_precision == glu::PRECISION_HIGHP && !m_context.getContextInfo().isFragmentHighPrecisionSupported())
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "Fragment highp not supported");
        else
            m_testCtx.setTestResult(QP_TEST_RESULT_FAIL, "Compile failed");
        return STOP;
    }

    // Position coordinates.
    Vec4 wCoord       = m_projective ? Vec4(1.3f, 0.8f, 0.6f, 2.0f) : Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float positions[] = {-1.0f * wCoord.x(), -1.0f * wCoord.x(), 0.0f, wCoord.x(),
                         -1.0f * wCoord.y(), +1.0f * wCoord.y(), 0.0f, wCoord.y(),
                         +1.0f * wCoord.z(), -1.0f * wCoord.z(), 0.0f, wCoord.z(),
                         +1.0f * wCoord.w(), +1.0f * wCoord.w(), 0.0f, wCoord.w()};

    // Coordinates for interpolation.
    tcu::Vec3 scale = 1.0f / (m_max - m_min);
    tcu::Vec3 bias  = -1.0f * m_min * scale;
    float coords[]  = {(0.0f - bias[0]) / scale[0], (0.5f - bias[1]) / scale[1], (1.0f - bias[2]) / scale[2],
                       (0.5f - bias[0]) / scale[0], (1.0f - bias[1]) / scale[1], (0.5f - bias[2]) / scale[2],
                       (0.5f - bias[0]) / scale[0], (0.0f - bias[1]) / scale[1], (0.5f - bias[2]) / scale[2],
                       (1.0f - bias[0]) / scale[0], (0.5f - bias[1]) / scale[1], (0.0f - bias[2]) / scale[2]};

    log << TestLog::Message << "a_coords = " << ((tcu::Vec3(0.0f) - bias) / scale) << " -> "
        << ((tcu::Vec3(1.0f) - bias) / scale) << TestLog::EndMessage;
    log << TestLog::Message << "u_scale = " << scale << TestLog::EndMessage;
    log << TestLog::Message << "u_bias = " << bias << TestLog::EndMessage;

    // Verify that none of the inputs are denormalized / inf / nan.
    TCU_CHECK(isValidFloatVec(m_precision, scale));
    TCU_CHECK(isValidFloatVec(m_precision, bias));
    for (int ndx = 0; ndx < DE_LENGTH_OF_ARRAY(coords); ndx++)
    {
        TCU_CHECK(isValidFloat(m_precision, coords[ndx]));
        TCU_CHECK(isValidFloat(m_precision, coords[ndx] * scale[ndx % 3] + bias[ndx % 3]));
    }

    // Indices.
    static const uint16_t indices[] = {0, 1, 2, 2, 1, 3};

    {
        const int posLoc   = glGetAttribLocation(program.getProgram(), "a_position");
        const int coordLoc = glGetAttribLocation(program.getProgram(), "a_coords");

        glEnableVertexAttribArray(posLoc);
        glVertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, &positions[0]);

        glEnableVertexAttribArray(coordLoc);
        glVertexAttribPointer(coordLoc, 3, GL_FLOAT, GL_FALSE, 0, &coords[0]);
    }

    glUseProgram(program.getProgram());
    glUniform3f(glGetUniformLocation(program.getProgram(), "u_scale"), scale.x(), scale.y(), scale.z());
    glUniform3f(glGetUniformLocation(program.getProgram(), "u_bias"), bias.x(), bias.y(), bias.z());

    GLU_CHECK_MSG("After program setup");

    // Frames.
    tcu::Surface rendered(viewportWidth, viewportHeight);
    tcu::Surface reference(viewportWidth, viewportHeight);

    // Render with GL.
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
    glDrawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(indices), GL_UNSIGNED_SHORT, &indices[0]);

    // Render reference \note While GPU is hopefully doing our draw call.
    renderReference(SurfaceAccess(reference, m_context.getRenderTarget().getPixelFormat()), coords, wCoord, scale,
                    bias);

    glu::readPixels(m_context.getRenderContext(), viewportX, viewportY, rendered.getAccess());

    // Compute difference.
    const int bestScoreDiff  = 16;
    const int worstScoreDiff = 300;
    int score = tcu::measurePixelDiffAccuracy(log, "Result", "Image comparison result", reference, rendered,
                                              bestScoreDiff, worstScoreDiff, tcu::COMPARE_LOG_EVERYTHING);

    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::toString(score).c_str());
    return STOP;
}

VaryingInterpolationTests::VaryingInterpolationTests(Context &context)
    : TestCaseGroup(context, "interpolation", "Varying Interpolation Accuracy Tests")
{
}

VaryingInterpolationTests::~VaryingInterpolationTests(void)
{
}

void VaryingInterpolationTests::init(void)
{
    DE_STATIC_ASSERT(glu::PRECISION_LOWP + 1 == glu::PRECISION_MEDIUMP);
    DE_STATIC_ASSERT(glu::PRECISION_MEDIUMP + 1 == glu::PRECISION_HIGHP);

    // Exp = Emax-3, Mantissa = 0
    float minF32 = tcu::Float32((0u << 31) | (0xfcu << 23) | 0x0u).asFloat();
    float maxF32 = tcu::Float32((1u << 31) | (0xfcu << 23) | 0x0u).asFloat();
    float minF16 = tcu::Float16((uint16_t)((0u << 15) | (0x1cu << 10) | 0x0u)).asFloat();
    float maxF16 = tcu::Float16((uint16_t)((1u << 15) | (0x1cu << 10) | 0x0u)).asFloat();

    static const struct
    {
        const char *name;
        Vec3 minVal;
        Vec3 maxVal;
        glu::Precision minPrecision;
    } coordRanges[] = {
        {"zero_to_one", Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 1.0f, 1.0f), glu::PRECISION_LOWP},
        {"zero_to_minus_one", Vec3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, -1.0f, -1.0f), glu::PRECISION_LOWP},
        {"minus_one_to_one", Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f), glu::PRECISION_LOWP},
        {"minus_ten_to_ten", Vec3(-10.0f, -10.0f, -10.0f), Vec3(10.0f, 10.0f, 10.0f), glu::PRECISION_MEDIUMP},
        {"thousands", Vec3(-5e3f, 1e3f, 1e3f), Vec3(3e3f, -1e3f, 7e3f), glu::PRECISION_MEDIUMP},
        {"full_mediump", Vec3(minF16, minF16, minF16), Vec3(maxF16, maxF16, maxF16), glu::PRECISION_MEDIUMP},
        {"full_highp", Vec3(minF32, minF32, minF32), Vec3(maxF32, maxF32, maxF32), glu::PRECISION_HIGHP},
    };

    for (int precision = glu::PRECISION_LOWP; precision <= glu::PRECISION_HIGHP; precision++)
    {
        for (int coordNdx = 0; coordNdx < DE_LENGTH_OF_ARRAY(coordRanges); coordNdx++)
        {
            if (precision < (int)coordRanges[coordNdx].minPrecision)
                continue;

            string baseName =
                string(glu::getPrecisionName((glu::Precision)precision)) + "_" + coordRanges[coordNdx].name;

            addChild(new InterpolationCase(m_context, baseName.c_str(), "", (glu::Precision)precision,
                                           coordRanges[coordNdx].minVal, coordRanges[coordNdx].maxVal, false));
            addChild(new InterpolationCase(m_context, (baseName + "_proj").c_str(), "", (glu::Precision)precision,
                                           coordRanges[coordNdx].minVal, coordRanges[coordNdx].maxVal, true));
        }
    }
}

} // namespace Accuracy
} // namespace gles2
} // namespace deqp
