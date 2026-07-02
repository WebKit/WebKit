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

#include "es2fShaderFunctionTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "gluTexture.hpp"
#include "tcuStringTemplate.hpp"
#include "tcuTextureUtil.hpp"

using namespace deqp::gls;

namespace deqp
{
namespace gles2
{
namespace Functional
{

typedef void (*SetupUniformsFunc)(const glw::Functions &gl, uint32_t programID, const tcu::Vec4 &constCoords);

class ShaderFunctionCase : public ShaderRenderCase
{
public:
    ShaderFunctionCase(Context &context, const char *name, const char *description, bool isVertexCase,
                       ShaderEvalFunc evalFunc, SetupUniformsFunc setupUniformsFunc, const char *vertShaderSource,
                       const char *fragShaderSource);
    ~ShaderFunctionCase(void);

    void init(void);
    void deinit(void);

    virtual void setupUniforms(int programID, const tcu::Vec4 &constCoords);

private:
    ShaderFunctionCase(const ShaderFunctionCase &);
    ShaderFunctionCase &operator=(const ShaderFunctionCase &);

    const SetupUniformsFunc m_setupUniforms;

    glu::Texture2D *m_brickTexture;
};

ShaderFunctionCase::ShaderFunctionCase(Context &context, const char *name, const char *description, bool isVertexCase,
                                       ShaderEvalFunc evalFunc, SetupUniformsFunc setupUniformsFunc,
                                       const char *vertShaderSource, const char *fragShaderSource)
    : ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name,
                       description, isVertexCase, evalFunc)
    , m_setupUniforms(setupUniformsFunc)
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
                                            bool isVertexCase, ShaderEvalFunc evalFunc, SetupUniformsFunc setupUniforms,
                                            const LineStream &shaderSrc,
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
        spParams["DECLARATIONS"] = "precision highp float;\n"
                                   "varying mediump vec4 v_coords;";
        spParams["COORDS"]       = "v_coords";
        spParams["DST"]          = "gl_FragColor";
        spParams["ASSIGN_POS"]   = "";
    }
    if (additionalParams)
        spParams.insert(additionalParams->begin(), additionalParams->end());

    if (isVertexCase)
        return new ShaderFunctionCase(context, name, description, isVertexCase, evalFunc, setupUniforms,
                                      tcu::StringTemplate(shaderSrc.str()).specialize(spParams).c_str(),
                                      defaultFragSrc);
    else
        return new ShaderFunctionCase(context, name, description, isVertexCase, evalFunc, setupUniforms, defaultVertSrc,
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
#define FUNCTION_CASE_PARAMETERIZED(NAME, DESCRIPTION, SHADER_SRC, EVAL_FUNC_BODY, PARAMS)                       \
    do                                                                                                           \
    {                                                                                                            \
        struct Eval_##NAME                                                                                       \
        {                                                                                                        \
            static void eval(ShaderEvalContext &c) EVAL_FUNC_BODY                                                \
        }; /* NOLINT(EVAL_FUNC_BODY) */                                                                          \
        addChild(createStructCase(m_context, #NAME "_vertex", DESCRIPTION, true, &Eval_##NAME::eval, nullptr,    \
                                  SHADER_SRC, PARAMS));                                                          \
        addChild(createStructCase(m_context, #NAME "_fragment", DESCRIPTION, false, &Eval_##NAME::eval, nullptr, \
                                  SHADER_SRC, PARAMS));                                                          \
    } while (false)

#define FUNCTION_CASE(NAME, DESCRIPTION, SHADER_SRC, EVAL_FUNC_BODY) \
    FUNCTION_CASE_PARAMETERIZED(NAME, DESCRIPTION, SHADER_SRC, EVAL_FUNC_BODY, nullptr)

    FUNCTION_CASE(local_variable_aliasing, "Function out parameter aliases local variable",
                  LineStream() << "${DECLARATIONS}"
                               << ""
                               << "bool out_params_are_distinct(float x, out float y)"
                               << "{"
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
        LineStream() << "${DECLARATIONS}"
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
} // namespace gles2
} // namespace deqp
