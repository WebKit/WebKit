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
 * \brief GLES3 shader constant expression tests
 *//*--------------------------------------------------------------------*/

#include "es3fShaderConstExprTests.hpp"

#include "glsShaderLibrary.hpp"
#include "glsShaderConstExprTests.hpp"

#include "tcuStringTemplate.hpp"
#include "gluShaderUtil.hpp"

#include "deStringUtil.hpp"
#include "deMath.h"

namespace deqp
{
namespace gles3
{
namespace Functional
{

// builtins
class ShaderConstExprBuiltinTests : public TestCaseGroup
{
public:
    ShaderConstExprBuiltinTests(Context &context) : TestCaseGroup(context, "builtin_functions", "Builtin functions")
    {
    }
    virtual ~ShaderConstExprBuiltinTests(void)
    {
    }

    void init(void);

    void addChildGroup(const char *name, const char *desc, const gls::ShaderConstExpr::TestParams *cases, int numCases);
};

void ShaderConstExprBuiltinTests::addChildGroup(const char *name, const char *desc,
                                                const gls::ShaderConstExpr::TestParams *cases, int numCases)
{
    const std::vector<tcu::TestNode *> children = createTests(
        m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), cases, numCases, glu::GLSL_VERSION_300_ES);
    tcu::TestCaseGroup *group = new tcu::TestCaseGroup(m_testCtx, name, desc);

    addChild(group);

    for (int i = 0; i < (int)children.size(); i++)
        group->addChild(children[i]);
}

void ShaderConstExprBuiltinTests::init(void)
{
    using namespace gls::ShaderConstExpr;

    // ${T} => final type, ${MT} => final type but with scalar version usable even when T is a vector

    // Trigonometry
    {
        const TestParams cases[] = {
            {"radians", "radians(${T} (90.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatRadians(90.0f)},
            {"degrees", "degrees(${T} (2.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatDegrees(2.0f)},
            {"sin", "sin(${T} (3.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatSin(3.0f)},
            {"cos", "cos(${T} (3.2))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatCos(3.2f)},
            {"tan", "tan(${T} (1.5))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatTan(1.5f)},
            {"asin", "asin(${T} (0.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatAsin(0.0f)},
            {"acos", "acos(${T} (1.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatAcos(1.0f)},
            {"atan_separate", "atan(${T} (-1.0), ${T} (-1.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT,
             deFloatAtan2(-1.0f, -1.0f)},
            {"atan_combined", "atan(${T} (2.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatAtanOver(2.0f)},
            {"sinh", "sinh(${T} (1.5))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatSinh(1.5f)},
            {"cosh", "cosh(${T} (1.5))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatCosh(1.5f)},
            {"tanh", "tanh(${T} (1.5))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatTanh(1.5f)},
            {"asinh", "asinh(${T} (2.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatAsinh(2.0f)},
            {"acosh", "acosh(${T} (2.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatAcosh(2.0f)},
            {"atanh", "atanh(${T} (0.8))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatAtanh(0.8f)},
        };

        addChildGroup("angle_and_trigonometry", "Angles and Trigonometry", cases, DE_LENGTH_OF_ARRAY(cases));
    }
    // Exponential
    {
        const TestParams cases[] = {
            {"pow", "pow(${T} (1.7), ${T} (3.5))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatPow(1.7f, 3.5f)},
            {"exp", "exp(${T} (4.2))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatExp(4.2f)},
            {"log", "log(${T} (42.12))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatLog(42.12f)},
            {"exp2", "exp2(${T} (6.7))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatExp2(6.7f)},
            {"log2", "log2(${T} (100.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatLog2(100.0f)},
            {"sqrt", "sqrt(${T} (10.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatSqrt(10.0f)},
            {"inversesqrt", "inversesqrt(${T} (10.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatRsq(10.0f)},
        };

        addChildGroup("exponential", "Exponential", cases, DE_LENGTH_OF_ARRAY(cases));
    }
    // Common
    {
        const TestParams cases[] = {
            {"abs", "abs(${T} (-42.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, 42.0f},
            {"abs", "abs(${T} (-42))", glu::TYPE_INT, 1, 4, glu::TYPE_INT, 42.0f},
            {"sign", "sign(${T} (-18.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, -1.0f},
            {"sign", "sign(${T} (-18))", glu::TYPE_INT, 1, 4, glu::TYPE_INT, -1.0f},
            {"floor", "floor(${T} (37.3))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatFloor(37.3f)},
            {"trunc", "trunc(${T} (-1.8))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, -1.0f},
            {"round", "round(${T} (42.7))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, 42.0f},
            {"roundEven", "roundEven(${T} (1.5))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, 2.0f},
            {"ceil", "ceil(${T} (82.2))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatCeil(82.2f)},
            {"fract", "fract(${T} (17.75))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatFrac(17.75f)},
            {"mod", "mod(${T} (87.65), ${MT} (3.7))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, deFloatMod(87.65f, 3.7f)},
            // modf cannot be tested due to lacking valid ways of using the 'out' parameter in a constant expression
            {"min", "min(${T} (12.3), ${MT} (32.1))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, 12.3f},
            {"min", "min(${T} (13), ${MT} (-14))", glu::TYPE_INT, 1, 4, glu::TYPE_INT, -14.0f},
            {"min", "min(${T} (13), ${MT} (14))", glu::TYPE_UINT, 1, 4, glu::TYPE_UINT, 13.0f},
            {"max", "max(${T} (12.3), ${MT} (32.1))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, 32.1f},
            {"max", "max(${T} (13), ${MT} (-14))", glu::TYPE_INT, 1, 4, glu::TYPE_INT, 13.0f},
            {"max", "max(${T} (13), ${MT} (14))", glu::TYPE_UINT, 1, 4, glu::TYPE_UINT, 14.0f},
            {"clamp", "clamp(${T} (42.1), ${MT} (10.0), ${MT} (15.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, 15.0f},
            {"clamp", "clamp(${T} (42), ${MT} (-10), ${MT} (15))", glu::TYPE_INT, 1, 4, glu::TYPE_INT, 15.0f},
            {"clamp", "clamp(${T} (42), ${MT} (10), ${MT} (15))", glu::TYPE_UINT, 1, 4, glu::TYPE_UINT, 15.0f},

            {"mix", "mix(${T} (10.0), ${T} (20.0), ${MT}(0.75))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, 17.5f},
            {"mix_float_bool", "mix(float(10.0), float(20.0), bool(1))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 20.0f},
            {"mix_vec2_bvec2", "mix(vec2(10.0), vec2(20.0), bvec2(1)).x", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             20.0f},
            {"mix_vec3_bvec3", "mix(vec3(10.0), vec3(20.0), bvec3(1)).x", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             20.0f},
            {"mix_vec4_bvec4", "mix(vec4(10.0), vec4(20.0), bvec4(1)).x", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             20.0f},

            {"step", "step(${MT} (3.2), ${T} (4.2))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, 1.0f},
            {"smoothstep", "smoothstep(${MT} (3.0), ${MT} (5.0), ${T} (4.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT,
             0.5f},
            {"isnan", "isnan(${T} (1.3))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_BOOL, 0.0f},
            {"isinf", "isinf(${T} (1.3))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_BOOL, 0.0f},
            {"floatbits_int", "intBitsToFloat(floatBitsToInt(42.12))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 42.12f},
            {"floatbits_uint", "uintBitsToFloat(floatBitsToUint(-14.2))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             -14.2f},
        };

        addChildGroup("common", "Common", cases, DE_LENGTH_OF_ARRAY(cases));
    }
    // Floating point pack & unpack
    {
        const TestParams cases[] = {
            {"packSnorm2x16", "packSnorm2x16(vec2(0.7, 0.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_UINT, 22937.0f},
            {"unpackSnorm2x16", "unpackSnorm2x16(22937u).x", glu::TYPE_UINT, 1, 1, glu::TYPE_FLOAT, 0.7f},
            {"packUnorm2x16", "packUnorm2x16(vec2(0.6, -0.3))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_UINT, 39321.0f},
            {"unpackUnorm2x16", "unpackUnorm2x16(39321u).x", glu::TYPE_UINT, 1, 1, glu::TYPE_FLOAT, 0.6f},
            {"packHalf2x16", "unpackHalf2x16(packHalf2x16(vec2(0.3, 0.1))).x", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             0.3f},
            // \todo [2014-01-29 otto] Separate testing of half-precision pack & unpack
        };

        addChildGroup("float_pack_unpack", "Floating point pack & unpack", cases, DE_LENGTH_OF_ARRAY(cases));
    }
    // Geometric
    {
        const TestParams cases[] = {
            {"length_float", "length(1.0)", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 1.0f},
            {"length_vec2", "length(vec2(1.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, deFloatSqrt(2.0f)},
            {"length_vec3", "length(vec3(1.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, deFloatSqrt(3.0f)},
            {"length_vec4", "length(vec4(1.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, deFloatSqrt(4.0f)},

            {"distance_float", "distance(1.0, 2.0)", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 1.0f},
            {"distance_vec2", "distance(vec2(1.0), vec2(2.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             deFloatSqrt(2.0f)},
            {"distance_vec3", "distance(vec3(1.0), vec3(2.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             deFloatSqrt(3.0f)},
            {"distance_vec4", "distance(vec4(1.0), vec4(2.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             deFloatSqrt(4.0f)},

            {"dot_float", "dot(1.0, 1.0)", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 1.0f},
            {"dot_vec2", "dot(vec2(1.0), vec2(1.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 2.0f},
            {"dot_vec3", "dot(vec3(1.0), vec3(1.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 3.0f},
            {"dot_vec4", "dot(vec4(1.0), vec4(1.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 4.0f},

            {"normalize_float", "normalize(1.0)", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 1.0f},
            {"normalize_vec2", "normalize(vec2(1.0)).x", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, deFloatRsq(2.0f)},
            {"normalize_vec3", "normalize(vec3(1.0)).x", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, deFloatRsq(3.0f)},
            {"normalize_vec4", "normalize(vec4(1.0)).x", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, deFloatRsq(4.0f)},

            {"faceforward", "faceforward(${T} (1.0), ${T} (1.0), ${T} (1.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT,
             -1.0f},

            // reflect(I, N) => I - 2*dot(N, I)*N
            {"reflect_float", "reflect(1.0, 1.0)", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, -1.0f},
            {"reflect_vec2", "reflect(vec2(1.0), vec2(1.0, 0.0)).x", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, -1.0f},
            {"reflect_vec3", "reflect(vec3(1.0), vec3(1.0, 0.0, 0.0)).x", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             -1.0f},
            {"reflect_vec4", "reflect(vec4(1.0), vec4(1.0, 0.0, 0.0, 0.0)).x", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             -1.0f},

            /*
            genType refract(genType I, genType N, float eta) =>
                k = 1.0 - (eta^2)*(1.0-dot(N,I)^2)
                if k < 0 return 0.0
                else return eta*I - (eta*dot(N,I) + sqrt(k))*N
            */
            {"refract_float", "refract(1.0, 1.0, 0.5)", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, -1.0f},
            {"refract_vec2", "refract(vec2(1.0), vec2(1.0, 0.0), 0.5).x", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             -1.0f},
            {"refract_vec3", "refract(vec3(1.0), vec3(1.0, 0.0, 0.0), 0.5).x", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             -1.0f},
            {"refract_vec4", "refract(vec4(1.0), vec4(1.0, 0.0, 0.0, 0.0), 0.5).x", glu::TYPE_FLOAT, 1, 1,
             glu::TYPE_FLOAT, -1.0f},
        };

        addChildGroup("geometric", "Geometric", cases, DE_LENGTH_OF_ARRAY(cases));
    }
    // Matrix
    {
        const TestParams cases[] = {
            {"compMult_mat2", "matrixCompMult(mat2(1.0), mat2(1.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             1.0f},
            {"compMult_mat3", "matrixCompMult(mat3(1.0), mat3(1.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             1.0f},
            {"compMult_mat4", "matrixCompMult(mat4(1.0), mat4(1.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             1.0f},
            {"outerProd_mat2", "outerProduct(vec2(3.0), vec2(3.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             9.0f},
            {"outerProd_mat3", "outerProduct(vec3(3.0), vec3(3.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             9.0f},
            {"outerProd_mat4", "outerProduct(vec4(3.0), vec4(3.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             9.0f},

            {"outerProd_mat2x3", "outerProduct(vec3(3.0), vec2(3.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             9.0f},
            {"outerProd_mat3x2", "outerProduct(vec2(3.0), vec3(3.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             9.0f},
            {"outerProd_mat2x4", "outerProduct(vec4(3.0), vec2(3.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             9.0f},
            {"outerProd_mat4x2", "outerProduct(vec2(3.0), vec4(3.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             9.0f},
            {"outerProd_mat3x4", "outerProduct(vec4(3.0), vec3(3.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             9.0f},
            {"outerProd_mat4x3", "outerProduct(vec3(3.0), vec4(3.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT,
             9.0f},

            {"transpose_mat2", "transpose(mat2(2.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 2.0f},
            {"transpose_mat3", "transpose(mat3(2.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 2.0f},
            {"transpose_mat4", "transpose(mat4(2.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 2.0f},
            {"transpose_mat3x2", "transpose(mat3x2(2.3))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 2.3f},
            {"transpose_mat2x3", "transpose(mat2x3(2.3))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 2.3f},
            {"transpose_mat4x2", "transpose(mat4x2(2.3))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 2.3f},
            {"transpose_mat4x3", "transpose(mat4x3(2.3))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 2.3f},
            {"transpose_mat2x4", "transpose(mat2x4(2.3))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 2.3f},
            {"transpose_mat3x4", "transpose(mat3x4(2.3))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 2.3f},

            {"determinant_mat2", "determinant(mat2(2.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 4.0f},
            {"determinant_mat3", "determinant(mat3(2.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 8.0f},
            {"determinant_mat4", "determinant(mat4(2.0))", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 16.0f},

            {"inverse_mat2", "inverse(mat2(2.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 0.5f},
            {"inverse_mat3", "inverse(mat3(2.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 0.5f},
            {"inverse_mat4", "inverse(mat4(2.0))[0][0]", glu::TYPE_FLOAT, 1, 1, glu::TYPE_FLOAT, 0.5f},
        };

        addChildGroup("matrix", "Matrix", cases, DE_LENGTH_OF_ARRAY(cases));
    }
    // Vector relational
    {
        const TestParams cases[] = {
            {"lessThan", "lessThan(${T} (1.0), ${T} (2.0))", glu::TYPE_FLOAT, 2, 4, glu::TYPE_BOOL, 1.0f},
            {"lessThan", "lessThan(${T} (-1), ${T} (2))", glu::TYPE_INT, 2, 4, glu::TYPE_BOOL, 1.0f},
            {"lessThan", "lessThan(${T} (1), ${T} (2))", glu::TYPE_UINT, 2, 4, glu::TYPE_BOOL, 1.0f},
            {"lessThanEqual", "lessThanEqual(${T} (1.0), ${T} (1.0))", glu::TYPE_FLOAT, 2, 4, glu::TYPE_BOOL, 1.0f},
            {"lessThanEqual", "lessThanEqual(${T} (-1), ${T} (-1))", glu::TYPE_INT, 2, 4, glu::TYPE_BOOL, 1.0f},
            {"lessThanEqual", "lessThanEqual(${T} (1), ${T} (1))", glu::TYPE_UINT, 2, 4, glu::TYPE_BOOL, 1.0f},
            {"greaterThan", "greaterThan(${T} (1.0), ${T} (2.0))", glu::TYPE_FLOAT, 2, 4, glu::TYPE_BOOL, 0.0f},
            {"greaterThan", "greaterThan(${T} (-1), ${T} (2))", glu::TYPE_INT, 2, 4, glu::TYPE_BOOL, 0.0f},
            {"greaterThan", "greaterThan(${T} (1), ${T} (2))", glu::TYPE_UINT, 2, 4, glu::TYPE_BOOL, 0.0f},
            {"greaterThanEqual", "greaterThanEqual(${T} (1.0), ${T} (2.0))", glu::TYPE_FLOAT, 2, 4, glu::TYPE_BOOL,
             0.0f},
            {"greaterThanEqual", "greaterThanEqual(${T} (-1), ${T} (2))", glu::TYPE_INT, 2, 4, glu::TYPE_BOOL, 0.0f},
            {"greaterThanEqual", "greaterThanEqual(${T} (1), ${T} (2))", glu::TYPE_UINT, 2, 4, glu::TYPE_BOOL, 0.0f},
            {"equal", "equal(${T} (1.0), ${T} (1.2))", glu::TYPE_FLOAT, 2, 4, glu::TYPE_BOOL, 0.0f},
            {"equal", "equal(${T} (1), ${T} (-2))", glu::TYPE_INT, 2, 4, glu::TYPE_BOOL, 0.0f},
            {"equal", "equal(${T} (1), ${T} (2))", glu::TYPE_UINT, 2, 4, glu::TYPE_BOOL, 0.0f},
            {"equal", "equal(${T} (true), ${T} (false))", glu::TYPE_BOOL, 2, 4, glu::TYPE_BOOL, 0.0f},
            {"notEqual", "notEqual(${T} (1.0), ${T} (1.2))", glu::TYPE_FLOAT, 2, 4, glu::TYPE_BOOL, 1.0f},
            {"notEqual", "notEqual(${T} (1), ${T} (-2))", glu::TYPE_INT, 2, 4, glu::TYPE_BOOL, 1.0f},
            {"notEqual", "notEqual(${T} (1), ${T} (2))", glu::TYPE_UINT, 2, 4, glu::TYPE_BOOL, 1.0f},
            {"notEqual", "notEqual(${T} (true), ${T} (false))", glu::TYPE_BOOL, 2, 4, glu::TYPE_BOOL, 1.0f},
            {"any_bvec2", "any(bvec2(true, false))", glu::TYPE_BOOL, 1, 1, glu::TYPE_BOOL, 1.0f},
            {"any_bvec3", "any(bvec3(true, false, false))", glu::TYPE_BOOL, 1, 1, glu::TYPE_BOOL, 1.0f},
            {"any_bvec4", "any(bvec4(true, false, false, false))", glu::TYPE_BOOL, 1, 1, glu::TYPE_BOOL, 1.0f},
            {"all_bvec2", "all(bvec2(true, false))", glu::TYPE_BOOL, 1, 1, glu::TYPE_BOOL, 0.0f},
            {"all_bvec3", "all(bvec3(true, false, false))", glu::TYPE_BOOL, 1, 1, glu::TYPE_BOOL, 0.0f},
            {"all_bvec4", "all(bvec4(true, false, false, false))", glu::TYPE_BOOL, 1, 1, glu::TYPE_BOOL, 0.0f},
            {"not", "not(${T} (false))", glu::TYPE_BOOL, 2, 4, glu::TYPE_BOOL, 1.0f},
        };

        addChildGroup("vector_relational", "Vector relational", cases, DE_LENGTH_OF_ARRAY(cases));
    }
    // Fragment processing (must return zero when used in initilizer with constexpr arguement)
    {
        const TestParams cases[] = {
            {"dFdx", "dFdx(${T} (123.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, 0.0},
            {"dFdy", "dFdx(${T} (234.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, 0.0},
            {"fwidth", "fwidth(${T} (345.0))", glu::TYPE_FLOAT, 1, 4, glu::TYPE_FLOAT, 0.0},
        };

        const std::vector<tcu::TestNode *> children =
            createTests(m_testCtx, m_context.getRenderContext(), m_context.getContextInfo(), cases,
                        DE_LENGTH_OF_ARRAY(cases), glu::GLSL_VERSION_300_ES, SHADER_FRAGMENT);
        tcu::TestCaseGroup *group = new tcu::TestCaseGroup(m_testCtx, "fragment_processing", "Fragment processing");

        addChild(group);

        for (int i = 0; i < (int)children.size(); i++)
            group->addChild(children[i]);
    }
}

// all
ShaderConstExprTests::ShaderConstExprTests(Context &context)
    : TestCaseGroup(context, "constant_expressions", "Constant expressions")
{
}

ShaderConstExprTests::~ShaderConstExprTests(void)
{
}

void ShaderConstExprTests::init(void)
{
    const std::vector<tcu::TestNode *> children =
        gls::ShaderLibrary(m_testCtx, m_context.getRenderContext(), m_context.getContextInfo())
            .loadShaderFile("shaders/constant_expressions.test");

    for (int i = 0; i < (int)children.size(); i++)
        addChild(children[i]);

    addChild(new ShaderConstExprBuiltinTests(m_context));
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
