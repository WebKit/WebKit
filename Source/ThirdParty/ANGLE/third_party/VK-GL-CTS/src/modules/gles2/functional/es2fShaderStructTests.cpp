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
 * \brief Shader struct tests.
 *//*--------------------------------------------------------------------*/

#include "es2fShaderStructTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "tcuStringTemplate.hpp"
#include "gluTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "glwEnums.hpp"
#include "glwFunctions.hpp"
#include "deMath.h"

using tcu::StringTemplate;

using std::ostringstream;
using std::string;
using std::vector;

using namespace glu;
using namespace deqp::gls;

namespace deqp
{
namespace gles2
{
namespace Functional
{

enum
{
    TEXTURE_BRICK = 0 //!< Unit index for brick texture
};

enum CaseFlags
{
    FLAG_USES_TEXTURES             = (1 << 0),
    FLAG_REQUIRES_DYNAMIC_LOOPS    = (1 << 1),
    FLAG_REQUIRES_DYNAMIC_INDEXING = (1 << 2),
    FLAG_REQUIRES_HIGHP_FRAGMENT   = (1 << 3),
};

typedef void (*SetupUniformsFunc)(const glw::Functions &gl, uint32_t programID, const tcu::Vec4 &constCoords);

class ShaderStructCase : public ShaderRenderCase
{
public:
    ShaderStructCase(Context &context, const char *name, const char *description, bool isVertexCase, uint32_t flags,
                     ShaderEvalFunc evalFunc, SetupUniformsFunc setupUniformsFunc, const char *vertShaderSource,
                     const char *fragShaderSource);
    ~ShaderStructCase(void);

    void init(void);
    void deinit(void);

    virtual void setupUniforms(int programID, const tcu::Vec4 &constCoords);

private:
    ShaderStructCase(const ShaderStructCase &);
    ShaderStructCase &operator=(const ShaderStructCase &);

    const SetupUniformsFunc m_setupUniforms;
    const uint32_t m_flags;

    glu::Texture2D *m_brickTexture;
};

ShaderStructCase::ShaderStructCase(Context &context, const char *name, const char *description, bool isVertexCase,
                                   uint32_t flags, ShaderEvalFunc evalFunc, SetupUniformsFunc setupUniformsFunc,
                                   const char *vertShaderSource, const char *fragShaderSource)
    : ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name,
                       description, isVertexCase, evalFunc)
    , m_setupUniforms(setupUniformsFunc)
    , m_flags(flags)
    , m_brickTexture(nullptr)
{
    m_vertShaderSource = vertShaderSource;
    m_fragShaderSource = fragShaderSource;
}

ShaderStructCase::~ShaderStructCase(void)
{
    delete m_brickTexture;
}

void ShaderStructCase::init(void)
{
    try
    {
        gls::ShaderRenderCase::init();
    }
    catch (const CompileFailed &)
    {
        if (m_flags & FLAG_REQUIRES_DYNAMIC_LOOPS)
        {
            const bool isSupported =
                m_isVertexCase ? m_ctxInfo.isVertexDynamicLoopSupported() : m_ctxInfo.isFragmentDynamicLoopSupported();
            if (!isSupported)
                throw tcu::NotSupportedError("Dynamic loops not supported");
        }

        if ((m_flags & FLAG_USES_TEXTURES) && m_isVertexCase)
        {
            int numTextures = 0;
            m_renderCtx.getFunctions().getIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &numTextures);
            if (numTextures == 0)
                throw tcu::NotSupportedError("Vertex shader texture access not supported");
        }

        if (m_flags & FLAG_REQUIRES_DYNAMIC_INDEXING)
            throw tcu::NotSupportedError("Dynamic indexing not supported");

        if (!m_isVertexCase && (m_flags & FLAG_REQUIRES_HIGHP_FRAGMENT) &&
            !m_ctxInfo.isFragmentHighPrecisionSupported())
            throw tcu::NotSupportedError("Highp in fragment shaders not supported");

        throw;
    }

    if (m_flags & FLAG_USES_TEXTURES)
    {
        m_brickTexture = glu::Texture2D::create(m_renderCtx, m_ctxInfo, m_testCtx.getArchive(), "data/brick.png");
        m_textures.push_back(TextureBinding(
            m_brickTexture, tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
                                         tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::LINEAR, tcu::Sampler::LINEAR)));
        DE_ASSERT(m_textures.size() == 1);
    }
}

void ShaderStructCase::deinit(void)
{
    gls::ShaderRenderCase::deinit();
    delete m_brickTexture;
    m_brickTexture = nullptr;
}

void ShaderStructCase::setupUniforms(int programID, const tcu::Vec4 &constCoords)
{
    ShaderRenderCase::setupUniforms(programID, constCoords);
    if (m_setupUniforms)
        m_setupUniforms(m_renderCtx.getFunctions(), programID, constCoords);
}

static ShaderStructCase *createStructCase(Context &context, const char *name, const char *description,
                                          bool isVertexCase, uint32_t flags, ShaderEvalFunc evalFunc,
                                          SetupUniformsFunc setupUniforms, const LineStream &shaderSrc,
                                          const std::map<std::string, std::string> *additionalParams)
{
    static const char *defaultVertSrc = "attribute highp vec4 a_position;\n"
                                        "attribute highp vec4 a_coords;\n"
                                        "varying mediump vec4 v_coords;\n\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    v_coords = a_coords;\n"
                                        "    gl_Position = a_position;\n"
                                        "}\n";
    static const char *defaultFragSrc = "varying mediump vec4 v_color;\n\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    gl_FragColor = v_color;\n"
                                        "}\n";

    // Fill in specialization parameters.
    std::map<std::string, std::string> spParams;
    if (isVertexCase)
    {
        spParams["DECLARATIONS"] = "attribute highp vec4 a_position;\n"
                                   "attribute highp vec4 a_coords;\n"
                                   "varying mediump vec4 v_color;";
        spParams["COORDS"]       = "a_coords";
        spParams["DST"]          = "v_color";
        spParams["ASSIGN_POS"]   = "gl_Position = a_position;";
    }
    else
    {
        spParams["DECLARATIONS"] = "varying mediump vec4 v_coords;";
        spParams["COORDS"]       = "v_coords";
        spParams["DST"]          = "gl_FragColor";
        spParams["ASSIGN_POS"]   = "";
    }
    if (additionalParams)
        spParams.insert(additionalParams->begin(), additionalParams->end());

    if (isVertexCase)
        return new ShaderStructCase(context, name, description, isVertexCase, flags, evalFunc, setupUniforms,
                                    StringTemplate(shaderSrc.str()).specialize(spParams).c_str(), defaultFragSrc);
    else
        return new ShaderStructCase(context, name, description, isVertexCase, flags, evalFunc, setupUniforms,
                                    defaultVertSrc, StringTemplate(shaderSrc.str()).specialize(spParams).c_str());
}

class LocalStructTests : public TestCaseGroup
{
public:
    LocalStructTests(Context &context) : TestCaseGroup(context, "local", "Local structs")
    {
    }

    ~LocalStructTests(void)
    {
    }

    virtual void init(void);
};

void LocalStructTests::init(void)
{
#define LOCAL_STRUCT_CASE_PARAMETERIZED(NAME, DESCRIPTION, FLAGS, SHADER_SRC, EVAL_FUNC_BODY, PARAMS)                \
    do                                                                                                               \
    {                                                                                                                \
        struct Eval_##NAME                                                                                           \
        {                                                                                                            \
            static void eval(ShaderEvalContext &c) EVAL_FUNC_BODY                                                    \
        }; /* NOLINT(EVAL_FUNC_BODY) */                                                                              \
        addChild(createStructCase(m_context, #NAME "_vertex", DESCRIPTION, true, FLAGS, &Eval_##NAME::eval, nullptr, \
                                  SHADER_SRC, PARAMS));                                                              \
        addChild(createStructCase(m_context, #NAME "_fragment", DESCRIPTION, false, FLAGS, &Eval_##NAME::eval,       \
                                  nullptr, SHADER_SRC, PARAMS));                                                     \
    } while (false)

#define LOCAL_STRUCT_CASE(NAME, DESCRIPTION, FLAGS, SHADER_SRC, EVAL_FUNC_BODY) \
    LOCAL_STRUCT_CASE_PARAMETERIZED(NAME, DESCRIPTION, FLAGS, SHADER_SRC, EVAL_FUNC_BODY, nullptr)

    LOCAL_STRUCT_CASE(basic, "Basic struct usage", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_one;"
                                   << ""
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    mediump vec3    b;"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s = S(${COORDS}.x, vec3(0.0), ui_one);"
                                   << "    s.b = ${COORDS}.yzw;"
                                   << "    ${DST} = vec4(s.a, s.b.x, s.b.y, s.c);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(0, 1, 2); });

    LOCAL_STRUCT_CASE(nested, "Nested struct", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << ""
                                   << "struct T {"
                                   << "    int                a;"
                                   << "    mediump vec2    b;"
                                   << "};"
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    T                b;"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s = S(${COORDS}.x, T(0, vec2(0.0)), ui_one);"
                                   << "    s.b = T(ui_zero, ${COORDS}.yz);"
                                   << "    ${DST} = vec4(s.a, s.b.b, s.b.a + s.c);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(0, 1, 2); });

    LOCAL_STRUCT_CASE(array_member, "Struct with array member", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_one;"
                                   << ""
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    mediump float    b[3];"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s;"
                                   << "    s.a = ${COORDS}.w;"
                                   << "    s.c = ui_one;"
                                   << "    s.b[0] = ${COORDS}.z;"
                                   << "    s.b[1] = ${COORDS}.y;"
                                   << "    s.b[2] = ${COORDS}.x;"
                                   << "    ${DST} = vec4(s.a, s.b[0], s.b[1], s.c);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(3, 2, 1); });

    LOCAL_STRUCT_CASE(array_member_dynamic_index, "Struct with array member, dynamic indexing",
                      FLAG_REQUIRES_DYNAMIC_INDEXING,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << "uniform int ui_two;"
                                   << ""
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    mediump float    b[3];"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s;"
                                   << "    s.a = ${COORDS}.w;"
                                   << "    s.c = ui_one;"
                                   << "    s.b[0] = ${COORDS}.z;"
                                   << "    s.b[1] = ${COORDS}.y;"
                                   << "    s.b[2] = ${COORDS}.x;"
                                   << "    ${DST} = vec4(s.b[ui_one], s.b[ui_zero], s.b[ui_two], s.c);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(1, 2, 0); });

    LOCAL_STRUCT_CASE(struct_array, "Struct array", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << "uniform int ui_two;"
                                   << ""
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    mediump int        b;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s[3];"
                                   << "    s[0] = S(${COORDS}.x, ui_zero);"
                                   << "    s[1].a = ${COORDS}.y;"
                                   << "    s[1].b = ui_one;"
                                   << "    s[2] = S(${COORDS}.z, ui_two);"
                                   << "    ${DST} = vec4(s[2].a, s[1].a, s[0].a, s[2].b - s[1].b + s[0].b);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(2, 1, 0); });

    LOCAL_STRUCT_CASE(
        struct_array_dynamic_index, "Struct array with dynamic indexing", FLAG_REQUIRES_DYNAMIC_INDEXING,
        LineStream()
            << "${DECLARATIONS}"
            << "uniform int ui_zero;"
            << "uniform int ui_one;"
            << "uniform int ui_two;"
            << ""
            << "struct S {"
            << "    mediump float    a;"
            << "    mediump int        b;"
            << "};"
            << ""
            << "void main (void)"
            << "{"
            << "    S s[3];"
            << "    s[0] = S(${COORDS}.x, ui_zero);"
            << "    s[1].a = ${COORDS}.y;"
            << "    s[1].b = ui_one;"
            << "    s[2] = S(${COORDS}.z, ui_two);"
            << "    ${DST} = vec4(s[ui_two].a, s[ui_one].a, s[ui_zero].a, s[ui_two].b - s[ui_one].b + s[ui_zero].b);"
            << "    ${ASSIGN_POS}"
            << "}",
        { c.color.xyz() = c.coords.swizzle(2, 1, 0); });

    LOCAL_STRUCT_CASE(
        nested_struct_array, "Nested struct array", 0,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_zero;"
                     << "uniform int ui_one;"
                     << "uniform int ui_two;"
                     << "uniform mediump float uf_two;"
                     << "uniform mediump float uf_three;"
                     << "uniform mediump float uf_four;"
                     << "uniform mediump float uf_half;"
                     << "uniform mediump float uf_third;"
                     << "uniform mediump float uf_fourth;"
                     << ""
                     << "struct T {"
                     << "    mediump float    a;"
                     << "    mediump vec2    b[2];"
                     << "};"
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    T                b[3];"
                     << "    int                c;"
                     << "};"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    S s[2];"
                     << ""
                     << "    // S[0]"
                     << "    s[0].a         = ${COORDS}.x;"
                     << "    s[0].b[0].a    = uf_half;"
                     << "    s[0].b[0].b[0] = ${COORDS}.xy;"
                     << "    s[0].b[0].b[1] = ${COORDS}.zw;"
                     << "    s[0].b[1].a    = uf_third;"
                     << "    s[0].b[1].b[0] = ${COORDS}.zw;"
                     << "    s[0].b[1].b[1] = ${COORDS}.xy;"
                     << "    s[0].b[2].a    = uf_fourth;"
                     << "    s[0].b[2].b[0] = ${COORDS}.xz;"
                     << "    s[0].b[2].b[1] = ${COORDS}.yw;"
                     << "    s[0].c         = ui_zero;"
                     << ""
                     << "    // S[1]"
                     << "    s[1].a         = ${COORDS}.w;"
                     << "    s[1].b[0].a    = uf_two;"
                     << "    s[1].b[0].b[0] = ${COORDS}.xx;"
                     << "    s[1].b[0].b[1] = ${COORDS}.yy;"
                     << "    s[1].b[1].a    = uf_three;"
                     << "    s[1].b[1].b[0] = ${COORDS}.zz;"
                     << "    s[1].b[1].b[1] = ${COORDS}.ww;"
                     << "    s[1].b[2].a    = uf_four;"
                     << "    s[1].b[2].b[0] = ${COORDS}.yx;"
                     << "    s[1].b[2].b[1] = ${COORDS}.wz;"
                     << "    s[1].c         = ui_one;"
                     << ""
                     << "    mediump float r = (s[0].b[1].b[0].x + s[1].b[2].b[1].y) * s[0].b[0].a; // (z + z) * 0.5"
                     << "    mediump float g = s[1].b[0].b[0].y * s[0].b[2].a * s[1].b[2].a; // x * 0.25 * 4"
                     << "    mediump float b = (s[0].b[2].b[1].y + s[0].b[1].b[0].y + s[1].a) * s[0].b[1].a; // (w + w "
                        "+ w) * 0.333"
                     << "    mediump float a = float(s[0].c) + s[1].b[2].a - s[1].b[1].a; // 0 + 4.0 - 3.0"
                     << "    ${DST} = vec4(r, g, b, a);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        { c.color.xyz() = c.coords.swizzle(2, 0, 3); });

    LOCAL_STRUCT_CASE(
        nested_struct_array_dynamic_index, "Nested struct array with dynamic indexing", FLAG_REQUIRES_DYNAMIC_INDEXING,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_zero;"
                     << "uniform int ui_one;"
                     << "uniform int ui_two;"
                     << "uniform mediump float uf_two;"
                     << "uniform mediump float uf_three;"
                     << "uniform mediump float uf_four;"
                     << "uniform mediump float uf_half;"
                     << "uniform mediump float uf_third;"
                     << "uniform mediump float uf_fourth;"
                     << ""
                     << "struct T {"
                     << "    mediump float    a;"
                     << "    mediump vec2    b[2];"
                     << "};"
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    T                b[3];"
                     << "    int                c;"
                     << "};"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    S s[2];"
                     << ""
                     << "    // S[0]"
                     << "    s[0].a         = ${COORDS}.x;"
                     << "    s[0].b[0].a    = uf_half;"
                     << "    s[0].b[0].b[0] = ${COORDS}.xy;"
                     << "    s[0].b[0].b[1] = ${COORDS}.zw;"
                     << "    s[0].b[1].a    = uf_third;"
                     << "    s[0].b[1].b[0] = ${COORDS}.zw;"
                     << "    s[0].b[1].b[1] = ${COORDS}.xy;"
                     << "    s[0].b[2].a    = uf_fourth;"
                     << "    s[0].b[2].b[0] = ${COORDS}.xz;"
                     << "    s[0].b[2].b[1] = ${COORDS}.yw;"
                     << "    s[0].c         = ui_zero;"
                     << ""
                     << "    // S[1]"
                     << "    s[1].a         = ${COORDS}.w;"
                     << "    s[1].b[0].a    = uf_two;"
                     << "    s[1].b[0].b[0] = ${COORDS}.xx;"
                     << "    s[1].b[0].b[1] = ${COORDS}.yy;"
                     << "    s[1].b[1].a    = uf_three;"
                     << "    s[1].b[1].b[0] = ${COORDS}.zz;"
                     << "    s[1].b[1].b[1] = ${COORDS}.ww;"
                     << "    s[1].b[2].a    = uf_four;"
                     << "    s[1].b[2].b[0] = ${COORDS}.yx;"
                     << "    s[1].b[2].b[1] = ${COORDS}.wz;"
                     << "    s[1].c         = ui_one;"
                     << ""
                     << "    mediump float r = (s[0].b[ui_one].b[ui_one-1].x + s[ui_one].b[ui_two].b[ui_zero+1].y) * "
                        "s[0].b[0].a; // (z + z) * 0.5"
                     << "    mediump float g = s[ui_two-1].b[ui_two-2].b[ui_zero].y * s[0].b[ui_two].a * "
                        "s[ui_one].b[2].a; // x * 0.25 * 4"
                     << "    mediump float b = (s[ui_zero].b[ui_one+1].b[1].y + s[0].b[ui_one*ui_one].b[0].y + "
                        "s[ui_one].a) * s[0].b[ui_two-ui_one].a; // (w + w + w) * 0.333"
                     << "    mediump float a = float(s[ui_zero].c) + s[ui_one-ui_zero].b[ui_two].a - "
                        "s[ui_zero+ui_one].b[ui_two-ui_one].a; // 0 + 4.0 - 3.0"
                     << "    ${DST} = vec4(r, g, b, a);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        { c.color.xyz() = c.coords.swizzle(2, 0, 3); });

    LOCAL_STRUCT_CASE(parameter, "Struct as a function parameter", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_one;"
                                   << ""
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    mediump vec3    b;"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "mediump vec4 myFunc (S s)"
                                   << "{"
                                   << "    return vec4(s.a, s.b.x, s.b.y, s.c);"
                                   << "}"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s = S(${COORDS}.x, vec3(0.0), ui_one);"
                                   << "    s.b = ${COORDS}.yzw;"
                                   << "    ${DST} = myFunc(s);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(0, 1, 2); });

    LineStream inoutSrc;
    inoutSrc << "${DECLARATIONS}"
             << ""
             << "struct S {"
             << "    ${PRECISION} vec3 red;"
             << "    ${PRECISION} vec3 blue;"
             << "};"
             << ""
             << "void modify (inout S s)"
             << "{"
             << "    s.red += vec3(0.5, 0.0, 0.0);"
             << "    s.blue += vec3(0.0, 0.0, 0.5);"
             << "}"
             << ""
             << "void main (void)"
             << "{"
             << "    S s;"
             << "    s.red = vec3(0.5, 0.0, 0.0);"
             << "    s.blue = vec3(0.0, 0.0, 0.5);"
             << "    modify(s);"
             << "    ${DST} = vec4(0.0, 0.0, 0.0, 1.0);"
             << "    if (s.red == vec3(1.0, 0.0, 0.0) && s.blue == vec3(0.0, 0.0, 1.0))"
             << "        ${DST} = vec4(1.0, 1.0, 1.0, 1.0);"
             << "    ${ASSIGN_POS}"
             << "}";

    std::map<std::string, std::string> precisionParams;
    precisionParams["PRECISION"] = "lowp";
    LOCAL_STRUCT_CASE_PARAMETERIZED(
        parameter_inout_lowp, "Struct with lowp members as an inout function parameter", 0, inoutSrc,
        { c.color.xyz() = tcu::Vec3(1.0, 1.0, 1.0); }, &precisionParams);

    precisionParams["PRECISION"] = "mediump";
    LOCAL_STRUCT_CASE_PARAMETERIZED(
        parameter_inout_mediump, "Struct with mediump members as an inout function parameter", 0, inoutSrc,
        { c.color.xyz() = tcu::Vec3(1.0, 1.0, 1.0); }, &precisionParams);

    precisionParams["PRECISION"] = "highp";
    LOCAL_STRUCT_CASE_PARAMETERIZED(
        parameter_inout_highp, "Struct with highp members as an inout function parameter", FLAG_REQUIRES_HIGHP_FRAGMENT,
        inoutSrc, { c.color.xyz() = tcu::Vec3(1.0, 1.0, 1.0); }, &precisionParams);

    LOCAL_STRUCT_CASE(parameter_nested, "Nested struct as a function parameter", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << ""
                                   << "struct T {"
                                   << "    int                a;"
                                   << "    mediump vec2    b;"
                                   << "};"
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    T                b;"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "mediump vec4 myFunc (S s)"
                                   << "{"
                                   << "    return vec4(s.a, s.b.b, s.b.a + s.c);"
                                   << "}"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s = S(${COORDS}.x, T(0, vec2(0.0)), ui_one);"
                                   << "    s.b = T(ui_zero, ${COORDS}.yz);"
                                   << "    ${DST} = myFunc(s);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(0, 1, 2); });

    LineStream outSrc;
    outSrc << "${DECLARATIONS}"
           << ""
           << "struct S {"
           << "    ${PRECISION} vec3 red;"
           << "    ${PRECISION} vec3 blue;"
           << "};"
           << ""
           << "void modify (out S s)"
           << "{"
           << "    s.red = vec3(1.0, 0.0, 0.0);"
           << "    s.blue = vec3(0.0, 0.0, 1.0);"
           << "}"
           << ""
           << "void main (void)"
           << "{"
           << "    S s;"
           << "    modify(s);"
           << "    ${DST} = vec4(0.0, 0.0, 0.0, 1.0);"
           << "    if (s.red == vec3(1.0, 0.0, 0.0) && s.blue == vec3(0.0, 0.0, 1.0))"
           << "        ${DST} = vec4(1.0, 1.0, 1.0, 1.0);"
           << "    ${ASSIGN_POS}"
           << "}";

    precisionParams["PRECISION"] = "lowp";
    LOCAL_STRUCT_CASE_PARAMETERIZED(
        parameter_out_lowp, "Struct with lowp members as an out function parameter", 0, outSrc,
        { c.color.xyz() = tcu::Vec3(1.0, 1.0, 1.0); }, &precisionParams);

    precisionParams["PRECISION"] = "mediump";
    LOCAL_STRUCT_CASE_PARAMETERIZED(
        parameter_out_mediump, "Struct with mediump members as an out function parameter", 0, outSrc,
        { c.color.xyz() = tcu::Vec3(1.0, 1.0, 1.0); }, &precisionParams);

    precisionParams["PRECISION"] = "highp";
    LOCAL_STRUCT_CASE_PARAMETERIZED(
        parameter_out_highp, "Struct with highp members as an out function parameter", FLAG_REQUIRES_HIGHP_FRAGMENT,
        outSrc, { c.color.xyz() = tcu::Vec3(1.0, 1.0, 1.0); }, &precisionParams);

    LOCAL_STRUCT_CASE(return, "Struct as a return value", 0,
                            LineStream() << "${DECLARATIONS}"
                                         << "uniform int ui_one;"
                                         << ""
                                         << "struct S {"
                                         << "    mediump float    a;"
                                         << "    mediump vec3    b;"
                                         << "    int                c;"
                                         << "};"
                                         << ""
                                         << "S myFunc (void)"
                                         << "{"
                                         << "    S s = S(${COORDS}.x, vec3(0.0), ui_one);"
                                         << "    s.b = ${COORDS}.yzw;"
                                         << "    return s;"
                                         << "}"
                                         << ""
                                         << "void main (void)"
                                         << "{"
                                         << "    S s = myFunc();"
                                         << "    ${DST} = vec4(s.a, s.b.x, s.b.y, s.c);"
                                         << "    ${ASSIGN_POS}"
                                         << "}",
                            { c.color.xyz() = c.coords.swizzle(0, 1, 2); });

    LOCAL_STRUCT_CASE(return_nested, "Nested struct", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << ""
                                   << "struct T {"
                                   << "    int                a;"
                                   << "    mediump vec2    b;"
                                   << "};"
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    T                b;"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "S myFunc (void)"
                                   << "{"
                                   << "    S s = S(${COORDS}.x, T(0, vec2(0.0)), ui_one);"
                                   << "    s.b = T(ui_zero, ${COORDS}.yz);"
                                   << "    return s;"
                                   << "}"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s = myFunc();"
                                   << "    ${DST} = vec4(s.a, s.b.b, s.b.a + s.c);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(0, 1, 2); });

    LOCAL_STRUCT_CASE(conditional_assignment, "Conditional struct assignment", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << "uniform mediump float uf_one;"
                                   << ""
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    mediump vec3    b;"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s = S(${COORDS}.x, ${COORDS}.yzw, ui_zero);"
                                   << "    if (uf_one > 0.0)"
                                   << "        s = S(${COORDS}.w, ${COORDS}.zyx, ui_one);"
                                   << "    ${DST} = vec4(s.a, s.b.xy, s.c);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(3, 2, 1); });

    LOCAL_STRUCT_CASE(loop_assignment, "Struct assignment in loop", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << ""
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    mediump vec3    b;"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s = S(${COORDS}.x, ${COORDS}.yzw, ui_zero);"
                                   << "    for (int i = 0; i < 3; i++)"
                                   << "    {"
                                   << "        if (i == 1)"
                                   << "            s = S(${COORDS}.w, ${COORDS}.zyx, ui_one);"
                                   << "    }"
                                   << "    ${DST} = vec4(s.a, s.b.xy, s.c);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(3, 2, 1); });

    LOCAL_STRUCT_CASE(dynamic_loop_assignment, "Struct assignment in loop", FLAG_REQUIRES_DYNAMIC_INDEXING,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << "uniform int ui_three;"
                                   << ""
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    mediump vec3    b;"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s = S(${COORDS}.x, ${COORDS}.yzw, ui_zero);"
                                   << "    for (int i = 0; i < ui_three; i++)"
                                   << "    {"
                                   << "        if (i == ui_one)"
                                   << "            s = S(${COORDS}.w, ${COORDS}.zyx, ui_one);"
                                   << "    }"
                                   << "    ${DST} = vec4(s.a, s.b.xy, s.c);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(3, 2, 1); });

    LOCAL_STRUCT_CASE(nested_conditional_assignment, "Conditional assignment of nested struct", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << "uniform mediump float uf_one;"
                                   << ""
                                   << "struct T {"
                                   << "    int                a;"
                                   << "    mediump vec2    b;"
                                   << "};"
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    T                b;"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s = S(${COORDS}.x, T(ui_one, ${COORDS}.yz), ui_one);"
                                   << "    if (uf_one > 0.0)"
                                   << "        s.b = T(ui_zero, ${COORDS}.zw);"
                                   << "    ${DST} = vec4(s.a, s.b.b, s.c - s.b.a);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(0, 2, 3); });

    LOCAL_STRUCT_CASE(nested_loop_assignment, "Nested struct assignment in loop", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << "uniform mediump float uf_one;"
                                   << ""
                                   << "struct T {"
                                   << "    int                a;"
                                   << "    mediump vec2    b;"
                                   << "};"
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    T                b;"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s = S(${COORDS}.x, T(ui_one, ${COORDS}.yz), ui_one);"
                                   << "    for (int i = 0; i < 3; i++)"
                                   << "    {"
                                   << "        if (i == 1)"
                                   << "            s.b = T(ui_zero, ${COORDS}.zw);"
                                   << "    }"
                                   << "    ${DST} = vec4(s.a, s.b.b, s.c - s.b.a);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(0, 2, 3); });

    LOCAL_STRUCT_CASE(nested_dynamic_loop_assignment, "Nested struct assignment in dynamic loop",
                      FLAG_REQUIRES_DYNAMIC_INDEXING,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << "uniform int ui_three;"
                                   << "uniform mediump float uf_one;"
                                   << ""
                                   << "struct T {"
                                   << "    int                a;"
                                   << "    mediump vec2    b;"
                                   << "};"
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    T                b;"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s = S(${COORDS}.x, T(ui_one, ${COORDS}.yz), ui_one);"
                                   << "    for (int i = 0; i < ui_three; i++)"
                                   << "    {"
                                   << "        if (i == ui_one)"
                                   << "            s.b = T(ui_zero, ${COORDS}.zw);"
                                   << "    }"
                                   << "    ${DST} = vec4(s.a, s.b.b, s.c - s.b.a);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(0, 2, 3); });

    LOCAL_STRUCT_CASE(loop_struct_array, "Struct array usage in loop", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << "uniform int ui_two;"
                                   << ""
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    mediump int        b;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s[3];"
                                   << "    s[0] = S(${COORDS}.x, ui_zero);"
                                   << "    s[1].a = ${COORDS}.y;"
                                   << "    s[1].b = -ui_one;"
                                   << "    s[2] = S(${COORDS}.z, ui_two);"
                                   << ""
                                   << "    mediump float rgb[3];"
                                   << "    int alpha = 0;"
                                   << "    for (int i = 0; i < 3; i++)"
                                   << "    {"
                                   << "        rgb[i] = s[2-i].a;"
                                   << "        alpha += s[i].b;"
                                   << "    }"
                                   << "    ${DST} = vec4(rgb[0], rgb[1], rgb[2], alpha);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(2, 1, 0); });

    LOCAL_STRUCT_CASE(loop_nested_struct_array, "Nested struct array usage in loop", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << "uniform int ui_two;"
                                   << "uniform mediump float uf_two;"
                                   << "uniform mediump float uf_three;"
                                   << "uniform mediump float uf_four;"
                                   << "uniform mediump float uf_half;"
                                   << "uniform mediump float uf_third;"
                                   << "uniform mediump float uf_fourth;"
                                   << "uniform mediump float uf_sixth;"
                                   << ""
                                   << "struct T {"
                                   << "    mediump float    a;"
                                   << "    mediump vec2    b[2];"
                                   << "};"
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    T                b[3];"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s[2];"
                                   << ""
                                   << "    // S[0]"
                                   << "    s[0].a         = ${COORDS}.x;"
                                   << "    s[0].b[0].a    = uf_half;"
                                   << "    s[0].b[0].b[0] = ${COORDS}.yx;"
                                   << "    s[0].b[0].b[1] = ${COORDS}.zx;"
                                   << "    s[0].b[1].a    = uf_third;"
                                   << "    s[0].b[1].b[0] = ${COORDS}.yy;"
                                   << "    s[0].b[1].b[1] = ${COORDS}.wy;"
                                   << "    s[0].b[2].a    = uf_fourth;"
                                   << "    s[0].b[2].b[0] = ${COORDS}.zx;"
                                   << "    s[0].b[2].b[1] = ${COORDS}.zy;"
                                   << "    s[0].c         = ui_zero;"
                                   << ""
                                   << "    // S[1]"
                                   << "    s[1].a         = ${COORDS}.w;"
                                   << "    s[1].b[0].a    = uf_two;"
                                   << "    s[1].b[0].b[0] = ${COORDS}.zx;"
                                   << "    s[1].b[0].b[1] = ${COORDS}.zy;"
                                   << "    s[1].b[1].a    = uf_three;"
                                   << "    s[1].b[1].b[0] = ${COORDS}.zz;"
                                   << "    s[1].b[1].b[1] = ${COORDS}.ww;"
                                   << "    s[1].b[2].a    = uf_four;"
                                   << "    s[1].b[2].b[0] = ${COORDS}.yx;"
                                   << "    s[1].b[2].b[1] = ${COORDS}.wz;"
                                   << "    s[1].c         = ui_one;"
                                   << ""
                                   << "    mediump float r = 0.0; // (x*3 + y*3) / 6.0"
                                   << "    mediump float g = 0.0; // (y*3 + z*3) / 6.0"
                                   << "    mediump float b = 0.0; // (z*3 + w*3) / 6.0"
                                   << "    mediump float a = 1.0;"
                                   << "    for (int i = 0; i < 2; i++)"
                                   << "    {"
                                   << "        for (int j = 0; j < 3; j++)"
                                   << "        {"
                                   << "            r += s[0].b[j].b[i].y;"
                                   << "            g += s[i].b[j].b[0].x;"
                                   << "            b += s[i].b[j].b[1].x;"
                                   << "            a *= s[i].b[j].a;"
                                   << "        }"
                                   << "    }"
                                   << "    ${DST} = vec4(r*uf_sixth, g*uf_sixth, b*uf_sixth, a);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = (c.coords.swizzle(0, 1, 2) + c.coords.swizzle(1, 2, 3)) * 0.5f; });

    LOCAL_STRUCT_CASE(dynamic_loop_struct_array, "Struct array usage in dynamic loop",
                      FLAG_REQUIRES_DYNAMIC_INDEXING | FLAG_REQUIRES_DYNAMIC_LOOPS,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << "uniform int ui_two;"
                                   << "uniform int ui_three;"
                                   << ""
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    mediump int        b;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s[3];"
                                   << "    s[0] = S(${COORDS}.x, ui_zero);"
                                   << "    s[1].a = ${COORDS}.y;"
                                   << "    s[1].b = -ui_one;"
                                   << "    s[2] = S(${COORDS}.z, ui_two);"
                                   << ""
                                   << "    mediump float rgb[3];"
                                   << "    int alpha = 0;"
                                   << "    for (int i = 0; i < ui_three; i++)"
                                   << "    {"
                                   << "        rgb[i] = s[2-i].a;"
                                   << "        alpha += s[i].b;"
                                   << "    }"
                                   << "    ${DST} = vec4(rgb[0], rgb[1], rgb[2], alpha);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = c.coords.swizzle(2, 1, 0); });

    LOCAL_STRUCT_CASE(dynamic_loop_nested_struct_array, "Nested struct array usage in dynamic loop",
                      FLAG_REQUIRES_DYNAMIC_INDEXING | FLAG_REQUIRES_DYNAMIC_LOOPS,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_zero;"
                                   << "uniform int ui_one;"
                                   << "uniform int ui_two;"
                                   << "uniform int ui_three;"
                                   << "uniform mediump float uf_two;"
                                   << "uniform mediump float uf_three;"
                                   << "uniform mediump float uf_four;"
                                   << "uniform mediump float uf_half;"
                                   << "uniform mediump float uf_third;"
                                   << "uniform mediump float uf_fourth;"
                                   << "uniform mediump float uf_sixth;"
                                   << ""
                                   << "struct T {"
                                   << "    mediump float    a;"
                                   << "    mediump vec2    b[2];"
                                   << "};"
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    T                b[3];"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S s[2];"
                                   << ""
                                   << "    // S[0]"
                                   << "    s[0].a         = ${COORDS}.x;"
                                   << "    s[0].b[0].a    = uf_half;"
                                   << "    s[0].b[0].b[0] = ${COORDS}.yx;"
                                   << "    s[0].b[0].b[1] = ${COORDS}.zx;"
                                   << "    s[0].b[1].a    = uf_third;"
                                   << "    s[0].b[1].b[0] = ${COORDS}.yy;"
                                   << "    s[0].b[1].b[1] = ${COORDS}.wy;"
                                   << "    s[0].b[2].a    = uf_fourth;"
                                   << "    s[0].b[2].b[0] = ${COORDS}.zx;"
                                   << "    s[0].b[2].b[1] = ${COORDS}.zy;"
                                   << "    s[0].c         = ui_zero;"
                                   << ""
                                   << "    // S[1]"
                                   << "    s[1].a         = ${COORDS}.w;"
                                   << "    s[1].b[0].a    = uf_two;"
                                   << "    s[1].b[0].b[0] = ${COORDS}.zx;"
                                   << "    s[1].b[0].b[1] = ${COORDS}.zy;"
                                   << "    s[1].b[1].a    = uf_three;"
                                   << "    s[1].b[1].b[0] = ${COORDS}.zz;"
                                   << "    s[1].b[1].b[1] = ${COORDS}.ww;"
                                   << "    s[1].b[2].a    = uf_four;"
                                   << "    s[1].b[2].b[0] = ${COORDS}.yx;"
                                   << "    s[1].b[2].b[1] = ${COORDS}.wz;"
                                   << "    s[1].c         = ui_one;"
                                   << ""
                                   << "    mediump float r = 0.0; // (x*3 + y*3) / 6.0"
                                   << "    mediump float g = 0.0; // (y*3 + z*3) / 6.0"
                                   << "    mediump float b = 0.0; // (z*3 + w*3) / 6.0"
                                   << "    mediump float a = 1.0;"
                                   << "    for (int i = 0; i < ui_two; i++)"
                                   << "    {"
                                   << "        for (int j = 0; j < ui_three; j++)"
                                   << "        {"
                                   << "            r += s[0].b[j].b[i].y;"
                                   << "            g += s[i].b[j].b[0].x;"
                                   << "            b += s[i].b[j].b[1].x;"
                                   << "            a *= s[i].b[j].a;"
                                   << "        }"
                                   << "    }"
                                   << "    ${DST} = vec4(r*uf_sixth, g*uf_sixth, b*uf_sixth, a);"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      { c.color.xyz() = (c.coords.swizzle(0, 1, 2) + c.coords.swizzle(1, 2, 3)) * 0.5f; });

    LOCAL_STRUCT_CASE(basic_equal, "Basic struct equality", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_one;"
                                   << "uniform int ui_two;"
                                   << ""
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    mediump vec3    b;"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S a = S(floor(${COORDS}.x), vec3(0.0, floor(${COORDS}.y), 2.3), ui_one);"
                                   << "    S b = S(floor(${COORDS}.x+0.5), vec3(0.0, floor(${COORDS}.y), 2.3), ui_one);"
                                   << "    S c = S(floor(${COORDS}.x), vec3(0.0, floor(${COORDS}.y+0.5), 2.3), ui_one);"
                                   << "    S d = S(floor(${COORDS}.x), vec3(0.0, floor(${COORDS}.y), 2.3), ui_two);"
                                   << "    ${DST} = vec4(0.0, 0.0, 0.0, 1.0);"
                                   << "    if (a == b) ${DST}.x = 1.0;"
                                   << "    if (a == c) ${DST}.y = 1.0;"
                                   << "    if (a == d) ${DST}.z = 1.0;"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      {
                          if (deFloatFloor(c.coords[0]) == deFloatFloor(c.coords[0] + 0.5f))
                              c.color.x() = 1.0f;
                          if (deFloatFloor(c.coords[1]) == deFloatFloor(c.coords[1] + 0.5f))
                              c.color.y() = 1.0f;
                      });

    LOCAL_STRUCT_CASE(basic_not_equal, "Basic struct equality", 0,
                      LineStream() << "${DECLARATIONS}"
                                   << "uniform int ui_one;"
                                   << "uniform int ui_two;"
                                   << ""
                                   << "struct S {"
                                   << "    mediump float    a;"
                                   << "    mediump vec3    b;"
                                   << "    int                c;"
                                   << "};"
                                   << ""
                                   << "void main (void)"
                                   << "{"
                                   << "    S a = S(floor(${COORDS}.x), vec3(0.0, floor(${COORDS}.y), 2.3), ui_one);"
                                   << "    S b = S(floor(${COORDS}.x+0.5), vec3(0.0, floor(${COORDS}.y), 2.3), ui_one);"
                                   << "    S c = S(floor(${COORDS}.x), vec3(0.0, floor(${COORDS}.y+0.5), 2.3), ui_one);"
                                   << "    S d = S(floor(${COORDS}.x), vec3(0.0, floor(${COORDS}.y), 2.3), ui_two);"
                                   << "    ${DST} = vec4(0.0, 0.0, 0.0, 1.0);"
                                   << "    if (a != b) ${DST}.x = 1.0;"
                                   << "    if (a != c) ${DST}.y = 1.0;"
                                   << "    if (a != d) ${DST}.z = 1.0;"
                                   << "    ${ASSIGN_POS}"
                                   << "}",
                      {
                          if (deFloatFloor(c.coords[0]) != deFloatFloor(c.coords[0] + 0.5f))
                              c.color.x() = 1.0f;
                          if (deFloatFloor(c.coords[1]) != deFloatFloor(c.coords[1] + 0.5f))
                              c.color.y() = 1.0f;
                          c.color.z() = 1.0f;
                      });

    LOCAL_STRUCT_CASE(
        nested_equal, "Nested struct struct equality", 0,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_one;"
                     << "uniform int ui_two;"
                     << ""
                     << "struct T {"
                     << "    mediump vec3    a;"
                     << "    int                b;"
                     << "};"
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    T                b;"
                     << "    int                c;"
                     << "};"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    S a = S(floor(${COORDS}.x), T(vec3(0.0, floor(${COORDS}.y), 2.3), ui_one), 1);"
                     << "    S b = S(floor(${COORDS}.x+0.5), T(vec3(0.0, floor(${COORDS}.y), 2.3), ui_one), 1);"
                     << "    S c = S(floor(${COORDS}.x), T(vec3(0.0, floor(${COORDS}.y+0.5), 2.3), ui_one), 1);"
                     << "    S d = S(floor(${COORDS}.x), T(vec3(0.0, floor(${COORDS}.y), 2.3), ui_two), 1);"
                     << "    ${DST} = vec4(0.0, 0.0, 0.0, 1.0);"
                     << "    if (a == b) ${DST}.x = 1.0;"
                     << "    if (a == c) ${DST}.y = 1.0;"
                     << "    if (a == d) ${DST}.z = 1.0;"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            if (deFloatFloor(c.coords[0]) == deFloatFloor(c.coords[0] + 0.5f))
                c.color.x() = 1.0f;
            if (deFloatFloor(c.coords[1]) == deFloatFloor(c.coords[1] + 0.5f))
                c.color.y() = 1.0f;
        });

    LOCAL_STRUCT_CASE(
        nested_not_equal, "Nested struct struct equality", 0,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_one;"
                     << "uniform int ui_two;"
                     << ""
                     << "struct T {"
                     << "    mediump vec3    a;"
                     << "    int                b;"
                     << "};"
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    T                b;"
                     << "    int                c;"
                     << "};"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    S a = S(floor(${COORDS}.x), T(vec3(0.0, floor(${COORDS}.y), 2.3), ui_one), 1);"
                     << "    S b = S(floor(${COORDS}.x+0.5), T(vec3(0.0, floor(${COORDS}.y), 2.3), ui_one), 1);"
                     << "    S c = S(floor(${COORDS}.x), T(vec3(0.0, floor(${COORDS}.y+0.5), 2.3), ui_one), 1);"
                     << "    S d = S(floor(${COORDS}.x), T(vec3(0.0, floor(${COORDS}.y), 2.3), ui_two), 1);"
                     << "    ${DST} = vec4(0.0, 0.0, 0.0, 1.0);"
                     << "    if (a != b) ${DST}.x = 1.0;"
                     << "    if (a != c) ${DST}.y = 1.0;"
                     << "    if (a != d) ${DST}.z = 1.0;"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            if (deFloatFloor(c.coords[0]) != deFloatFloor(c.coords[0] + 0.5f))
                c.color.x() = 1.0f;
            if (deFloatFloor(c.coords[1]) != deFloatFloor(c.coords[1] + 0.5f))
                c.color.y() = 1.0f;
            c.color.z() = 1.0f;
        });
}

class UniformStructTests : public TestCaseGroup
{
public:
    UniformStructTests(Context &context) : TestCaseGroup(context, "uniform", "Uniform structs")
    {
    }

    ~UniformStructTests(void)
    {
    }

    virtual void init(void);
};

namespace
{

#define CHECK_SET_UNIFORM(NAME) GLU_EXPECT_NO_ERROR(gl.getError(), (string("Failed to set ") + (NAME)).c_str())

#define MAKE_SET_VEC_UNIFORM(VECTYPE, SETUNIFORM)                                                            \
    void setUniform(const glw::Functions &gl, uint32_t programID, const char *name, const tcu::VECTYPE &vec) \
    {                                                                                                        \
        int loc = gl.getUniformLocation(programID, name);                                                    \
        SETUNIFORM(loc, 1, vec.getPtr());                                                                    \
        CHECK_SET_UNIFORM(name);                                                                             \
    }                                                                                                        \
    struct SetUniform##VECTYPE##Unused_s                                                                     \
    {                                                                                                        \
        int unused;                                                                                          \
    }

#define MAKE_SET_VEC_UNIFORM_PTR(VECTYPE, SETUNIFORM)                                                        \
    void setUniform(const glw::Functions &gl, uint32_t programID, const char *name, const tcu::VECTYPE *vec, \
                    int arraySize)                                                                           \
    {                                                                                                        \
        int loc = gl.getUniformLocation(programID, name);                                                    \
        SETUNIFORM(loc, arraySize, vec->getPtr());                                                           \
        CHECK_SET_UNIFORM(name);                                                                             \
    }                                                                                                        \
    struct SetUniformPtr##VECTYPE##Unused_s                                                                  \
    {                                                                                                        \
        int unused;                                                                                          \
    }

MAKE_SET_VEC_UNIFORM(Vec2, gl.uniform2fv);
MAKE_SET_VEC_UNIFORM(Vec3, gl.uniform3fv);
MAKE_SET_VEC_UNIFORM_PTR(Vec2, gl.uniform2fv);

void setUniform(const glw::Functions &gl, uint32_t programID, const char *name, float value)
{
    int loc = gl.getUniformLocation(programID, name);
    gl.uniform1f(loc, value);
    CHECK_SET_UNIFORM(name);
}

void setUniform(const glw::Functions &gl, uint32_t programID, const char *name, int value)
{
    int loc = gl.getUniformLocation(programID, name);
    gl.uniform1i(loc, value);
    CHECK_SET_UNIFORM(name);
}

void setUniform(const glw::Functions &gl, uint32_t programID, const char *name, const float *value, int arraySize)
{
    int loc = gl.getUniformLocation(programID, name);
    gl.uniform1fv(loc, arraySize, value);
    CHECK_SET_UNIFORM(name);
}

} // namespace

void UniformStructTests::init(void)
{
#define UNIFORM_STRUCT_CASE(NAME, DESCRIPTION, FLAGS, SHADER_SRC, SET_UNIFORMS_BODY, EVAL_FUNC_BODY)                \
    do                                                                                                              \
    {                                                                                                               \
        struct SetUniforms_##NAME                                                                                   \
        {                                                                                                           \
            static void setUniforms(const glw::Functions &gl, uint32_t programID,                                   \
                                    const tcu::Vec4 &constCoords) SET_UNIFORMS_BODY /* NOLINT(SET_UNIFORMS_BODY) */ \
        };                                                                                                          \
        struct Eval_##NAME                                                                                          \
        {                                                                                                           \
            static void eval(ShaderEvalContext &c) EVAL_FUNC_BODY                                                   \
        }; /* NOLINT(EVAL_FUNC_BODY) */                                                                             \
        addChild(createStructCase(m_context, #NAME "_vertex", DESCRIPTION, true, FLAGS, Eval_##NAME::eval,          \
                                  SetUniforms_##NAME::setUniforms, SHADER_SRC, nullptr));                           \
        addChild(createStructCase(m_context, #NAME "_fragment", DESCRIPTION, false, FLAGS, Eval_##NAME::eval,       \
                                  SetUniforms_##NAME::setUniforms, SHADER_SRC, nullptr));                           \
    } while (false)

    UNIFORM_STRUCT_CASE(
        basic, "Basic struct usage", 0,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_one;"
                     << ""
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    mediump vec3    b;"
                     << "    int                c;"
                     << "};"
                     << "uniform S s;"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    ${DST} = vec4(s.a, s.b.x, s.b.y, s.c);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            setUniform(gl, programID, "s.a", constCoords.x());
            setUniform(gl, programID, "s.b", constCoords.swizzle(1, 2, 3));
            setUniform(gl, programID, "s.c", 1);
        },
        { c.color.xyz() = c.constCoords.swizzle(0, 1, 2); });

    UNIFORM_STRUCT_CASE(
        nested, "Nested struct", 0,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_zero;"
                     << "uniform int ui_one;"
                     << ""
                     << "struct T {"
                     << "    int                a;"
                     << "    mediump vec2    b;"
                     << "};"
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    T                b;"
                     << "    int                c;"
                     << "};"
                     << "uniform S s;"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    ${DST} = vec4(s.a, s.b.b, s.b.a + s.c);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            setUniform(gl, programID, "s.a", constCoords.x());
            setUniform(gl, programID, "s.b.a", 0);
            setUniform(gl, programID, "s.b.b", constCoords.swizzle(1, 2));
            setUniform(gl, programID, "s.c", 1);
        },
        { c.color.xyz() = c.constCoords.swizzle(0, 1, 2); });

    UNIFORM_STRUCT_CASE(
        array_member, "Struct with array member", 0,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_one;"
                     << ""
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    mediump float    b[3];"
                     << "    int                c;"
                     << "};"
                     << "uniform S s;"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    ${DST} = vec4(s.a, s.b[0], s.b[1], s.c);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            setUniform(gl, programID, "s.a", constCoords.w());
            setUniform(gl, programID, "s.c", 1);

            float b[3];
            b[0] = constCoords.z();
            b[1] = constCoords.y();
            b[2] = constCoords.x();
            setUniform(gl, programID, "s.b", b, DE_LENGTH_OF_ARRAY(b));
        },
        { c.color.xyz() = c.constCoords.swizzle(3, 2, 1); });

    UNIFORM_STRUCT_CASE(
        array_member_dynamic_index, "Struct with array member, dynamic indexing", FLAG_REQUIRES_DYNAMIC_INDEXING,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_zero;"
                     << "uniform int ui_one;"
                     << "uniform int ui_two;"
                     << ""
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    mediump float    b[3];"
                     << "    int                c;"
                     << "};"
                     << "uniform S s;"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    ${DST} = vec4(s.b[ui_one], s.b[ui_zero], s.b[ui_two], s.c);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            setUniform(gl, programID, "s.a", constCoords.w());
            setUniform(gl, programID, "s.c", 1);

            float b[3];
            b[0] = constCoords.z();
            b[1] = constCoords.y();
            b[2] = constCoords.x();
            setUniform(gl, programID, "s.b", b, DE_LENGTH_OF_ARRAY(b));
        },
        { c.color.xyz() = c.constCoords.swizzle(1, 2, 0); });

    UNIFORM_STRUCT_CASE(
        struct_array, "Struct array", 0,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_zero;"
                     << "uniform int ui_one;"
                     << "uniform int ui_two;"
                     << ""
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    mediump int        b;"
                     << "};"
                     << "uniform S s[3];"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    ${DST} = vec4(s[2].a, s[1].a, s[0].a, s[2].b - s[1].b + s[0].b);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            setUniform(gl, programID, "s[0].a", constCoords.x());
            setUniform(gl, programID, "s[0].b", 0);
            setUniform(gl, programID, "s[1].a", constCoords.y());
            setUniform(gl, programID, "s[1].b", 1);
            setUniform(gl, programID, "s[2].a", constCoords.z());
            setUniform(gl, programID, "s[2].b", 2);
        },
        { c.color.xyz() = c.constCoords.swizzle(2, 1, 0); });

    UNIFORM_STRUCT_CASE(
        struct_array_dynamic_index, "Struct array with dynamic indexing", FLAG_REQUIRES_DYNAMIC_INDEXING,
        LineStream()
            << "${DECLARATIONS}"
            << "uniform int ui_zero;"
            << "uniform int ui_one;"
            << "uniform int ui_two;"
            << ""
            << "struct S {"
            << "    mediump float    a;"
            << "    mediump int        b;"
            << "};"
            << "uniform S s[3];"
            << ""
            << "void main (void)"
            << "{"
            << "    ${DST} = vec4(s[ui_two].a, s[ui_one].a, s[ui_zero].a, s[ui_two].b - s[ui_one].b + s[ui_zero].b);"
            << "    ${ASSIGN_POS}"
            << "}",
        {
            setUniform(gl, programID, "s[0].a", constCoords.x());
            setUniform(gl, programID, "s[0].b", 0);
            setUniform(gl, programID, "s[1].a", constCoords.y());
            setUniform(gl, programID, "s[1].b", 1);
            setUniform(gl, programID, "s[2].a", constCoords.z());
            setUniform(gl, programID, "s[2].b", 2);
        },
        { c.color.xyz() = c.constCoords.swizzle(2, 1, 0); });

    UNIFORM_STRUCT_CASE(
        nested_struct_array, "Nested struct array", 0,
        LineStream() << "${DECLARATIONS}"
                     << "struct T {"
                     << "    mediump float    a;"
                     << "    mediump vec2    b[2];"
                     << "};"
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    T                b[3];"
                     << "    int                c;"
                     << "};"
                     << "uniform S s[2];"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    mediump float r = (s[0].b[1].b[0].x + s[1].b[2].b[1].y) * s[0].b[0].a; // (z + z) * 0.5"
                     << "    mediump float g = s[1].b[0].b[0].y * s[0].b[2].a * s[1].b[2].a; // x * 0.25 * 4"
                     << "    mediump float b = (s[0].b[2].b[1].y + s[0].b[1].b[0].y + s[1].a) * s[0].b[1].a; // (w + w "
                        "+ w) * 0.333"
                     << "    mediump float a = float(s[0].c) + s[1].b[2].a - s[1].b[1].a; // 0 + 4.0 - 3.0"
                     << "    ${DST} = vec4(r, g, b, a);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            tcu::Vec2 arr[2];

            setUniform(gl, programID, "s[0].a", constCoords.x());
            arr[0] = constCoords.swizzle(0, 1);
            arr[1] = constCoords.swizzle(2, 3);
            setUniform(gl, programID, "s[0].b[0].a", 0.5f);
            setUniform(gl, programID, "s[0].b[0].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(2, 3);
            arr[1] = constCoords.swizzle(0, 1);
            setUniform(gl, programID, "s[0].b[1].a", 1.0f / 3.0f);
            setUniform(gl, programID, "s[0].b[1].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(0, 2);
            arr[1] = constCoords.swizzle(1, 3);
            setUniform(gl, programID, "s[0].b[2].a", 1.0f / 4.0f);
            setUniform(gl, programID, "s[0].b[2].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            setUniform(gl, programID, "s[0].c", 0);

            setUniform(gl, programID, "s[1].a", constCoords.w());
            arr[0] = constCoords.swizzle(0, 0);
            arr[1] = constCoords.swizzle(1, 1);
            setUniform(gl, programID, "s[1].b[0].a", 2.0f);
            setUniform(gl, programID, "s[1].b[0].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(2, 2);
            arr[1] = constCoords.swizzle(3, 3);
            setUniform(gl, programID, "s[1].b[1].a", 3.0f);
            setUniform(gl, programID, "s[1].b[1].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(1, 0);
            arr[1] = constCoords.swizzle(3, 2);
            setUniform(gl, programID, "s[1].b[2].a", 4.0f);
            setUniform(gl, programID, "s[1].b[2].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            setUniform(gl, programID, "s[1].c", 1);
        },
        { c.color.xyz() = c.constCoords.swizzle(2, 0, 3); });

    UNIFORM_STRUCT_CASE(
        nested_struct_array_dynamic_index, "Nested struct array with dynamic indexing", FLAG_REQUIRES_DYNAMIC_INDEXING,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_zero;"
                     << "uniform int ui_one;"
                     << "uniform int ui_two;"
                     << ""
                     << "struct T {"
                     << "    mediump float    a;"
                     << "    mediump vec2    b[2];"
                     << "};"
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    T                b[3];"
                     << "    int                c;"
                     << "};"
                     << "uniform S s[2];"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    mediump float r = (s[0].b[ui_one].b[ui_one-1].x + s[ui_one].b[ui_two].b[ui_zero+1].y) * "
                        "s[0].b[0].a; // (z + z) * 0.5"
                     << "    mediump float g = s[ui_two-1].b[ui_two-2].b[ui_zero].y * s[0].b[ui_two].a * "
                        "s[ui_one].b[2].a; // x * 0.25 * 4"
                     << "    mediump float b = (s[ui_zero].b[ui_one+1].b[1].y + s[0].b[ui_one*ui_one].b[0].y + "
                        "s[ui_one].a) * s[0].b[ui_two-ui_one].a; // (w + w + w) * 0.333"
                     << "    mediump float a = float(s[ui_zero].c) + s[ui_one-ui_zero].b[ui_two].a - "
                        "s[ui_zero+ui_one].b[ui_two-ui_one].a; // 0 + 4.0 - 3.0"
                     << "    ${DST} = vec4(r, g, b, a);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            tcu::Vec2 arr[2];

            setUniform(gl, programID, "s[0].a", constCoords.x());
            arr[0] = constCoords.swizzle(0, 1);
            arr[1] = constCoords.swizzle(2, 3);
            setUniform(gl, programID, "s[0].b[0].a", 0.5f);
            setUniform(gl, programID, "s[0].b[0].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(2, 3);
            arr[1] = constCoords.swizzle(0, 1);
            setUniform(gl, programID, "s[0].b[1].a", 1.0f / 3.0f);
            setUniform(gl, programID, "s[0].b[1].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(0, 2);
            arr[1] = constCoords.swizzle(1, 3);
            setUniform(gl, programID, "s[0].b[2].a", 1.0f / 4.0f);
            setUniform(gl, programID, "s[0].b[2].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            setUniform(gl, programID, "s[0].c", 0);

            setUniform(gl, programID, "s[1].a", constCoords.w());
            arr[0] = constCoords.swizzle(0, 0);
            arr[1] = constCoords.swizzle(1, 1);
            setUniform(gl, programID, "s[1].b[0].a", 2.0f);
            setUniform(gl, programID, "s[1].b[0].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(2, 2);
            arr[1] = constCoords.swizzle(3, 3);
            setUniform(gl, programID, "s[1].b[1].a", 3.0f);
            setUniform(gl, programID, "s[1].b[1].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(1, 0);
            arr[1] = constCoords.swizzle(3, 2);
            setUniform(gl, programID, "s[1].b[2].a", 4.0f);
            setUniform(gl, programID, "s[1].b[2].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            setUniform(gl, programID, "s[1].c", 1);
        },
        { c.color.xyz() = c.constCoords.swizzle(2, 0, 3); });

    UNIFORM_STRUCT_CASE(
        loop_struct_array, "Struct array usage in loop", 0,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_zero;"
                     << "uniform int ui_one;"
                     << "uniform int ui_two;"
                     << ""
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    mediump int        b;"
                     << "};"
                     << "uniform S s[3];"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    mediump float rgb[3];"
                     << "    int alpha = 0;"
                     << "    for (int i = 0; i < 3; i++)"
                     << "    {"
                     << "        rgb[i] = s[2-i].a;"
                     << "        alpha += s[i].b;"
                     << "    }"
                     << "    ${DST} = vec4(rgb[0], rgb[1], rgb[2], alpha);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            setUniform(gl, programID, "s[0].a", constCoords.x());
            setUniform(gl, programID, "s[0].b", 0);
            setUniform(gl, programID, "s[1].a", constCoords.y());
            setUniform(gl, programID, "s[1].b", -1);
            setUniform(gl, programID, "s[2].a", constCoords.z());
            setUniform(gl, programID, "s[2].b", 2);
        },
        { c.color.xyz() = c.constCoords.swizzle(2, 1, 0); });

    UNIFORM_STRUCT_CASE(
        loop_nested_struct_array, "Nested struct array usage in loop", 0,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_zero;"
                     << "uniform int ui_one;"
                     << "uniform int ui_two;"
                     << "uniform mediump float uf_two;"
                     << "uniform mediump float uf_three;"
                     << "uniform mediump float uf_four;"
                     << "uniform mediump float uf_half;"
                     << "uniform mediump float uf_third;"
                     << "uniform mediump float uf_fourth;"
                     << "uniform mediump float uf_sixth;"
                     << ""
                     << "struct T {"
                     << "    mediump float    a;"
                     << "    mediump vec2    b[2];"
                     << "};"
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    T                b[3];"
                     << "    int                c;"
                     << "};"
                     << "uniform S s[2];"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    mediump float r = 0.0; // (x*3 + y*3) / 6.0"
                     << "    mediump float g = 0.0; // (y*3 + z*3) / 6.0"
                     << "    mediump float b = 0.0; // (z*3 + w*3) / 6.0"
                     << "    mediump float a = 1.0;"
                     << "    for (int i = 0; i < 2; i++)"
                     << "    {"
                     << "        for (int j = 0; j < 3; j++)"
                     << "        {"
                     << "            r += s[0].b[j].b[i].y;"
                     << "            g += s[i].b[j].b[0].x;"
                     << "            b += s[i].b[j].b[1].x;"
                     << "            a *= s[i].b[j].a;"
                     << "        }"
                     << "    }"
                     << "    ${DST} = vec4(r*uf_sixth, g*uf_sixth, b*uf_sixth, a);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            tcu::Vec2 arr[2];

            setUniform(gl, programID, "s[0].a", constCoords.x());
            arr[0] = constCoords.swizzle(1, 0);
            arr[1] = constCoords.swizzle(2, 0);
            setUniform(gl, programID, "s[0].b[0].a", 0.5f);
            setUniform(gl, programID, "s[0].b[0].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(1, 1);
            arr[1] = constCoords.swizzle(3, 1);
            setUniform(gl, programID, "s[0].b[1].a", 1.0f / 3.0f);
            setUniform(gl, programID, "s[0].b[1].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(2, 1);
            arr[1] = constCoords.swizzle(2, 1);
            setUniform(gl, programID, "s[0].b[2].a", 1.0f / 4.0f);
            setUniform(gl, programID, "s[0].b[2].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            setUniform(gl, programID, "s[0].c", 0);

            setUniform(gl, programID, "s[1].a", constCoords.w());
            arr[0] = constCoords.swizzle(2, 0);
            arr[1] = constCoords.swizzle(2, 1);
            setUniform(gl, programID, "s[1].b[0].a", 2.0f);
            setUniform(gl, programID, "s[1].b[0].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(2, 2);
            arr[1] = constCoords.swizzle(3, 3);
            setUniform(gl, programID, "s[1].b[1].a", 3.0f);
            setUniform(gl, programID, "s[1].b[1].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(1, 0);
            arr[1] = constCoords.swizzle(3, 2);
            setUniform(gl, programID, "s[1].b[2].a", 4.0f);
            setUniform(gl, programID, "s[1].b[2].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            setUniform(gl, programID, "s[1].c", 1);
        },
        { c.color.xyz() = (c.constCoords.swizzle(0, 1, 2) + c.constCoords.swizzle(1, 2, 3)) * 0.5f; });

    UNIFORM_STRUCT_CASE(
        dynamic_loop_struct_array, "Struct array usage in dynamic loop",
        FLAG_REQUIRES_DYNAMIC_INDEXING | FLAG_REQUIRES_DYNAMIC_LOOPS,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_zero;"
                     << "uniform int ui_one;"
                     << "uniform int ui_two;"
                     << "uniform int ui_three;"
                     << ""
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    mediump int        b;"
                     << "};"
                     << "uniform S s[3];"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    mediump float rgb[3];"
                     << "    int alpha = 0;"
                     << "    for (int i = 0; i < ui_three; i++)"
                     << "    {"
                     << "        rgb[i] = s[2-i].a;"
                     << "        alpha += s[i].b;"
                     << "    }"
                     << "    ${DST} = vec4(rgb[0], rgb[1], rgb[2], alpha);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            setUniform(gl, programID, "s[0].a", constCoords.x());
            setUniform(gl, programID, "s[0].b", 0);
            setUniform(gl, programID, "s[1].a", constCoords.y());
            setUniform(gl, programID, "s[1].b", -1);
            setUniform(gl, programID, "s[2].a", constCoords.z());
            setUniform(gl, programID, "s[2].b", 2);
        },
        { c.color.xyz() = c.constCoords.swizzle(2, 1, 0); });

    UNIFORM_STRUCT_CASE(
        dynamic_loop_nested_struct_array, "Nested struct array usage in dynamic loop",
        FLAG_REQUIRES_DYNAMIC_INDEXING | FLAG_REQUIRES_DYNAMIC_LOOPS,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_zero;"
                     << "uniform int ui_one;"
                     << "uniform int ui_two;"
                     << "uniform int ui_three;"
                     << "uniform mediump float uf_two;"
                     << "uniform mediump float uf_three;"
                     << "uniform mediump float uf_four;"
                     << "uniform mediump float uf_half;"
                     << "uniform mediump float uf_third;"
                     << "uniform mediump float uf_fourth;"
                     << "uniform mediump float uf_sixth;"
                     << ""
                     << "struct T {"
                     << "    mediump float    a;"
                     << "    mediump vec2    b[2];"
                     << "};"
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    T                b[3];"
                     << "    int                c;"
                     << "};"
                     << "uniform S s[2];"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    mediump float r = 0.0; // (x*3 + y*3) / 6.0"
                     << "    mediump float g = 0.0; // (y*3 + z*3) / 6.0"
                     << "    mediump float b = 0.0; // (z*3 + w*3) / 6.0"
                     << "    mediump float a = 1.0;"
                     << "    for (int i = 0; i < ui_two; i++)"
                     << "    {"
                     << "        for (int j = 0; j < ui_three; j++)"
                     << "        {"
                     << "            r += s[0].b[j].b[i].y;"
                     << "            g += s[i].b[j].b[0].x;"
                     << "            b += s[i].b[j].b[1].x;"
                     << "            a *= s[i].b[j].a;"
                     << "        }"
                     << "    }"
                     << "    ${DST} = vec4(r*uf_sixth, g*uf_sixth, b*uf_sixth, a);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            tcu::Vec2 arr[2];

            setUniform(gl, programID, "s[0].a", constCoords.x());
            arr[0] = constCoords.swizzle(1, 0);
            arr[1] = constCoords.swizzle(2, 0);
            setUniform(gl, programID, "s[0].b[0].a", 0.5f);
            setUniform(gl, programID, "s[0].b[0].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(1, 1);
            arr[1] = constCoords.swizzle(3, 1);
            setUniform(gl, programID, "s[0].b[1].a", 1.0f / 3.0f);
            setUniform(gl, programID, "s[0].b[1].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(2, 1);
            arr[1] = constCoords.swizzle(2, 1);
            setUniform(gl, programID, "s[0].b[2].a", 1.0f / 4.0f);
            setUniform(gl, programID, "s[0].b[2].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            setUniform(gl, programID, "s[0].c", 0);

            setUniform(gl, programID, "s[1].a", constCoords.w());
            arr[0] = constCoords.swizzle(2, 0);
            arr[1] = constCoords.swizzle(2, 1);
            setUniform(gl, programID, "s[1].b[0].a", 2.0f);
            setUniform(gl, programID, "s[1].b[0].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(2, 2);
            arr[1] = constCoords.swizzle(3, 3);
            setUniform(gl, programID, "s[1].b[1].a", 3.0f);
            setUniform(gl, programID, "s[1].b[1].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            arr[0] = constCoords.swizzle(1, 0);
            arr[1] = constCoords.swizzle(3, 2);
            setUniform(gl, programID, "s[1].b[2].a", 4.0f);
            setUniform(gl, programID, "s[1].b[2].b", &arr[0], DE_LENGTH_OF_ARRAY(arr));
            setUniform(gl, programID, "s[1].c", 1);
        },
        { c.color.xyz() = (c.constCoords.swizzle(0, 1, 2) + c.constCoords.swizzle(1, 2, 3)) * 0.5f; });

    UNIFORM_STRUCT_CASE(
        sampler, "Sampler in struct", FLAG_USES_TEXTURES,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_one;"
                     << ""
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    mediump vec3    b;"
                     << "    sampler2D        c;"
                     << "};"
                     << "uniform S s;"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    ${DST} = vec4(texture2D(s.c, ${COORDS}.xy * s.b.xy + s.b.z).rgb, s.a);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            DE_UNREF(constCoords);
            setUniform(gl, programID, "s.a", 1.0f);
            setUniform(gl, programID, "s.b", tcu::Vec3(0.25f, 0.25f, 0.5f));
            setUniform(gl, programID, "s.c", 0);
        },
        { c.color.xyz() = c.texture2D(TEXTURE_BRICK, c.coords.swizzle(0, 1) * 0.25f + 0.5f).swizzle(0, 1, 2); });

    UNIFORM_STRUCT_CASE(
        sampler_nested, "Sampler in nested struct", FLAG_USES_TEXTURES,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_zero;"
                     << "uniform int ui_one;"
                     << ""
                     << "struct T {"
                     << "    sampler2D        a;"
                     << "    mediump vec2    b;"
                     << "};"
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    T                b;"
                     << "    int                c;"
                     << "};"
                     << "uniform S s;"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    ${DST} = vec4(texture2D(s.b.a, ${COORDS}.xy * s.b.b + s.a).rgb, s.c);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            DE_UNREF(constCoords);
            setUniform(gl, programID, "s.a", 0.5f);
            setUniform(gl, programID, "s.b.a", 0);
            setUniform(gl, programID, "s.b.b", tcu::Vec2(0.25f, 0.25f));
            setUniform(gl, programID, "s.c", 1);
        },
        { c.color.xyz() = c.texture2D(TEXTURE_BRICK, c.coords.swizzle(0, 1) * 0.25f + 0.5f).swizzle(0, 1, 2); });

    UNIFORM_STRUCT_CASE(
        sampler_array, "Sampler in struct array", FLAG_USES_TEXTURES,
        LineStream() << "${DECLARATIONS}"
                     << "uniform int ui_one;"
                     << ""
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    mediump vec3    b;"
                     << "    sampler2D        c;"
                     << "};"
                     << "uniform S s[2];"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    ${DST} = vec4(texture2D(s[1].c, ${COORDS}.xy * s[0].b.xy + s[1].b.z).rgb, s[0].a);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            DE_UNREF(constCoords);
            setUniform(gl, programID, "s[0].a", 1.0f);
            setUniform(gl, programID, "s[0].b", tcu::Vec3(0.25f, 0.25f, 0.25f));
            setUniform(gl, programID, "s[0].c", 1);
            setUniform(gl, programID, "s[1].a", 0.0f);
            setUniform(gl, programID, "s[1].b", tcu::Vec3(0.5f, 0.5f, 0.5f));
            setUniform(gl, programID, "s[1].c", 0);
        },
        { c.color.xyz() = c.texture2D(TEXTURE_BRICK, c.coords.swizzle(0, 1) * 0.25f + 0.5f).swizzle(0, 1, 2); });

    UNIFORM_STRUCT_CASE(
        sampler_in_function_arg, "Sampler in struct as function arg", FLAG_USES_TEXTURES,
        LineStream() << "${DECLARATIONS}"
                     << ""
                     << "struct S {"
                     << "    sampler2D        source;"
                     << "};"
                     << ""
                     << "mediump vec4 fun(S s) {"
                     << "    return texture2D(s.source, vec2(0.5));"
                     << "}"
                     << ""
                     << "uniform S s;"
                     << "void main (void)"
                     << "{"
                     << "    ${DST} = fun(s);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            DE_UNREF(constCoords);
            setUniform(gl, programID, "s.source", 0);
        },
        { c.color.xyz() = c.texture2D(TEXTURE_BRICK, tcu::Vec2(0.5f, 0.5f)).swizzle(0, 1, 2); });

    UNIFORM_STRUCT_CASE(
        sampler_in_array_function_arg, "Sampler in struct as function arg", FLAG_USES_TEXTURES,
        LineStream() << "${DECLARATIONS}"
                     << ""
                     << "struct S {"
                     << "    sampler2D        source;"
                     << "};"
                     << ""
                     << "mediump vec4 fun(S s[2]) {"
                     << "    return texture2D(s[0].source, vec2(0.5));"
                     << "}"
                     << ""
                     << "uniform S s[2];"
                     << "void main (void)"
                     << "{"
                     << "    ${DST} = fun(s);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            DE_UNREF(constCoords);
            setUniform(gl, programID, "s[0].source", 0);
        },
        { c.color.xyz() = c.texture2D(TEXTURE_BRICK, tcu::Vec2(0.5f, 0.5f)).swizzle(0, 1, 2); });

    UNIFORM_STRUCT_CASE(
        equal, "Struct equality", 0,
        LineStream() << "${DECLARATIONS}"
                     << "uniform mediump float uf_one;"
                     << "uniform int ui_two;"
                     << ""
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    mediump vec3    b;"
                     << "    int                c;"
                     << "};"
                     << "uniform S a;"
                     << "uniform S b;"
                     << "uniform S c;"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    S d = S(uf_one, vec3(0.0, floor(${COORDS}.y+1.0), 2.0), ui_two);"
                     << "    ${DST} = vec4(0.0, 0.0, 0.0, 1.0);"
                     << "    if (a == b) ${DST}.x = 1.0;"
                     << "    if (a == c) ${DST}.y = 1.0;"
                     << "    if (a == d) ${DST}.z = 1.0;"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            DE_UNREF(constCoords);
            setUniform(gl, programID, "a.a", 1.0f);
            setUniform(gl, programID, "a.b", tcu::Vec3(0.0f, 1.0f, 2.0f));
            setUniform(gl, programID, "a.c", 2);
            setUniform(gl, programID, "b.a", 1.0f);
            setUniform(gl, programID, "b.b", tcu::Vec3(0.0f, 1.0f, 2.0f));
            setUniform(gl, programID, "b.c", 2);
            setUniform(gl, programID, "c.a", 1.0f);
            setUniform(gl, programID, "c.b", tcu::Vec3(0.0f, 1.1f, 2.0f));
            setUniform(gl, programID, "c.c", 2);
        },
        {
            c.color.xy() = tcu::Vec2(1.0f, 0.0f);
            if (deFloatFloor(c.coords[1] + 1.0f) == deFloatFloor(1.1f))
                c.color.z() = 1.0f;
        });

    UNIFORM_STRUCT_CASE(
        not_equal, "Struct equality", 0,
        LineStream() << "${DECLARATIONS}"
                     << "uniform mediump float uf_one;"
                     << "uniform int ui_two;"
                     << ""
                     << "struct S {"
                     << "    mediump float    a;"
                     << "    mediump vec3    b;"
                     << "    int                c;"
                     << "};"
                     << "uniform S a;"
                     << "uniform S b;"
                     << "uniform S c;"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    S d = S(uf_one, vec3(0.0, floor(${COORDS}.y+1.0), 2.0), ui_two);"
                     << "    ${DST} = vec4(0.0, 0.0, 0.0, 1.0);"
                     << "    if (a != b) ${DST}.x = 1.0;"
                     << "    if (a != c) ${DST}.y = 1.0;"
                     << "    if (a != d) ${DST}.z = 1.0;"
                     << "    ${ASSIGN_POS}"
                     << "}",
        {
            DE_UNREF(constCoords);
            setUniform(gl, programID, "a.a", 1.0f);
            setUniform(gl, programID, "a.b", tcu::Vec3(0.0f, 1.0f, 2.0f));
            setUniform(gl, programID, "a.c", 2);
            setUniform(gl, programID, "b.a", 1.0f);
            setUniform(gl, programID, "b.b", tcu::Vec3(0.0f, 1.0f, 2.0f));
            setUniform(gl, programID, "b.c", 2);
            setUniform(gl, programID, "c.a", 1.0f);
            setUniform(gl, programID, "c.b", tcu::Vec3(0.0f, 1.1f, 2.0f));
            setUniform(gl, programID, "c.c", 2);
        },
        {
            c.color.xy() = tcu::Vec2(0.0f, 1.0f);
            if (deFloatFloor(c.coords[1] + 1.0f) != deFloatFloor(1.1f))
                c.color.z() = 1.0f;
        });
}

ShaderStructTests::ShaderStructTests(Context &context) : TestCaseGroup(context, "struct", "Struct Tests")
{
}

ShaderStructTests::~ShaderStructTests(void)
{
}

void ShaderStructTests::init(void)
{
    addChild(new LocalStructTests(m_context));
    addChild(new UniformStructTests(m_context));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
