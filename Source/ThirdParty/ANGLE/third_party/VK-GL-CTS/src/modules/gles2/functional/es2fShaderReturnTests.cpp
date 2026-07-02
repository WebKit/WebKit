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
 * \brief Shader return statement tests.
 *//*--------------------------------------------------------------------*/

#include "es2fShaderReturnTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "tcuStringTemplate.hpp"

#include <map>
#include <sstream>
#include <string>

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

enum ReturnMode
{
    RETURNMODE_ALWAYS = 0,
    RETURNMODE_NEVER,
    RETURNMODE_DYNAMIC,

    RETURNMODE_LAST
};

enum RequireFlags
{
    REQUIRE_DYNAMIC_LOOPS = (1 << 0),
};

// Evaluation functions
inline void evalReturnAlways(ShaderEvalContext &c)
{
    c.color.xyz() = c.coords.swizzle(0, 1, 2);
}
inline void evalReturnNever(ShaderEvalContext &c)
{
    c.color.xyz() = c.coords.swizzle(3, 2, 1);
}
inline void evalReturnDynamic(ShaderEvalContext &c)
{
    c.color.xyz() = (c.coords.x() + c.coords.y() >= 0.0f) ? c.coords.swizzle(0, 1, 2) : c.coords.swizzle(3, 2, 1);
}

static ShaderEvalFunc getEvalFunc(ReturnMode mode)
{
    switch (mode)
    {
    case RETURNMODE_ALWAYS:
        return evalReturnAlways;
    case RETURNMODE_NEVER:
        return evalReturnNever;
    case RETURNMODE_DYNAMIC:
        return evalReturnDynamic;
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

class ShaderReturnCase : public ShaderRenderCase
{
public:
    ShaderReturnCase(Context &context, const char *name, const char *description, bool isVertexCase,
                     const char *shaderSource, ShaderEvalFunc evalFunc, uint32_t requirements = 0);
    virtual ~ShaderReturnCase(void);

    void init(void);

private:
    const uint32_t m_requirements;
};

ShaderReturnCase::ShaderReturnCase(Context &context, const char *name, const char *description, bool isVertexCase,
                                   const char *shaderSource, ShaderEvalFunc evalFunc, uint32_t requirements)
    : ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name,
                       description, isVertexCase, evalFunc)
    , m_requirements(requirements)
{
    if (isVertexCase)
    {
        m_vertShaderSource = shaderSource;
        m_fragShaderSource = "varying mediump vec4 v_color;\n\n"
                             "void main (void)\n"
                             "{\n"
                             "    gl_FragColor = v_color;\n"
                             "}\n";
    }
    else
    {
        m_fragShaderSource = shaderSource;
        m_vertShaderSource = "attribute highp   vec4 a_position;\n"
                             "attribute highp   vec4 a_coords;\n"
                             "varying   mediump vec4 v_coords;\n\n"
                             "void main (void)\n"
                             "{\n"
                             "    gl_Position = a_position;\n"
                             "    v_coords = a_coords;\n"
                             "}\n";
    }
}

ShaderReturnCase::~ShaderReturnCase(void)
{
}

void ShaderReturnCase::init(void)
{
    try
    {
        ShaderRenderCase::init();
    }
    catch (const CompileFailed &)
    {
        if (m_requirements & REQUIRE_DYNAMIC_LOOPS)
        {
            const bool isSupported =
                m_isVertexCase ? m_ctxInfo.isVertexDynamicLoopSupported() : m_ctxInfo.isFragmentDynamicLoopSupported();
            if (!isSupported)
                throw tcu::NotSupportedError("Dynamic loops not supported");
        }

        throw;
    }
}

ShaderReturnTests::ShaderReturnTests(Context &context) : TestCaseGroup(context, "return", "Return Statement Tests")
{
}

ShaderReturnTests::~ShaderReturnTests(void)
{
}

ShaderReturnCase *makeConditionalReturnInFuncCase(Context &context, const char *name, const char *description,
                                                  ReturnMode returnMode, bool isVertex)
{
    // Template
    StringTemplate tmpl("${COORDSTORAGE} ${COORDPREC} vec4 ${COORDS};\n"
                        "${EXTRADECL}\n"
                        "${COORDPREC} vec4 getColor (void)\n"
                        "{\n"
                        "    if (${RETURNCOND})\n"
                        "        return vec4(${COORDS}.xyz, 1.0);\n"
                        "    return vec4(${COORDS}.wzy, 1.0);\n"
                        "}\n\n"
                        "void main (void)\n"
                        "{\n"
                        "${POSITIONWRITE}"
                        "    ${OUTPUT} = getColor();\n"
                        "}\n");

    const char *coords = isVertex ? "a_coords" : "v_coords";

    map<string, string> params;

    params["COORDSTORAGE"]  = isVertex ? "attribute" : "varying";
    params["COORDPREC"]     = isVertex ? "highp" : "mediump";
    params["OUTPUT"]        = isVertex ? "v_color" : "gl_FragColor";
    params["COORDS"]        = coords;
    params["EXTRADECL"]     = isVertex ? "attribute highp vec4 a_position;\nvarying mediump vec4 v_color;\n" : "";
    params["POSITIONWRITE"] = isVertex ? "    gl_Position = a_position;\n" : "";

    switch (returnMode)
    {
    case RETURNMODE_ALWAYS:
        params["RETURNCOND"] = "true";
        break;
    case RETURNMODE_NEVER:
        params["RETURNCOND"] = "false";
        break;
    case RETURNMODE_DYNAMIC:
        params["RETURNCOND"] = string(coords) + ".x+" + coords + ".y >= 0.0";
        break;
    default:
        DE_ASSERT(false);
    }

    return new ShaderReturnCase(context, name, description, isVertex, tmpl.specialize(params).c_str(),
                                getEvalFunc(returnMode));
}

ShaderReturnCase *makeOutputWriteReturnCase(Context &context, const char *name, const char *description,
                                            bool inFunction, ReturnMode returnMode, bool isVertex)
{
    // Template
    StringTemplate tmpl(inFunction ? "${COORDATTRS} vec4 ${COORDS};\n"
                                     "${EXTRADECL}\n"
                                     "void myfunc (void)\n"
                                     "{\n"
                                     "    ${OUTPUT} = vec4(${COORDS}.xyz, 1.0);\n"
                                     "    if (${RETURNCOND})\n"
                                     "        return;\n"
                                     "    ${OUTPUT} = vec4(${COORDS}.wzy, 1.0);\n"
                                     "}\n\n"
                                     "void main (void)\n"
                                     "{\n"
                                     "${POSITIONWRITE}"
                                     "    myfunc();\n"
                                     "}\n" :
                                     "${COORDATTRS} vec4 ${COORDS};\n"
                                     "uniform mediump int ui_one;\n"
                                     "${EXTRADECL}\n"
                                     "void main ()\n"
                                     "{\n"
                                     "${POSITIONWRITE}"
                                     "    ${OUTPUT} = vec4(${COORDS}.xyz, 1.0);\n"
                                     "    if (${RETURNCOND})\n"
                                     "        return;\n"
                                     "    ${OUTPUT} = vec4(${COORDS}.wzy, 1.0);\n"
                                     "}\n");

    const char *coords = isVertex ? "a_coords" : "v_coords";

    map<string, string> params;

    params["COORDATTRS"]    = isVertex ? "attribute highp" : "varying mediump";
    params["COORDS"]        = coords;
    params["OUTPUT"]        = isVertex ? "v_color" : "gl_FragColor";
    params["EXTRADECL"]     = isVertex ? "attribute highp vec4 a_position;\nvarying mediump vec4 v_color;\n" : "";
    params["POSITIONWRITE"] = isVertex ? "    gl_Position = a_position;\n" : "";

    switch (returnMode)
    {
    case RETURNMODE_ALWAYS:
        params["RETURNCOND"] = "true";
        break;
    case RETURNMODE_NEVER:
        params["RETURNCOND"] = "false";
        break;
    case RETURNMODE_DYNAMIC:
        params["RETURNCOND"] = string(coords) + ".x+" + coords + ".y >= 0.0";
        break;
    default:
        DE_ASSERT(false);
    }

    return new ShaderReturnCase(context, name, description, isVertex, tmpl.specialize(params).c_str(),
                                getEvalFunc(returnMode));
}

ShaderReturnCase *makeReturnInLoopCase(Context &context, const char *name, const char *description, bool isDynamicLoop,
                                       ReturnMode returnMode, bool isVertex)
{
    // Template
    StringTemplate tmpl("${COORDSTORAGE} ${COORDPREC} vec4 ${COORDS};\n"
                        "uniform mediump int ui_one;\n"
                        "${EXTRADECL}\n"
                        "${COORDPREC} vec4 getCoords (void)\n"
                        "{\n"
                        "    ${COORDPREC} vec4 coords = ${COORDS};\n"
                        "    for (int i = 0; i < ${ITERLIMIT}; i++)\n"
                        "    {\n"
                        "        if (${RETURNCOND})\n"
                        "            return coords;\n"
                        "        coords = coords.wzyx;\n"
                        "    }\n"
                        "    return coords;\n"
                        "}\n\n"
                        "void main (void)\n"
                        "{\n"
                        "${POSITIONWRITE}"
                        "    ${OUTPUT} = vec4(getCoords().xyz, 1.0);\n"
                        "}\n");

    const char *coords = isVertex ? "a_coords" : "v_coords";

    map<string, string> params;

    params["COORDSTORAGE"]  = isVertex ? "attribute" : "varying";
    params["COORDPREC"]     = isVertex ? "highp" : "mediump";
    params["OUTPUT"]        = isVertex ? "v_color" : "gl_FragColor";
    params["COORDS"]        = coords;
    params["EXTRADECL"]     = isVertex ? "attribute highp vec4 a_position;\nvarying mediump vec4 v_color;\n" : "";
    params["POSITIONWRITE"] = isVertex ? "    gl_Position = a_position;\n" : "";
    params["ITERLIMIT"]     = isDynamicLoop ? "ui_one" : "1";

    switch (returnMode)
    {
    case RETURNMODE_ALWAYS:
        params["RETURNCOND"] = "true";
        break;
    case RETURNMODE_NEVER:
        params["RETURNCOND"] = "false";
        break;
    case RETURNMODE_DYNAMIC:
        params["RETURNCOND"] = string(coords) + ".x+" + coords + ".y >= 0.0";
        break;
    default:
        DE_ASSERT(false);
    }

    return new ShaderReturnCase(context, name, description, isVertex, tmpl.specialize(params).c_str(),
                                getEvalFunc(returnMode), isDynamicLoop ? REQUIRE_DYNAMIC_LOOPS : 0);
}

static const char *getReturnModeName(ReturnMode mode)
{
    switch (mode)
    {
    case RETURNMODE_ALWAYS:
        return "always";
    case RETURNMODE_NEVER:
        return "never";
    case RETURNMODE_DYNAMIC:
        return "dynamic";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

static const char *getReturnModeDesc(ReturnMode mode)
{
    switch (mode)
    {
    case RETURNMODE_ALWAYS:
        return "Always return";
    case RETURNMODE_NEVER:
        return "Never return";
    case RETURNMODE_DYNAMIC:
        return "Return based on coords";
    default:
        DE_ASSERT(false);
        return nullptr;
    }
}

void ShaderReturnTests::init(void)
{
    // Single return statement in function.
    addChild(new ShaderReturnCase(m_context, "single_return_vertex", "Single return statement in function", true,
                                  "attribute highp vec4 a_position;\n"
                                  "attribute highp vec4 a_coords;\n"
                                  "varying highp vec4 v_color;\n\n"
                                  "vec4 getColor (void)\n"
                                  "{\n"
                                  "    return vec4(a_coords.xyz, 1.0);\n"
                                  "}\n\n"
                                  "void main (void)\n"
                                  "{\n"
                                  "    gl_Position = a_position;\n"
                                  "    v_color = getColor();\n"
                                  "}\n",
                                  evalReturnAlways));
    addChild(new ShaderReturnCase(m_context, "single_return_fragment", "Single return statement in function", false,
                                  "varying mediump vec4 v_coords;\n"
                                  "mediump vec4 getColor (void)\n"
                                  "{\n"
                                  "    return vec4(v_coords.xyz, 1.0);\n"
                                  "}\n\n"
                                  "void main (void)\n"
                                  "{\n"
                                  "    gl_FragColor = getColor();\n"
                                  "}\n",
                                  evalReturnAlways));

    // Conditional return statement in function.
    for (int returnMode = 0; returnMode < RETURNMODE_LAST; returnMode++)
    {
        for (int isFragment = 0; isFragment < 2; isFragment++)
        {
            string name = string("conditional_return_") + getReturnModeName((ReturnMode)returnMode) +
                          (isFragment ? "_fragment" : "_vertex");
            string description = string(getReturnModeDesc((ReturnMode)returnMode)) + " in function";
            addChild(makeConditionalReturnInFuncCase(m_context, name.c_str(), description.c_str(),
                                                     (ReturnMode)returnMode, isFragment == 0));
        }
    }

    // Unconditional double return in function.
    addChild(new ShaderReturnCase(m_context, "double_return_vertex", "Unconditional double return in function", true,
                                  "attribute highp vec4 a_position;\n"
                                  "attribute highp vec4 a_coords;\n"
                                  "varying highp vec4 v_color;\n\n"
                                  "vec4 getColor (void)\n"
                                  "{\n"
                                  "    return vec4(a_coords.xyz, 1.0);\n"
                                  "    return vec4(a_coords.wzy, 1.0);\n"
                                  "}\n\n"
                                  "void main (void)\n"
                                  "{\n"
                                  "    gl_Position = a_position;\n"
                                  "    v_color = getColor();\n"
                                  "}\n",
                                  evalReturnAlways));
    addChild(new ShaderReturnCase(m_context, "double_return_fragment", "Unconditional double return in function", false,
                                  "varying mediump vec4 v_coords;\n"
                                  "mediump vec4 getColor (void)\n"
                                  "{\n"
                                  "    return vec4(v_coords.xyz, 1.0);\n"
                                  "    return vec4(v_coords.wzy, 1.0);\n"
                                  "}\n\n"
                                  "void main (void)\n"
                                  "{\n"
                                  "    gl_FragColor = getColor();\n"
                                  "}\n",
                                  evalReturnAlways));

    // Last statement in main.
    addChild(new ShaderReturnCase(m_context, "last_statement_in_main_vertex", "Return as a final statement in main()",
                                  true,
                                  "attribute highp vec4 a_position;\n"
                                  "attribute highp vec4 a_coords;\n"
                                  "varying highp vec4 v_color;\n\n"
                                  "void main (void)\n"
                                  "{\n"
                                  "    gl_Position = a_position;\n"
                                  "    v_color = vec4(a_coords.xyz, 1.0);\n"
                                  "    return;\n"
                                  "}\n",
                                  evalReturnAlways));
    addChild(new ShaderReturnCase(m_context, "last_statement_in_main_fragment", "Return as a final statement in main()",
                                  false,
                                  "varying mediump vec4 v_coords;\n\n"
                                  "void main (void)\n"
                                  "{\n"
                                  "    gl_FragColor = vec4(v_coords.xyz, 1.0);\n"
                                  "    return;\n"
                                  "}\n",
                                  evalReturnAlways));

    // Return between output variable writes.
    for (int inFunc = 0; inFunc < 2; inFunc++)
    {
        for (int returnMode = 0; returnMode < RETURNMODE_LAST; returnMode++)
        {
            for (int isFragment = 0; isFragment < 2; isFragment++)
            {
                string name = string("output_write_") + (inFunc ? "in_func_" : "") +
                              getReturnModeName((ReturnMode)returnMode) + (isFragment ? "_fragment" : "_vertex");
                string desc = string(getReturnModeDesc((ReturnMode)returnMode)) +
                              (inFunc ? " in user-defined function" : " in main()") + " between output writes";

                addChild(makeOutputWriteReturnCase(m_context, name.c_str(), desc.c_str(), inFunc != 0,
                                                   (ReturnMode)returnMode, isFragment == 0));
            }
        }
    }

    // Conditional return statement in loop.
    for (int isDynamicLoop = 0; isDynamicLoop < 2; isDynamicLoop++)
    {
        for (int returnMode = 0; returnMode < RETURNMODE_LAST; returnMode++)
        {
            for (int isFragment = 0; isFragment < 2; isFragment++)
            {
                string name = string("return_in_") + (isDynamicLoop ? "dynamic" : "static") + "_loop_" +
                              getReturnModeName((ReturnMode)returnMode) + (isFragment ? "_fragment" : "_vertex");
                string description = string(getReturnModeDesc((ReturnMode)returnMode)) + " in loop";
                addChild(makeReturnInLoopCase(m_context, name.c_str(), description.c_str(), isDynamicLoop != 0,
                                              (ReturnMode)returnMode, isFragment == 0));
            }
        }
    }

    // Unconditional return in infinite loop.
    addChild(new ShaderReturnCase(m_context, "return_in_infinite_loop_vertex", "Return in infinite loop", true,
                                  "attribute highp vec4 a_position;\n"
                                  "attribute highp vec4 a_coords;\n"
                                  "varying highp vec4 v_color;\n"
                                  "uniform int ui_zero;\n\n"
                                  "highp vec4 getCoords (void)\n"
                                  "{\n"
                                  "    for (int i = 1; i < 10; i += ui_zero)\n"
                                  "        return a_coords;\n"
                                  "    return a_coords.wzyx;\n"
                                  "}\n\n"
                                  "void main (void)\n"
                                  "{\n"
                                  "    gl_Position = a_position;\n"
                                  "    v_color = vec4(getCoords().xyz, 1.0);\n"
                                  "    return;\n"
                                  "}\n",
                                  evalReturnAlways, REQUIRE_DYNAMIC_LOOPS));
    addChild(new ShaderReturnCase(m_context, "return_in_infinite_loop_fragment", "Return in infinite loop", false,
                                  "varying mediump vec4 v_coords;\n"
                                  "uniform int ui_zero;\n\n"
                                  "mediump vec4 getCoords (void)\n"
                                  "{\n"
                                  "    for (int i = 1; i < 10; i += ui_zero)\n"
                                  "        return v_coords;\n"
                                  "    return v_coords.wzyx;\n"
                                  "}\n\n"
                                  "void main (void)\n"
                                  "{\n"
                                  "    gl_FragColor = vec4(getCoords().xyz, 1.0);\n"
                                  "    return;\n"
                                  "}\n",
                                  evalReturnAlways, REQUIRE_DYNAMIC_LOOPS));
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
