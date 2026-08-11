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
 * \brief Shader discard statement tests.
 *//*--------------------------------------------------------------------*/

#include "es2fShaderDiscardTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "tcuStringTemplate.hpp"
#include "gluTexture.hpp"

#include <map>
#include <sstream>
#include <string>

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

using tcu::StringTemplate;

using std::map;
using std::ostringstream;
using std::string;

using namespace glu;
using namespace deqp::gls;

namespace deqp
{
namespace gles2
{
namespace Functional
{

enum CaseFlags
{
    FLAG_USES_TEXTURES          = (1 << 0),
    FLAG_REQUIRES_DYNAMIC_LOOPS = (1 << 1),
};

class ShaderDiscardCase : public ShaderRenderCase
{
public:
    ShaderDiscardCase(Context &context, const char *name, const char *description, const char *shaderSource,
                      ShaderEvalFunc evalFunc, uint32_t flags);
    virtual ~ShaderDiscardCase(void);

    void init(void);
    void deinit(void);

    void setupUniforms(int programID, const tcu::Vec4 &constCoords);

private:
    const uint32_t m_flags;
    glu::Texture2D *m_brickTexture;
};

ShaderDiscardCase::ShaderDiscardCase(Context &context, const char *name, const char *description,
                                     const char *shaderSource, ShaderEvalFunc evalFunc, uint32_t flags)
    : ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name,
                       description, false, evalFunc)
    , m_flags(flags)
    , m_brickTexture(nullptr)
{
    m_fragShaderSource = shaderSource;
    m_vertShaderSource = "attribute highp   vec4 a_position;\n"
                         "attribute highp   vec4 a_coords;\n"
                         "varying   mediump vec4 v_color;\n"
                         "varying   mediump vec4 v_coords;\n\n"
                         "void main (void)\n"
                         "{\n"
                         "    gl_Position = a_position;\n"
                         "    v_color = vec4(a_coords.xyz, 1.0);\n"
                         "    v_coords = a_coords;\n"
                         "}\n";
}

ShaderDiscardCase::~ShaderDiscardCase(void)
{
    delete m_brickTexture;
}

void ShaderDiscardCase::init(void)
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

        throw;
    }

    if (m_flags & FLAG_USES_TEXTURES)
    {
        m_brickTexture = glu::Texture2D::create(m_renderCtx, m_ctxInfo, m_testCtx.getArchive(), "data/brick.png");
        m_textures.push_back(TextureBinding(
            m_brickTexture, tcu::Sampler(tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::CLAMP_TO_EDGE,
                                         tcu::Sampler::CLAMP_TO_EDGE, tcu::Sampler::LINEAR, tcu::Sampler::LINEAR)));
    }
}

void ShaderDiscardCase::deinit(void)
{
    gls::ShaderRenderCase::deinit();
    delete m_brickTexture;
    m_brickTexture = nullptr;
}

void ShaderDiscardCase::setupUniforms(int programID, const tcu::Vec4 &)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    gl.uniform1i(gl.getUniformLocation(programID, "ut_brick"), 0);
}

ShaderDiscardTests::ShaderDiscardTests(Context &context) : TestCaseGroup(context, "discard", "Discard statement tests")
{
}

ShaderDiscardTests::~ShaderDiscardTests(void)
{
}

enum DiscardMode
{
    DISCARDMODE_ALWAYS = 0,
    DISCARDMODE_NEVER,
    DISCARDMODE_UNIFORM,
    DISCARDMODE_DYNAMIC,
    DISCARDMODE_TEXTURE,

    DISCARDMODE_LAST
};

enum DiscardTemplate
{
    DISCARDTEMPLATE_MAIN_BASIC = 0,
    DISCARDTEMPLATE_FUNCTION_BASIC,
    DISCARDTEMPLATE_MAIN_STATIC_LOOP,
    DISCARDTEMPLATE_MAIN_DYNAMIC_LOOP,
    DISCARDTEMPLATE_FUNCTION_STATIC_LOOP,

    DISCARDTEMPLATE_LAST
};

// Evaluation functions
inline void evalDiscardAlways(ShaderEvalContext &c)
{
    c.discard();
}
inline void evalDiscardNever(ShaderEvalContext &c)
{
    c.color.xyz() = c.coords.swizzle(0, 1, 2);
}
inline void evalDiscardDynamic(ShaderEvalContext &c)
{
    c.color.xyz() = c.coords.swizzle(0, 1, 2);
    if (c.coords.x() + c.coords.y() > 0.0f)
        c.discard();
}

inline void evalDiscardTexture(ShaderEvalContext &c)
{
    c.color.xyz() = c.coords.swizzle(0, 1, 2);
    if (c.texture2D(0, c.coords.swizzle(0, 1) * 0.25f + 0.5f).x() < 0.7f)
        c.discard();
}

static ShaderEvalFunc getEvalFunc(DiscardMode mode)
{
    switch (mode)
    {
    case DISCARDMODE_ALWAYS:
        return evalDiscardAlways;
    case DISCARDMODE_NEVER:
        return evalDiscardNever;
    case DISCARDMODE_UNIFORM:
        return evalDiscardAlways;
    case DISCARDMODE_DYNAMIC:
        return evalDiscardDynamic;
    case DISCARDMODE_TEXTURE:
        return evalDiscardTexture;
    default:
        DE_ASSERT(false);
        return evalDiscardAlways;
    }
}

static const char *getTemplate(DiscardTemplate variant)
{
    switch (variant)
    {
    case DISCARDTEMPLATE_MAIN_BASIC:
        return "varying mediump vec4 v_color;\n"
               "varying mediump vec4 v_coords;\n"
               "uniform sampler2D    ut_brick;\n"
               "uniform mediump int  ui_one;\n\n"
               "void main (void)\n"
               "{\n"
               "    gl_FragColor = v_color;\n"
               "    ${DISCARD};\n"
               "}\n";

    case DISCARDTEMPLATE_FUNCTION_BASIC:
        return "varying mediump vec4 v_color;\n"
               "varying mediump vec4 v_coords;\n"
               "uniform sampler2D    ut_brick;\n"
               "uniform mediump int  ui_one;\n\n"
               "void myfunc (void)\n"
               "{\n"
               "    ${DISCARD};\n"
               "}\n\n"
               "void main (void)\n"
               "{\n"
               "    gl_FragColor = v_color;\n"
               "    myfunc();\n"
               "}\n";

    case DISCARDTEMPLATE_MAIN_STATIC_LOOP:
        return "varying mediump vec4 v_color;\n"
               "varying mediump vec4 v_coords;\n"
               "uniform sampler2D    ut_brick;\n"
               "uniform mediump int  ui_one;\n\n"
               "void main (void)\n"
               "{\n"
               "    gl_FragColor = v_color;\n"
               "    for (int i = 0; i < 2; i++)\n"
               "    {\n"
               "        if (i > 0)\n"
               "            ${DISCARD};\n"
               "    }\n"
               "}\n";

    case DISCARDTEMPLATE_MAIN_DYNAMIC_LOOP:
        return "varying mediump vec4 v_color;\n"
               "varying mediump vec4 v_coords;\n"
               "uniform sampler2D    ut_brick;\n"
               "uniform mediump int  ui_one;\n"
               "uniform mediump int  ui_two;\n\n"
               "void main (void)\n"
               "{\n"
               "    gl_FragColor = v_color;\n"
               "    for (int i = 0; i < ui_two; i++)\n"
               "    {\n"
               "        if (i > 0)\n"
               "            ${DISCARD};\n"
               "    }\n"
               "}\n";

    case DISCARDTEMPLATE_FUNCTION_STATIC_LOOP:
        return "varying mediump vec4 v_color;\n"
               "varying mediump vec4 v_coords;\n"
               "uniform sampler2D    ut_brick;\n"
               "uniform mediump int  ui_one;\n\n"
               "void myfunc (void)\n"
               "{\n"
               "    for (int i = 0; i < 2; i++)\n"
               "    {\n"
               "        if (i > 0)\n"
               "            ${DISCARD};\n"
               "    }\n"
               "}\n\n"
               "void main (void)\n"
               "{\n"
               "    gl_FragColor = v_color;\n"
               "    myfunc();\n"
               "}\n";

    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

static const char *getTemplateName(DiscardTemplate variant)
{
    switch (variant)
    {
    case DISCARDTEMPLATE_MAIN_BASIC:
        return "basic";
    case DISCARDTEMPLATE_FUNCTION_BASIC:
        return "function";
    case DISCARDTEMPLATE_MAIN_STATIC_LOOP:
        return "static_loop";
    case DISCARDTEMPLATE_MAIN_DYNAMIC_LOOP:
        return "dynamic_loop";
    case DISCARDTEMPLATE_FUNCTION_STATIC_LOOP:
        return "function_static_loop";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

static const char *getModeName(DiscardMode mode)
{
    switch (mode)
    {
    case DISCARDMODE_ALWAYS:
        return "always";
    case DISCARDMODE_NEVER:
        return "never";
    case DISCARDMODE_UNIFORM:
        return "uniform";
    case DISCARDMODE_DYNAMIC:
        return "dynamic";
    case DISCARDMODE_TEXTURE:
        return "texture";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

static const char *getTemplateDesc(DiscardTemplate variant)
{
    switch (variant)
    {
    case DISCARDTEMPLATE_MAIN_BASIC:
        return "main";
    case DISCARDTEMPLATE_FUNCTION_BASIC:
        return "function";
    case DISCARDTEMPLATE_MAIN_STATIC_LOOP:
        return "static loop";
    case DISCARDTEMPLATE_MAIN_DYNAMIC_LOOP:
        return "dynamic loop";
    case DISCARDTEMPLATE_FUNCTION_STATIC_LOOP:
        return "static loop in function";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

static const char *getModeDesc(DiscardMode mode)
{
    switch (mode)
    {
    case DISCARDMODE_ALWAYS:
        return "Always discard";
    case DISCARDMODE_NEVER:
        return "Never discard";
    case DISCARDMODE_UNIFORM:
        return "Discard based on uniform value";
    case DISCARDMODE_DYNAMIC:
        return "Discard based on varying values";
    case DISCARDMODE_TEXTURE:
        return "Discard based on texture value";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

ShaderDiscardCase *makeDiscardCase(Context &context, DiscardTemplate tmpl, DiscardMode mode)
{
    StringTemplate shaderTemplate(getTemplate(tmpl));

    map<string, string> params;

    switch (mode)
    {
    case DISCARDMODE_ALWAYS:
        params["DISCARD"] = "discard";
        break;
    case DISCARDMODE_NEVER:
        params["DISCARD"] = "if (false) discard";
        break;
    case DISCARDMODE_UNIFORM:
        params["DISCARD"] = "if (ui_one > 0) discard";
        break;
    case DISCARDMODE_DYNAMIC:
        params["DISCARD"] = "if (v_coords.x+v_coords.y > 0.0) discard";
        break;
    case DISCARDMODE_TEXTURE:
        params["DISCARD"] = "if (texture2D(ut_brick, v_coords.xy*0.25+0.5).x < 0.7) discard";
        break;
    default:
        DE_ASSERT(false);
        break;
    }

    string name        = string(getTemplateName(tmpl)) + "_" + getModeName(mode);
    string description = string(getModeDesc(mode)) + " in " + getTemplateDesc(tmpl);
    uint32_t flags     = (mode == DISCARDMODE_TEXTURE ? FLAG_USES_TEXTURES : 0) |
                     (tmpl == DISCARDTEMPLATE_MAIN_DYNAMIC_LOOP ? FLAG_REQUIRES_DYNAMIC_LOOPS : 0);

    return new ShaderDiscardCase(context, name.c_str(), description.c_str(), shaderTemplate.specialize(params).c_str(),
                                 getEvalFunc(mode), flags);
}

void ShaderDiscardTests::init(void)
{
    for (int tmpl = 0; tmpl < DISCARDTEMPLATE_LAST; tmpl++)
        for (int mode = 0; mode < DISCARDMODE_LAST; mode++)
            addChild(makeDiscardCase(m_context, (DiscardTemplate)tmpl, (DiscardMode)mode));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
