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
 * \brief Shader struct tests.
 *//*--------------------------------------------------------------------*/

#include "es3fShaderFunctionTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "gluTexture.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"

using namespace deqp::gls;

namespace deqp
{
namespace gles3
{
namespace Functional
{

typedef void (*SetupUniformsFunc)(const glw::Functions &gl, uint32_t programID, const tcu::Vec4 &constCoords);

class ShaderFunctionCase : public ShaderRenderCase
{
public:
    ShaderFunctionCase(Context &context, const char *name, const char *description, bool isVertexCase,
                       bool usesTextures, ShaderEvalFunc evalFunc, SetupUniformsFunc setupUniformsFunc,
                       const char *vertShaderSource, const char *fragShaderSource);
    ~ShaderFunctionCase(void);

    void init(void);
    void deinit(void);

    virtual void setupUniforms(int programID, const tcu::Vec4 &constCoords);

private:
    ShaderFunctionCase(const ShaderFunctionCase &);
    ShaderFunctionCase &operator=(const ShaderFunctionCase &);

    SetupUniformsFunc m_setupUniforms;
    bool m_usesTexture;

    glu::Texture2D *m_brickTexture;
};

ShaderFunctionCase::ShaderFunctionCase(Context &context, const char *name, const char *description, bool isVertexCase,
                                       bool usesTextures, ShaderEvalFunc evalFunc, SetupUniformsFunc setupUniformsFunc,
                                       const char *vertShaderSource, const char *fragShaderSource)
    : ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name,
                       description, isVertexCase, evalFunc)
    , m_setupUniforms(setupUniformsFunc)
    , m_usesTexture(usesTextures)
    , m_brickTexture(nullptr)
{
    m_vertShaderSource = vertShaderSource;
    m_fragShaderSource = fragShaderSource;
}

ShaderFunctionCase::~ShaderFunctionCase(void)
{
    delete m_brickTexture;
}

void ShaderFunctionCase::init(void)
{
    if (m_usesTexture)
    {
        m_brickTexture = glu::Texture2D::create(m_renderCtx, m_ctxInfo, m_testCtx.getArchive(), "data/brick.png");
        m_textures.push_back(TextureBinding(
            m_brickTexture, tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
                                         tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::LINEAR, tcu::Sampler::LINEAR)));
        DE_ASSERT(m_textures.size() == 1);
    }
    gls::ShaderRenderCase::init();
}

void ShaderFunctionCase::deinit(void)
{
    gls::ShaderRenderCase::deinit();
    delete m_brickTexture;
    m_brickTexture = nullptr;
}

void ShaderFunctionCase::setupUniforms(int programID, const tcu::Vec4 &constCoords)
{
    ShaderRenderCase::setupUniforms(programID, constCoords);
    if (m_setupUniforms)
        m_setupUniforms(m_renderCtx.getFunctions(), programID, constCoords);
}

static ShaderFunctionCase *createStructCase(Context &context, const char *name, const char *description,
                                            bool isVertexCase, bool usesTextures, ShaderEvalFunc evalFunc,
                                            SetupUniformsFunc setupUniforms, const LineStream &shaderSrc,
                                            const std::map<std::string, std::string> *additionalParams)
{
    static const char *defaultVertSrc = "#version 300 es\n"
                                        "in highp vec4 a_position;\n"
                                        "in highp vec4 a_coords;\n"
                                        "out mediump vec4 v_coords;\n\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    v_coords = a_coords;\n"
                                        "    gl_Position = a_position;\n"
                                        "}\n";
    static const char *defaultFragSrc = "#version 300 es\n"
                                        "in mediump vec4 v_color;\n"
                                        "layout(location = 0) out mediump vec4 o_color;\n\n"
                                        "void main (void)\n"
                                        "{\n"
                                        "    o_color = v_color;\n"
                                        "}\n";

    // Fill in specialization parameters.
    std::map<std::string, std::string> spParams;
    if (isVertexCase)
    {
        spParams["HEADER"]     = "#version 300 es\n"
                                 "in highp vec4 a_position;\n"
                                 "in highp vec4 a_coords;\n"
                                 "out mediump vec4 v_color;";
        spParams["COORDS"]     = "a_coords";
        spParams["DST"]        = "v_color";
        spParams["ASSIGN_POS"] = "gl_Position = a_position;";
    }
    else
    {
        spParams["HEADER"]     = "#version 300 es\n"
                                 "precision mediump float;\n"
                                 "in mediump vec4 v_coords;\n"
                                 "layout(location = 0) out mediump vec4 o_color;";
        spParams["COORDS"]     = "v_coords";
        spParams["DST"]        = "o_color";
        spParams["ASSIGN_POS"] = "";
    }
    if (additionalParams)
        spParams.insert(additionalParams->begin(), additionalParams->end());

    if (isVertexCase)
        return new ShaderFunctionCase(context, name, description, isVertexCase, usesTextures, evalFunc, setupUniforms,
                                      tcu::StringTemplate(shaderSrc.str()).specialize(spParams).c_str(),
                                      defaultFragSrc);
    else
        return new ShaderFunctionCase(context, name, description, isVertexCase, usesTextures, evalFunc, setupUniforms,
                                      defaultVertSrc,
                                      tcu::StringTemplate(shaderSrc.str()).specialize(spParams).c_str());
}

ShaderFunctionTests::ShaderFunctionTests(Context &context) : TestCaseGroup(context, "function", "Function Tests")
{
}

ShaderFunctionTests::~ShaderFunctionTests(void)
{
}

void ShaderFunctionTests::init(void)
{
#define FUNCTION_CASE_PARAMETERIZED(NAME, DESCRIPTION, SHADER_SRC, EVAL_FUNC_BODY, PARAMS)                           \
    do                                                                                                               \
    {                                                                                                                \
        struct Eval_##NAME                                                                                           \
        {                                                                                                            \
            static void eval(ShaderEvalContext &c) EVAL_FUNC_BODY                                                    \
        }; /* NOLINT(EVAL_FUNC_BODY) */                                                                              \
        addChild(createStructCase(m_context, #NAME "_vertex", DESCRIPTION, true, false, &Eval_##NAME::eval, nullptr, \
                                  SHADER_SRC, PARAMS));                                                              \
        addChild(createStructCase(m_context, #NAME "_fragment", DESCRIPTION, false, false, &Eval_##NAME::eval,       \
                                  nullptr, SHADER_SRC, PARAMS));                                                     \
    } while (false)

#define FUNCTION_CASE(NAME, DESCRIPTION, SHADER_SRC, EVAL_FUNC_BODY) \
    FUNCTION_CASE_PARAMETERIZED(NAME, DESCRIPTION, SHADER_SRC, EVAL_FUNC_BODY, nullptr)

    FUNCTION_CASE(local_variable_aliasing, "Function out parameter aliases local variable",
                  LineStream() << "${HEADER}"
                               << ""
                               << "bool out_params_are_distinct(float x, out float y) {"
                               << "    y = 2.;"
                               << "    return x == 1. && y == 2.;"
                               << "}"
                               << ""
                               << "void main (void)"
                               << "{"
                               << "    float x = 1.;"
                               << "    ${DST} = out_params_are_distinct(x, x) ? vec4(0.,1.,0.,1.) : vec4(1.,0.,0.,1.);"
                               << "    ${ASSIGN_POS}"
                               << "}",
                  { c.color.xyz() = tcu::Vec3(0.0f, 1.0f, 0.0f); });

    FUNCTION_CASE(
        global_variable_aliasing, "Function out parameter aliases global variable",
        LineStream() << "${HEADER}"
                     << ""
                     << ""
                     << "float x = 1.;"
                     << "bool out_params_are_distinct_from_global(out float y) {"
                     << "    y = 2.;"
                     << "    return x == 1. && y == 2.;"
                     << "}"
                     << ""
                     << "void main (void)"
                     << "{"
                     << "    ${DST} = out_params_are_distinct_from_global(x) ? vec4(0.,1.,0.,1.) : vec4(1.,0.,0.,1.);"
                     << "    ${ASSIGN_POS}"
                     << "}",
        { c.color.xyz() = tcu::Vec3(0.0f, 1.0f, 0.0f); });
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
