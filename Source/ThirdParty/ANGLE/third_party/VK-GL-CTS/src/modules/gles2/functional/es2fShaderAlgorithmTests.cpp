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
 * \brief Algorithm implementation tests.
 *//*--------------------------------------------------------------------*/

#include "es2fShaderAlgorithmTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuStringTemplate.hpp"

#include "deInt32.h"
#include "deMemory.h"

#include <map>
#include <algorithm>

using namespace std;
using namespace tcu;
using namespace glu;
using namespace deqp::gls;

namespace deqp
{
namespace gles2
{
namespace Functional
{

// ShaderAlgorithmCase

class ShaderAlgorithmCase : public ShaderRenderCase
{
public:
    ShaderAlgorithmCase(Context &context, const char *name, const char *description, bool isVertexCase,
                        ShaderEvalFunc evalFunc, const char *vertShaderSource, const char *fragShaderSource);
    virtual ~ShaderAlgorithmCase(void);

private:
    ShaderAlgorithmCase(const ShaderAlgorithmCase &);            // not allowed!
    ShaderAlgorithmCase &operator=(const ShaderAlgorithmCase &); // not allowed!
};

ShaderAlgorithmCase::ShaderAlgorithmCase(Context &context, const char *name, const char *description, bool isVertexCase,
                                         ShaderEvalFunc evalFunc, const char *vertShaderSource,
                                         const char *fragShaderSource)
    : ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name,
                       description, isVertexCase, evalFunc)
{
    m_vertShaderSource = vertShaderSource;
    m_fragShaderSource = fragShaderSource;
}

ShaderAlgorithmCase::~ShaderAlgorithmCase(void)
{
}

// Helpers.

static ShaderAlgorithmCase *createExpressionCase(Context &context, const char *caseName, const char *description,
                                                 bool isVertexCase, ShaderEvalFunc evalFunc, LineStream &shaderBody)
{
    std::ostringstream vtx;
    std::ostringstream frag;
    std::ostringstream &op = isVertexCase ? vtx : frag;

    vtx << "attribute highp vec4 a_position;\n";
    vtx << "attribute ${PRECISION} vec4 a_unitCoords;\n";

    if (isVertexCase)
    {
        vtx << "varying mediump vec3 v_color;\n";
        frag << "varying mediump vec3 v_color;\n";
    }
    else
    {
        vtx << "varying mediump vec4 v_coords;\n";
        frag << "varying mediump vec4 v_coords;\n";
    }

    // op << "uniform mediump sampler2D ut_brick;\n";

    vtx << "\n";
    vtx << "void main()\n";
    vtx << "{\n";
    vtx << "    gl_Position = a_position;\n";

    frag << "\n";
    frag << "void main()\n";
    frag << "{\n";

    // Write matrix.
    if (isVertexCase)
        op << "    ${PRECISION} vec4 coords = a_unitCoords;\n";
    else
        op << "    ${PRECISION} vec4 coords = v_coords;\n";

    op << "    ${PRECISION} vec3 res = vec3(0.0);\n";
    op << shaderBody.str();

    if (isVertexCase)
    {
        vtx << "    v_color = res;\n";
        frag << "    gl_FragColor = vec4(v_color, 1.0);\n";
    }
    else
    {
        vtx << "    v_coords = a_unitCoords;\n";
        frag << "    gl_FragColor = vec4(res, 1.0);\n";
    }

    vtx << "}\n";
    frag << "}\n";

    // Fill in shader templates.
    map<string, string> params;
    params.insert(pair<string, string>("PRECISION", "mediump"));

    StringTemplate vertTemplate(vtx.str().c_str());
    StringTemplate fragTemplate(frag.str().c_str());
    string vertexShaderSource   = vertTemplate.specialize(params);
    string fragmentShaderSource = fragTemplate.specialize(params);

    return new ShaderAlgorithmCase(context, caseName, description, isVertexCase, evalFunc, vertexShaderSource.c_str(),
                                   fragmentShaderSource.c_str());
}

// ShaderAlgorithmTests.

ShaderAlgorithmTests::ShaderAlgorithmTests(Context &context)
    : TestCaseGroup(context, "algorithm", "Miscellaneous algorithm implementations using shaders.")
{
}

ShaderAlgorithmTests::~ShaderAlgorithmTests(void)
{
}

void ShaderAlgorithmTests::init(void)
{
    // TestCaseGroup* colorGroup = new TestCaseGroup(m_testCtx, "color", "Miscellaneous color related algorithm tests.");
    // addChild(colorGroup);

#define SHADER_OP_CASE(NAME, DESCRIPTION, SHADER_OP, EVAL_FUNC_BODY)                                                  \
    do                                                                                                                \
    {                                                                                                                 \
        struct Eval_##NAME                                                                                            \
        {                                                                                                             \
            static void eval(ShaderEvalContext &c) EVAL_FUNC_BODY                                                     \
        }; /* NOLINT(EVAL_FUNC_BODY) */                                                                               \
        addChild(createExpressionCase(m_context, #NAME "_vertex", DESCRIPTION, true, &Eval_##NAME::eval, SHADER_OP)); \
        addChild(                                                                                                     \
            createExpressionCase(m_context, #NAME "_fragment", DESCRIPTION, false, &Eval_##NAME::eval, SHADER_OP));   \
    } while (false)

    SHADER_OP_CASE(hsl_to_rgb, "Conversion from HSL color space into RGB.",
                   LineStream(1) << "mediump float H = coords.x, S = coords.y, L = coords.z;"
                                 << "mediump float v = (L <= 0.5) ? (L * (1.0 + S)) : (L + S - L * S);"
                                 << "res = vec3(L); // default to gray"
                                 << "if (v > 0.0)"
                                 << "{"
                                 << "    mediump float m = L + L - v;"
                                 << "    mediump float sv = (v - m) / v;"
                                 << "    H *= 6.0;"
                                 << "    mediump int sextant = int(H);"
                                 << "    mediump float fract = H - float(sextant);"
                                 << "    mediump float vsf = v * sv * fract;"
                                 << "    mediump float mid1 = m + vsf;"
                                 << "    mediump float mid2 = m - vsf;"
                                 << "    if (sextant == 0)      res = vec3(v, mid1, m);"
                                 << "    else if (sextant == 1) res = vec3(mid2, v, m);"
                                 << "    else if (sextant == 2) res = vec3(m, v, mid1);"
                                 << "    else if (sextant == 3) res = vec3(m, mid2, v);"
                                 << "    else if (sextant == 4) res = vec3(mid1, m, v);"
                                 << "    else                   res = vec3(v, m, mid2);"
                                 << "}",
                   {
                       float H  = c.unitCoords.x();
                       float S  = c.unitCoords.y();
                       float L  = c.unitCoords.z();
                       Vec3 rgb = Vec3(L);
                       float v  = (L <= 0.5f) ? (L * (1.0f + S)) : (L + S - L * S);
                       if (v > 0.0f)
                       {
                           float m  = L + L - v;
                           float sv = (v - m) / v;
                           H *= 6.0f;
                           int sextant = int(H);
                           float fract = H - float(sextant);
                           float vsf   = v * sv * fract;
                           float mid1  = m + vsf;
                           float mid2  = m - vsf;
                           if (sextant == 0)
                               rgb = Vec3(v, mid1, m);
                           else if (sextant == 1)
                               rgb = Vec3(mid2, v, m);
                           else if (sextant == 2)
                               rgb = Vec3(m, v, mid1);
                           else if (sextant == 3)
                               rgb = Vec3(m, mid2, v);
                           else if (sextant == 4)
                               rgb = Vec3(mid1, m, v);
                           else
                               rgb = Vec3(v, m, mid2);
                       }
                       c.color.xyz() = rgb;
                   });

    SHADER_OP_CASE(rgb_to_hsl, "Conversion from RGB color space into HSL.",
                   LineStream(1) << "mediump float r = coords.x, g = coords.y, b = coords.z;"
                                 << "mediump float minVal = min(min(r, g), b);"
                                 << "mediump float maxVal = max(max(r, g), b);"
                                 << "mediump float L = (minVal + maxVal) * 0.5;"
                                 << "if (minVal == maxVal)"
                                 << "    res = vec3(0.0, 0.0, L);"
                                 << "else"
                                 << "{"
                                 << "    mediump float H;"
                                 << "    mediump float S;"
                                 << "    if (L < 0.5)"
                                 << "        S = (maxVal - minVal) / (maxVal + minVal);"
                                 << "    else"
                                 << "        S = (maxVal - minVal) / (2.0 - maxVal - minVal);"
                                 << ""
                                 << "    mediump float ooDiff = 1.0 / (maxVal - minVal);"
                                 << "    if (r == maxVal)      H = (g - b) * ooDiff;"
                                 << "    else if (g == maxVal) H = 2.0 + (b - r) * ooDiff;"
                                 << "    else                  H = 4.0 + (r - g) * ooDiff;"
                                 << "    H /= 6.0;"
                                 << ""
                                 << "    res = vec3(H, S, L);"
                                 << "}",
                   {
                       float r      = c.unitCoords.x();
                       float g      = c.unitCoords.y();
                       float b      = c.unitCoords.z();
                       float minVal = min(min(r, g), b);
                       float maxVal = max(max(r, g), b);
                       float L      = (minVal + maxVal) * 0.5f;
                       Vec3 hsl;

                       if (minVal == maxVal)
                           hsl = Vec3(0.0f, 0.0f, L);
                       else
                       {
                           float H;
                           float S;
                           if (L < 0.5f)
                               S = (maxVal - minVal) / (maxVal + minVal);
                           else
                               S = (maxVal - minVal) / (2.0f - maxVal - minVal);

                           float ooDiff = 1.0f / (maxVal - minVal);
                           if (r == maxVal)
                               H = (g - b) * ooDiff;
                           else if (g == maxVal)
                               H = 2.0f + (b - r) * ooDiff;
                           else
                               H = 4.0f + (r - g) * ooDiff;
                           H /= 6.0f;

                           hsl = Vec3(H, S, L);
                       }
                       c.color.xyz() = hsl;
                   });

    /*    SHADER_OP_CASE(image_to_grayscale, "Convert image to grayscale.",
            LineStream(1)
            << "res = texture2D(ut_brick, coords.xy).rgb;",
            {
                c.color.xyz() = Vec3(0.5f);
            });*/
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
