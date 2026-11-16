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
 * \brief Shader indexing (arrays, vector, matrices) tests.
 *//*--------------------------------------------------------------------*/

#include "es2fShaderIndexingTests.hpp"
#include "glsShaderRenderCase.hpp"
#include "gluShaderUtil.hpp"
#include "tcuStringTemplate.hpp"

#include "deInt32.h"

#include <map>

#include "glwFunctions.hpp"

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

enum IndexAccessType
{
    INDEXACCESS_STATIC = 0,
    INDEXACCESS_DYNAMIC,
    INDEXACCESS_STATIC_LOOP,
    INDEXACCESS_DYNAMIC_LOOP,

    /* Must be next to last, since most loop iterations won't include
     * _CONST
     */
    INDEXACCESS_CONST,
    INDEXACCESS_LAST
};

static const char *getIndexAccessTypeName(IndexAccessType accessType)
{
    static const char *s_names[INDEXACCESS_LAST] = {
        "static", "dynamic", "static_loop", "dynamic_loop", "const",
    };

    DE_ASSERT(deInBounds32((int)accessType, 0, INDEXACCESS_LAST));
    return s_names[(int)accessType];
}

enum VectorAccessType
{
    DIRECT = 0,
    COMPONENT,
    SUBSCRIPT_STATIC,
    SUBSCRIPT_DYNAMIC,
    SUBSCRIPT_STATIC_LOOP,
    SUBSCRIPT_DYNAMIC_LOOP,

    VECTORACCESS_LAST
};

static const char *getVectorAccessTypeName(VectorAccessType accessType)
{
    static const char *s_names[VECTORACCESS_LAST] = {"direct",
                                                     "component",
                                                     "static_subscript",
                                                     "dynamic_subscript",
                                                     "static_loop_subscript",
                                                     "dynamic_loop_subscript"};

    DE_ASSERT(deInBounds32((int)accessType, 0, VECTORACCESS_LAST));
    return s_names[(int)accessType];
}

enum RequirementFlags
{
    REQUIREMENT_UNIFORM_INDEXING       = (1 << 0),
    REQUIREMENT_VERTEX_UNIFORM_LOOPS   = (1 << 1),
    REQUIREMENT_FRAGMENT_UNIFORM_LOOPS = (1 << 2),
};

void evalArrayCoordsFloat(ShaderEvalContext &c)
{
    c.color.x() = 1.875f * c.coords.x();
}
void evalArrayCoordsVec2(ShaderEvalContext &c)
{
    c.color.xy() = 1.875f * c.coords.swizzle(0, 1);
}
void evalArrayCoordsVec3(ShaderEvalContext &c)
{
    c.color.xyz() = 1.875f * c.coords.swizzle(0, 1, 2);
}
void evalArrayCoordsVec4(ShaderEvalContext &c)
{
    c.color = 1.875f * c.coords;
}

static ShaderEvalFunc getArrayCoordsEvalFunc(DataType dataType)
{
    if (dataType == TYPE_FLOAT)
        return evalArrayCoordsFloat;
    else if (dataType == TYPE_FLOAT_VEC2)
        return evalArrayCoordsVec2;
    else if (dataType == TYPE_FLOAT_VEC3)
        return evalArrayCoordsVec3;
    else if (dataType == TYPE_FLOAT_VEC4)
        return evalArrayCoordsVec4;

    DE_FATAL("Invalid data type.");
    return NULL;
}

void evalArrayUniformFloat(ShaderEvalContext &c)
{
    c.color.x() = 1.875f * c.constCoords.x();
}
void evalArrayUniformVec2(ShaderEvalContext &c)
{
    c.color.xy() = 1.875f * c.constCoords.swizzle(0, 1);
}
void evalArrayUniformVec3(ShaderEvalContext &c)
{
    c.color.xyz() = 1.875f * c.constCoords.swizzle(0, 1, 2);
}
void evalArrayUniformVec4(ShaderEvalContext &c)
{
    c.color = 1.875f * c.constCoords;
}

static ShaderEvalFunc getArrayUniformEvalFunc(DataType dataType)
{
    if (dataType == TYPE_FLOAT)
        return evalArrayUniformFloat;
    else if (dataType == TYPE_FLOAT_VEC2)
        return evalArrayUniformVec2;
    else if (dataType == TYPE_FLOAT_VEC3)
        return evalArrayUniformVec3;
    else if (dataType == TYPE_FLOAT_VEC4)
        return evalArrayUniformVec4;

    DE_FATAL("Invalid data type.");
    return NULL;
}

// ShaderIndexingCase

class ShaderIndexingCase : public ShaderRenderCase
{
public:
    ShaderIndexingCase(Context &context, const char *name, const char *description, bool isVertexCase, DataType varType,
                       ShaderEvalFunc evalFunc, uint32_t requirements, const char *vertShaderSource,
                       const char *fragShaderSource);
    virtual ~ShaderIndexingCase(void);

    virtual void init(void);

private:
    ShaderIndexingCase(const ShaderIndexingCase &);            // not allowed!
    ShaderIndexingCase &operator=(const ShaderIndexingCase &); // not allowed!

    virtual void setup(int programID);
    virtual void setupUniforms(int programID, const Vec4 &constCoords);

    DataType m_varType;
    uint32_t m_requirements;
};

ShaderIndexingCase::ShaderIndexingCase(Context &context, const char *name, const char *description, bool isVertexCase,
                                       DataType varType, ShaderEvalFunc evalFunc, uint32_t requirements,
                                       const char *vertShaderSource, const char *fragShaderSource)
    : ShaderRenderCase(context.getTestContext(), context.getRenderContext(), context.getContextInfo(), name,
                       description, isVertexCase, evalFunc, true /*useLevel*/)
    , m_requirements(requirements)
{
    m_varType          = varType;
    m_vertShaderSource = vertShaderSource;
    m_fragShaderSource = fragShaderSource;
}

ShaderIndexingCase::~ShaderIndexingCase(void)
{
}

void ShaderIndexingCase::init(void)
{
    const bool isSupported =
        !(m_requirements & REQUIREMENT_UNIFORM_INDEXING) &&
        (!(m_requirements & REQUIREMENT_VERTEX_UNIFORM_LOOPS) || m_ctxInfo.isVertexUniformLoopSupported()) &&
        (!(m_requirements & REQUIREMENT_FRAGMENT_UNIFORM_LOOPS) || m_ctxInfo.isFragmentUniformLoopSupported());

    try
    {
        ShaderRenderCase::init();
    }
    catch (const CompileFailed &)
    {
        if (!isSupported)
            throw tcu::NotSupportedError("Shader is not supported");
        else
            throw;
    }
}

void ShaderIndexingCase::setup(int programID)
{
    DE_UNREF(programID);
}

void ShaderIndexingCase::setupUniforms(int programID, const Vec4 &constCoords)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    DE_UNREF(constCoords);

    int arrLoc = gl.getUniformLocation(programID, "u_arr");
    if (arrLoc != -1)
    {
        //int scalarSize = getDataTypeScalarSize(m_varType);
        if (m_varType == TYPE_FLOAT)
        {
            float arr[4];
            arr[0] = constCoords.x();
            arr[1] = constCoords.x() * 0.5f;
            arr[2] = constCoords.x() * 0.25f;
            arr[3] = constCoords.x() * 0.125f;
            gl.uniform1fv(arrLoc, 4, &arr[0]);
        }
        else if (m_varType == TYPE_FLOAT_VEC2)
        {
            Vec2 arr[4];
            arr[0] = constCoords.swizzle(0, 1);
            arr[1] = constCoords.swizzle(0, 1) * 0.5f;
            arr[2] = constCoords.swizzle(0, 1) * 0.25f;
            arr[3] = constCoords.swizzle(0, 1) * 0.125f;
            gl.uniform2fv(arrLoc, 4, arr[0].getPtr());
        }
        else if (m_varType == TYPE_FLOAT_VEC3)
        {
            Vec3 arr[4];
            arr[0] = constCoords.swizzle(0, 1, 2);
            arr[1] = constCoords.swizzle(0, 1, 2) * 0.5f;
            arr[2] = constCoords.swizzle(0, 1, 2) * 0.25f;
            arr[3] = constCoords.swizzle(0, 1, 2) * 0.125f;
            gl.uniform3fv(arrLoc, 4, arr[0].getPtr());
        }
        else if (m_varType == TYPE_FLOAT_VEC4)
        {
            Vec4 arr[4];
            arr[0] = constCoords.swizzle(0, 1, 2, 3);
            arr[1] = constCoords.swizzle(0, 1, 2, 3) * 0.5f;
            arr[2] = constCoords.swizzle(0, 1, 2, 3) * 0.25f;
            arr[3] = constCoords.swizzle(0, 1, 2, 3) * 0.125f;
            gl.uniform4fv(arrLoc, 4, arr[0].getPtr());
        }
        else
            throw tcu::TestError("u_arr should not have location assigned in this test case");
    }
}

// Helpers.

static ShaderIndexingCase *createVaryingArrayCase(Context &context, const char *caseName, const char *description,
                                                  DataType varType, IndexAccessType vertAccess,
                                                  IndexAccessType fragAccess)
{
    std::ostringstream vtx;
    vtx << "attribute highp vec4 a_position;\n";
    vtx << "attribute highp vec4 a_coords;\n";
    if (vertAccess == INDEXACCESS_DYNAMIC)
        vtx << "uniform mediump int ui_zero, ui_one, ui_two, ui_three;\n";
    else if (vertAccess == INDEXACCESS_DYNAMIC_LOOP)
        vtx << "uniform mediump int ui_four;\n";
    vtx << "varying ${PRECISION} ${VAR_TYPE} var[${ARRAY_LEN}];\n";
    vtx << "\n";
    vtx << "void main()\n";
    vtx << "{\n";
    vtx << "    gl_Position = a_position;\n";
    if (vertAccess == INDEXACCESS_STATIC)
    {
        vtx << "    var[0] = ${VAR_TYPE}(a_coords);\n";
        vtx << "    var[1] = ${VAR_TYPE}(a_coords) * 0.5;\n";
        vtx << "    var[2] = ${VAR_TYPE}(a_coords) * 0.25;\n";
        vtx << "    var[3] = ${VAR_TYPE}(a_coords) * 0.125;\n";
    }
    else if (vertAccess == INDEXACCESS_DYNAMIC)
    {
        vtx << "    var[ui_zero]  = ${VAR_TYPE}(a_coords);\n";
        vtx << "    var[ui_one]   = ${VAR_TYPE}(a_coords) * 0.5;\n";
        vtx << "    var[ui_two]   = ${VAR_TYPE}(a_coords) * 0.25;\n";
        vtx << "    var[ui_three] = ${VAR_TYPE}(a_coords) * 0.125;\n";
    }
    else if (vertAccess == INDEXACCESS_STATIC_LOOP)
    {
        vtx << "    ${PRECISION} ${VAR_TYPE} coords = ${VAR_TYPE}(a_coords);\n";
        vtx << "    for (int i = 0; i < 4; i++)\n";
        vtx << "    {\n";
        vtx << "        var[i] = ${VAR_TYPE}(coords);\n";
        vtx << "        coords = coords * 0.5;\n";
        vtx << "    }\n";
    }
    else
    {
        DE_ASSERT(vertAccess == INDEXACCESS_DYNAMIC_LOOP);
        vtx << "    ${PRECISION} ${VAR_TYPE} coords = ${VAR_TYPE}(a_coords);\n";
        vtx << "    for (int i = 0; i < ui_four; i++)\n";
        vtx << "    {\n";
        vtx << "        var[i] = ${VAR_TYPE}(coords);\n";
        vtx << "        coords = coords * 0.5;\n";
        vtx << "    }\n";
    }
    vtx << "}\n";

    std::ostringstream frag;
    frag << "precision mediump int;\n";
    if (fragAccess == INDEXACCESS_DYNAMIC)
        frag << "uniform mediump int ui_zero, ui_one, ui_two, ui_three;\n";
    else if (fragAccess == INDEXACCESS_DYNAMIC_LOOP)
        frag << "uniform int ui_four;\n";
    frag << "varying ${PRECISION} ${VAR_TYPE} var[${ARRAY_LEN}];\n";
    frag << "\n";
    frag << "void main()\n";
    frag << "{\n";
    frag << "    ${PRECISION} ${VAR_TYPE} res = ${VAR_TYPE}(0.0);\n";
    if (fragAccess == INDEXACCESS_STATIC)
    {
        frag << "    res += var[0];\n";
        frag << "    res += var[1];\n";
        frag << "    res += var[2];\n";
        frag << "    res += var[3];\n";
    }
    else if (fragAccess == INDEXACCESS_DYNAMIC)
    {
        frag << "    res += var[ui_zero];\n";
        frag << "    res += var[ui_one];\n";
        frag << "    res += var[ui_two];\n";
        frag << "    res += var[ui_three];\n";
    }
    else if (fragAccess == INDEXACCESS_STATIC_LOOP)
    {
        frag << "    for (int i = 0; i < 4; i++)\n";
        frag << "        res += var[i];\n";
    }
    else
    {
        DE_ASSERT(fragAccess == INDEXACCESS_DYNAMIC_LOOP);
        frag << "    for (int i = 0; i < ui_four; i++)\n";
        frag << "        res += var[i];\n";
    }
    frag << "    gl_FragColor = vec4(res${PADDING});\n";
    frag << "}\n";

    // Fill in shader templates.
    map<string, string> params;
    params.insert(pair<string, string>("VAR_TYPE", getDataTypeName(varType)));
    params.insert(pair<string, string>("ARRAY_LEN", "4"));
    params.insert(pair<string, string>("PRECISION", "mediump"));

    if (varType == TYPE_FLOAT)
        params.insert(pair<string, string>("PADDING", ", 0.0, 0.0, 1.0"));
    else if (varType == TYPE_FLOAT_VEC2)
        params.insert(pair<string, string>("PADDING", ", 0.0, 1.0"));
    else if (varType == TYPE_FLOAT_VEC3)
        params.insert(pair<string, string>("PADDING", ", 1.0"));
    else
        params.insert(pair<string, string>("PADDING", ""));

    StringTemplate vertTemplate(vtx.str().c_str());
    StringTemplate fragTemplate(frag.str().c_str());
    string vertexShaderSource   = vertTemplate.specialize(params);
    string fragmentShaderSource = fragTemplate.specialize(params);

    ShaderEvalFunc evalFunc = getArrayCoordsEvalFunc(varType);
    uint32_t requirements   = 0;

    if (vertAccess == INDEXACCESS_DYNAMIC || fragAccess == INDEXACCESS_DYNAMIC)
        requirements |= REQUIREMENT_UNIFORM_INDEXING;

    if (vertAccess == INDEXACCESS_DYNAMIC_LOOP)
        requirements |= REQUIREMENT_VERTEX_UNIFORM_LOOPS | REQUIREMENT_UNIFORM_INDEXING;

    if (fragAccess == INDEXACCESS_DYNAMIC_LOOP)
        requirements |= REQUIREMENT_FRAGMENT_UNIFORM_LOOPS | REQUIREMENT_UNIFORM_INDEXING;

    return new ShaderIndexingCase(context, caseName, description, true, varType, evalFunc, requirements,
                                  vertexShaderSource.c_str(), fragmentShaderSource.c_str());
}

static ShaderIndexingCase *createUniformArrayCase(Context &context, const char *caseName, const char *description,
                                                  bool isVertexCase, DataType varType, IndexAccessType readAccess)
{
    std::ostringstream vtx;
    std::ostringstream frag;
    std::ostringstream &op = isVertexCase ? vtx : frag;

    vtx << "attribute highp vec4 a_position;\n";
    vtx << "attribute highp vec4 a_coords;\n";

    if (isVertexCase)
    {
        vtx << "varying mediump vec4 v_color;\n";
        frag << "varying mediump vec4 v_color;\n";
    }
    else
    {
        vtx << "varying mediump vec4 v_coords;\n";
        frag << "varying mediump vec4 v_coords;\n";
    }

    if (readAccess == INDEXACCESS_DYNAMIC)
        op << "uniform mediump int ui_zero, ui_one, ui_two, ui_three;\n";
    else if (readAccess == INDEXACCESS_DYNAMIC_LOOP)
        op << "uniform mediump int ui_four;\n";

    op << "uniform ${PRECISION} ${VAR_TYPE} u_arr[${ARRAY_LEN}];\n";

    vtx << "\n";
    vtx << "void main()\n";
    vtx << "{\n";
    vtx << "    gl_Position = a_position;\n";

    frag << "\n";
    frag << "void main()\n";
    frag << "{\n";

    // Read array.
    op << "    ${PRECISION} ${VAR_TYPE} res = ${VAR_TYPE}(0.0);\n";
    if (readAccess == INDEXACCESS_STATIC)
    {
        op << "    res += u_arr[0];\n";
        op << "    res += u_arr[1];\n";
        op << "    res += u_arr[2];\n";
        op << "    res += u_arr[3];\n";
    }
    else if (readAccess == INDEXACCESS_DYNAMIC)
    {
        op << "    res += u_arr[ui_zero];\n";
        op << "    res += u_arr[ui_one];\n";
        op << "    res += u_arr[ui_two];\n";
        op << "    res += u_arr[ui_three];\n";
    }
    else if (readAccess == INDEXACCESS_STATIC_LOOP)
    {
        op << "    for (int i = 0; i < 4; i++)\n";
        op << "        res += u_arr[i];\n";
    }
    else
    {
        DE_ASSERT(readAccess == INDEXACCESS_DYNAMIC_LOOP);
        op << "    for (int i = 0; i < ui_four; i++)\n";
        op << "        res += u_arr[i];\n";
    }

    if (isVertexCase)
    {
        vtx << "    v_color = vec4(res${PADDING});\n";
        frag << "    gl_FragColor = v_color;\n";
    }
    else
    {
        vtx << "    v_coords = a_coords;\n";
        frag << "    gl_FragColor = vec4(res${PADDING});\n";
    }

    vtx << "}\n";
    frag << "}\n";

    // Fill in shader templates.
    map<string, string> params;
    params.insert(pair<string, string>("VAR_TYPE", getDataTypeName(varType)));
    params.insert(pair<string, string>("ARRAY_LEN", "4"));
    params.insert(pair<string, string>("PRECISION", "mediump"));

    if (varType == TYPE_FLOAT)
        params.insert(pair<string, string>("PADDING", ", 0.0, 0.0, 1.0"));
    else if (varType == TYPE_FLOAT_VEC2)
        params.insert(pair<string, string>("PADDING", ", 0.0, 1.0"));
    else if (varType == TYPE_FLOAT_VEC3)
        params.insert(pair<string, string>("PADDING", ", 1.0"));
    else
        params.insert(pair<string, string>("PADDING", ""));

    StringTemplate vertTemplate(vtx.str().c_str());
    StringTemplate fragTemplate(frag.str().c_str());
    string vertexShaderSource   = vertTemplate.specialize(params);
    string fragmentShaderSource = fragTemplate.specialize(params);

    ShaderEvalFunc evalFunc = getArrayUniformEvalFunc(varType);
    uint32_t requirements   = 0;

    if (readAccess == INDEXACCESS_DYNAMIC)
        requirements |= REQUIREMENT_UNIFORM_INDEXING;

    if (readAccess == INDEXACCESS_DYNAMIC_LOOP)
        requirements |= (isVertexCase ? REQUIREMENT_VERTEX_UNIFORM_LOOPS : REQUIREMENT_FRAGMENT_UNIFORM_LOOPS) |
                        REQUIREMENT_UNIFORM_INDEXING;

    return new ShaderIndexingCase(context, caseName, description, isVertexCase, varType, evalFunc, requirements,
                                  vertexShaderSource.c_str(), fragmentShaderSource.c_str());
}

static ShaderIndexingCase *createTmpArrayCase(Context &context, const char *caseName, const char *description,
                                              bool isVertexCase, DataType varType, IndexAccessType writeAccess,
                                              IndexAccessType readAccess)
{
    std::ostringstream vtx;
    std::ostringstream frag;
    std::ostringstream &op = isVertexCase ? vtx : frag;

    vtx << "attribute highp vec4 a_position;\n";
    vtx << "attribute highp vec4 a_coords;\n";

    if (isVertexCase)
    {
        vtx << "varying mediump vec4 v_color;\n";
        frag << "varying mediump vec4 v_color;\n";
    }
    else if (writeAccess != INDEXACCESS_CONST)
    {
        vtx << "varying mediump vec4 v_coords;\n";
        frag << "varying mediump vec4 v_coords;\n";
    }

    if (writeAccess == INDEXACCESS_DYNAMIC || readAccess == INDEXACCESS_DYNAMIC)
        op << "uniform mediump int ui_zero, ui_one, ui_two, ui_three;\n";

    if (writeAccess == INDEXACCESS_DYNAMIC_LOOP || readAccess == INDEXACCESS_DYNAMIC_LOOP)
        op << "uniform mediump int ui_four;\n";

    vtx << "\n";
    vtx << "void main()\n";
    vtx << "{\n";
    vtx << "    gl_Position = a_position;\n";

    frag << "\n";
    frag << "void main()\n";
    frag << "{\n";

    // Write array.
    if (writeAccess != INDEXACCESS_CONST)
    {
        if (isVertexCase)
            op << "    ${PRECISION} ${VAR_TYPE} coords = ${VAR_TYPE}(a_coords);\n";
        else
            op << "    ${PRECISION} ${VAR_TYPE} coords = ${VAR_TYPE}(v_coords);\n";
    }

    op << "    ${PRECISION} ${VAR_TYPE} arr[${ARRAY_LEN}];\n";
    if (writeAccess == INDEXACCESS_STATIC)
    {
        op << "    arr[0] = ${VAR_TYPE}(coords);\n";
        op << "    arr[1] = ${VAR_TYPE}(coords) * 0.5;\n";
        op << "    arr[2] = ${VAR_TYPE}(coords) * 0.25;\n";
        op << "    arr[3] = ${VAR_TYPE}(coords) * 0.125;\n";
    }
    else if (writeAccess == INDEXACCESS_CONST)
    {
        // Not using a loop inside the shader because we want it
        // unrolled to encourage the shader compiler to store it as
        // constant data.
        static const char *constructors[] = {"0.125", "0.125, 0.25", "0.125, 0.25, 0.5", "0.125, 0.25, 0.5, 1.0"};
        const char *constructor_args      = constructors[getDataTypeNumComponents(varType) - 1];

        op << "    arr[0] = ${VAR_TYPE}(" << constructor_args << ");\n";
        op << "    arr[1] = ${VAR_TYPE}(" << constructor_args << ") * 0.5;\n";
        op << "    arr[2] = ${VAR_TYPE}(" << constructor_args << ") * 0.25;\n";
        op << "    arr[3] = ${VAR_TYPE}(" << constructor_args << ") * 0.125;\n";

        /* Stuff unused values in the rest of the array. */
        op << "    int i = 4;\n";
        for (int i = 4; i < 40; i++)
            op << "    arr[i++] = ${VAR_TYPE}(" << i << ".0);\n";
    }
    else if (writeAccess == INDEXACCESS_DYNAMIC)
    {
        op << "    arr[ui_zero]  = ${VAR_TYPE}(coords);\n";
        op << "    arr[ui_one]   = ${VAR_TYPE}(coords) * 0.5;\n";
        op << "    arr[ui_two]   = ${VAR_TYPE}(coords) * 0.25;\n";
        op << "    arr[ui_three] = ${VAR_TYPE}(coords) * 0.125;\n";
    }
    else if (writeAccess == INDEXACCESS_STATIC_LOOP)
    {
        op << "    for (int i = 0; i < 4; i++)\n";
        op << "    {\n";
        op << "        arr[i] = ${VAR_TYPE}(coords);\n";
        op << "        coords = coords * 0.5;\n";
        op << "    }\n";
    }
    else
    {
        DE_ASSERT(writeAccess == INDEXACCESS_DYNAMIC_LOOP);
        op << "    for (int i = 0; i < ui_four; i++)\n";
        op << "    {\n";
        op << "        arr[i] = ${VAR_TYPE}(coords);\n";
        op << "        coords = coords * 0.5;\n";
        op << "    }\n";
    }

    // Read array.
    op << "    ${PRECISION} ${VAR_TYPE} res = ${VAR_TYPE}(0.0);\n";
    if (readAccess == INDEXACCESS_STATIC)
    {
        op << "    res += arr[0];\n";
        op << "    res += arr[1];\n";
        op << "    res += arr[2];\n";
        op << "    res += arr[3];\n";
    }
    else if (readAccess == INDEXACCESS_DYNAMIC)
    {
        op << "    res += arr[ui_zero];\n";
        op << "    res += arr[ui_one];\n";
        op << "    res += arr[ui_two];\n";
        op << "    res += arr[ui_three];\n";
    }
    else if (readAccess == INDEXACCESS_STATIC_LOOP)
    {
        op << "    for (int i = 0; i < 4; i++)\n";
        op << "        res += arr[i];\n";
    }
    else
    {
        DE_ASSERT(readAccess == INDEXACCESS_DYNAMIC_LOOP);
        op << "    for (int i = 0; i < ui_four; i++)\n";
        op << "        res += arr[i];\n";
    }

    if (isVertexCase)
    {
        vtx << "    v_color = vec4(res${PADDING});\n";
        frag << "    gl_FragColor = v_color;\n";
    }
    else
    {
        if (writeAccess != INDEXACCESS_CONST)
            vtx << "    v_coords = a_coords;\n";
        frag << "    gl_FragColor = vec4(res${PADDING});\n";
    }

    vtx << "}\n";
    frag << "}\n";

    // Fill in shader templates.
    map<string, string> params;
    params.insert(pair<string, string>("VAR_TYPE", getDataTypeName(varType)));
    // For const indexing, size the array such that the compiler is
    // more likely to optimize the temporary to constants.  4 wasn't
    // enough for Mesa's i965 driver to do it, while 40 was enough to
    // trigger the pass, and also enough to trigger compile failures
    // on the freedreno driver at vec3/vec4 without the optimization
    // in place.
    if (writeAccess == INDEXACCESS_CONST)
        params.insert(pair<string, string>("ARRAY_LEN", "40"));
    else
        params.insert(pair<string, string>("ARRAY_LEN", "4"));
    params.insert(pair<string, string>("PRECISION", "mediump"));

    if (varType == TYPE_FLOAT)
        params.insert(pair<string, string>("PADDING", ", 0.0, 0.0, 1.0"));
    else if (varType == TYPE_FLOAT_VEC2)
        params.insert(pair<string, string>("PADDING", ", 0.0, 1.0"));
    else if (varType == TYPE_FLOAT_VEC3)
        params.insert(pair<string, string>("PADDING", ", 1.0"));
    else
        params.insert(pair<string, string>("PADDING", ""));

    StringTemplate vertTemplate(vtx.str().c_str());
    StringTemplate fragTemplate(frag.str().c_str());
    string vertexShaderSource   = vertTemplate.specialize(params);
    string fragmentShaderSource = fragTemplate.specialize(params);

    ShaderEvalFunc evalFunc;
    if (writeAccess == INDEXACCESS_CONST)
        evalFunc = getArrayUniformEvalFunc(varType);
    else
        evalFunc = getArrayCoordsEvalFunc(varType);
    uint32_t requirements = 0;

    if (readAccess == INDEXACCESS_DYNAMIC || writeAccess == INDEXACCESS_DYNAMIC)
        requirements |= REQUIREMENT_UNIFORM_INDEXING;

    if (readAccess == INDEXACCESS_DYNAMIC_LOOP || writeAccess == INDEXACCESS_DYNAMIC_LOOP)
        requirements |= (isVertexCase ? REQUIREMENT_VERTEX_UNIFORM_LOOPS : REQUIREMENT_FRAGMENT_UNIFORM_LOOPS) |
                        REQUIREMENT_UNIFORM_INDEXING;

    return new ShaderIndexingCase(context, caseName, description, isVertexCase, varType, evalFunc, requirements,
                                  vertexShaderSource.c_str(), fragmentShaderSource.c_str());
}

// VECTOR SUBSCRIPT.

void evalSubscriptVec2(ShaderEvalContext &c)
{
    c.color.xyz() = Vec3(c.coords.x() + 0.5f * c.coords.y());
}
void evalSubscriptVec3(ShaderEvalContext &c)
{
    c.color.xyz() = Vec3(c.coords.x() + 0.5f * c.coords.y() + 0.25f * c.coords.z());
}
void evalSubscriptVec4(ShaderEvalContext &c)
{
    c.color.xyz() = Vec3(c.coords.x() + 0.5f * c.coords.y() + 0.25f * c.coords.z() + 0.125f * c.coords.w());
}

static ShaderEvalFunc getVectorSubscriptEvalFunc(DataType dataType)
{
    if (dataType == TYPE_FLOAT_VEC2)
        return evalSubscriptVec2;
    else if (dataType == TYPE_FLOAT_VEC3)
        return evalSubscriptVec3;
    else if (dataType == TYPE_FLOAT_VEC4)
        return evalSubscriptVec4;

    DE_FATAL("Invalid data type.");
    return NULL;
}

static ShaderIndexingCase *createVectorSubscriptCase(Context &context, const char *caseName, const char *description,
                                                     bool isVertexCase, DataType varType, VectorAccessType writeAccess,
                                                     VectorAccessType readAccess)
{
    std::ostringstream vtx;
    std::ostringstream frag;
    std::ostringstream &op = isVertexCase ? vtx : frag;

    int vecLen             = getDataTypeScalarSize(varType);
    const char *vecLenName = getIntUniformName(vecLen);

    vtx << "attribute highp vec4 a_position;\n";
    vtx << "attribute highp vec4 a_coords;\n";

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

    if (writeAccess == SUBSCRIPT_DYNAMIC || readAccess == SUBSCRIPT_DYNAMIC)
    {
        op << "uniform mediump int ui_zero";
        if (vecLen >= 2)
            op << ", ui_one";
        if (vecLen >= 3)
            op << ", ui_two";
        if (vecLen >= 4)
            op << ", ui_three";
        op << ";\n";
    }

    if (writeAccess == SUBSCRIPT_DYNAMIC_LOOP || readAccess == SUBSCRIPT_DYNAMIC_LOOP)
        op << "uniform mediump int " << vecLenName << ";\n";

    vtx << "\n";
    vtx << "void main()\n";
    vtx << "{\n";
    vtx << "    gl_Position = a_position;\n";

    frag << "\n";
    frag << "void main()\n";
    frag << "{\n";

    // Write vector.
    if (isVertexCase)
        op << "    ${PRECISION} ${VAR_TYPE} coords = ${VAR_TYPE}(a_coords);\n";
    else
        op << "    ${PRECISION} ${VAR_TYPE} coords = ${VAR_TYPE}(v_coords);\n";

    op << "    ${PRECISION} ${VAR_TYPE} tmp;\n";
    if (writeAccess == DIRECT)
        op << "    tmp = coords.${SWIZZLE} * vec4(1.0, 0.5, 0.25, 0.125).${SWIZZLE};\n";
    else if (writeAccess == COMPONENT)
    {
        op << "    tmp.x = coords.x;\n";
        if (vecLen >= 2)
            op << "    tmp.y = coords.y * 0.5;\n";
        if (vecLen >= 3)
            op << "    tmp.z = coords.z * 0.25;\n";
        if (vecLen >= 4)
            op << "    tmp.w = coords.w * 0.125;\n";
    }
    else if (writeAccess == SUBSCRIPT_STATIC)
    {
        op << "    tmp[0] = coords.x;\n";
        if (vecLen >= 2)
            op << "    tmp[1] = coords.y * 0.5;\n";
        if (vecLen >= 3)
            op << "    tmp[2] = coords.z * 0.25;\n";
        if (vecLen >= 4)
            op << "    tmp[3] = coords.w * 0.125;\n";
    }
    else if (writeAccess == SUBSCRIPT_DYNAMIC)
    {
        op << "    tmp[ui_zero]  = coords.x;\n";
        if (vecLen >= 2)
            op << "    tmp[ui_one]   = coords.y * 0.5;\n";
        if (vecLen >= 3)
            op << "    tmp[ui_two]   = coords.z * 0.25;\n";
        if (vecLen >= 4)
            op << "    tmp[ui_three] = coords.w * 0.125;\n";
    }
    else if (writeAccess == SUBSCRIPT_STATIC_LOOP)
    {
        op << "    for (int i = 0; i < " << vecLen << "; i++)\n";
        op << "    {\n";
        op << "        tmp[i] = coords.x;\n";
        op << "        coords = coords.${ROT_SWIZZLE} * 0.5;\n";
        op << "    }\n";
    }
    else
    {
        DE_ASSERT(writeAccess == SUBSCRIPT_DYNAMIC_LOOP);
        op << "    for (int i = 0; i < " << vecLenName << "; i++)\n";
        op << "    {\n";
        op << "        tmp[i] = coords.x;\n";
        op << "        coords = coords.${ROT_SWIZZLE} * 0.5;\n";
        op << "    }\n";
    }

    // Read vector.
    op << "    ${PRECISION} float res = 0.0;\n";
    if (readAccess == DIRECT)
        op << "    res = dot(tmp, ${VAR_TYPE}(1.0));\n";
    else if (readAccess == COMPONENT)
    {
        op << "    res += tmp.x;\n";
        if (vecLen >= 2)
            op << "    res += tmp.y;\n";
        if (vecLen >= 3)
            op << "    res += tmp.z;\n";
        if (vecLen >= 4)
            op << "    res += tmp.w;\n";
    }
    else if (readAccess == SUBSCRIPT_STATIC)
    {
        op << "    res += tmp[0];\n";
        if (vecLen >= 2)
            op << "    res += tmp[1];\n";
        if (vecLen >= 3)
            op << "    res += tmp[2];\n";
        if (vecLen >= 4)
            op << "    res += tmp[3];\n";
    }
    else if (readAccess == SUBSCRIPT_DYNAMIC)
    {
        op << "    res += tmp[ui_zero];\n";
        if (vecLen >= 2)
            op << "    res += tmp[ui_one];\n";
        if (vecLen >= 3)
            op << "    res += tmp[ui_two];\n";
        if (vecLen >= 4)
            op << "    res += tmp[ui_three];\n";
    }
    else if (readAccess == SUBSCRIPT_STATIC_LOOP)
    {
        op << "    for (int i = 0; i < " << vecLen << "; i++)\n";
        op << "        res += tmp[i];\n";
    }
    else
    {
        DE_ASSERT(readAccess == SUBSCRIPT_DYNAMIC_LOOP);
        op << "    for (int i = 0; i < " << vecLenName << "; i++)\n";
        op << "        res += tmp[i];\n";
    }

    if (isVertexCase)
    {
        vtx << "    v_color = vec3(res);\n";
        frag << "    gl_FragColor = vec4(v_color, 1.0);\n";
    }
    else
    {
        vtx << "    v_coords = a_coords;\n";
        frag << "    gl_FragColor = vec4(vec3(res), 1.0);\n";
    }

    vtx << "}\n";
    frag << "}\n";

    // Fill in shader templates.
    static const char *s_swizzles[5]    = {"", "x", "xy", "xyz", "xyzw"};
    static const char *s_rotSwizzles[5] = {"", "x", "yx", "yzx", "yzwx"};

    map<string, string> params;
    params.insert(pair<string, string>("VAR_TYPE", getDataTypeName(varType)));
    params.insert(pair<string, string>("PRECISION", "mediump"));
    params.insert(pair<string, string>("SWIZZLE", s_swizzles[vecLen]));
    params.insert(pair<string, string>("ROT_SWIZZLE", s_rotSwizzles[vecLen]));

    StringTemplate vertTemplate(vtx.str().c_str());
    StringTemplate fragTemplate(frag.str().c_str());
    string vertexShaderSource   = vertTemplate.specialize(params);
    string fragmentShaderSource = fragTemplate.specialize(params);

    ShaderEvalFunc evalFunc = getVectorSubscriptEvalFunc(varType);
    uint32_t requirements   = 0;

    if (readAccess == SUBSCRIPT_DYNAMIC || writeAccess == SUBSCRIPT_DYNAMIC)
        requirements |= REQUIREMENT_UNIFORM_INDEXING;

    if (readAccess == SUBSCRIPT_DYNAMIC_LOOP || writeAccess == SUBSCRIPT_DYNAMIC_LOOP)
        requirements |= (isVertexCase ? REQUIREMENT_VERTEX_UNIFORM_LOOPS : REQUIREMENT_FRAGMENT_UNIFORM_LOOPS) |
                        REQUIREMENT_UNIFORM_INDEXING;

    return new ShaderIndexingCase(context, caseName, description, isVertexCase, varType, evalFunc, requirements,
                                  vertexShaderSource.c_str(), fragmentShaderSource.c_str());
}

// MATRIX SUBSCRIPT.

void evalSubscriptMat2(ShaderEvalContext &c)
{
    c.color.xy() = c.coords.swizzle(0, 1) + 0.5f * c.coords.swizzle(1, 2);
}
void evalSubscriptMat3(ShaderEvalContext &c)
{
    c.color.xyz() = c.coords.swizzle(0, 1, 2) + 0.5f * c.coords.swizzle(1, 2, 3) + 0.25f * c.coords.swizzle(2, 3, 0);
}
void evalSubscriptMat4(ShaderEvalContext &c)
{
    c.color = c.coords + 0.5f * c.coords.swizzle(1, 2, 3, 0) + 0.25f * c.coords.swizzle(2, 3, 0, 1) +
              0.125f * c.coords.swizzle(3, 0, 1, 2);
}

static ShaderEvalFunc getMatrixSubscriptEvalFunc(DataType dataType)
{
    if (dataType == TYPE_FLOAT_MAT2)
        return evalSubscriptMat2;
    else if (dataType == TYPE_FLOAT_MAT3)
        return evalSubscriptMat3;
    else if (dataType == TYPE_FLOAT_MAT4)
        return evalSubscriptMat4;

    DE_FATAL("Invalid data type.");
    return NULL;
}

static ShaderIndexingCase *createMatrixSubscriptCase(Context &context, const char *caseName, const char *description,
                                                     bool isVertexCase, DataType varType, IndexAccessType writeAccess,
                                                     IndexAccessType readAccess)
{
    std::ostringstream vtx;
    std::ostringstream frag;
    std::ostringstream &op = isVertexCase ? vtx : frag;

    int matSize             = getDataTypeMatrixNumRows(varType);
    const char *matSizeName = getIntUniformName(matSize);
    DataType vecType        = getDataTypeFloatVec(matSize);

    vtx << "attribute highp vec4 a_position;\n";
    vtx << "attribute highp vec4 a_coords;\n";

    if (isVertexCase)
    {
        vtx << "varying mediump vec4 v_color;\n";
        frag << "varying mediump vec4 v_color;\n";
    }
    else
    {
        vtx << "varying mediump vec4 v_coords;\n";
        frag << "varying mediump vec4 v_coords;\n";
    }

    if (writeAccess == INDEXACCESS_DYNAMIC || readAccess == INDEXACCESS_DYNAMIC)
    {
        op << "uniform mediump int ui_zero";
        if (matSize >= 2)
            op << ", ui_one";
        if (matSize >= 3)
            op << ", ui_two";
        if (matSize >= 4)
            op << ", ui_three";
        op << ";\n";
    }

    if (writeAccess == INDEXACCESS_DYNAMIC_LOOP || readAccess == INDEXACCESS_DYNAMIC_LOOP)
        op << "uniform mediump int " << matSizeName << ";\n";

    vtx << "\n";
    vtx << "void main()\n";
    vtx << "{\n";
    vtx << "    gl_Position = a_position;\n";

    frag << "\n";
    frag << "void main()\n";
    frag << "{\n";

    // Write matrix.
    if (isVertexCase)
        op << "    ${PRECISION} vec4 coords = a_coords;\n";
    else
        op << "    ${PRECISION} vec4 coords = v_coords;\n";

    op << "    ${PRECISION} ${MAT_TYPE} tmp;\n";
    if (writeAccess == INDEXACCESS_STATIC)
    {
        op << "    tmp[0] = ${VEC_TYPE}(coords);\n";
        if (matSize >= 2)
            op << "    tmp[1] = ${VEC_TYPE}(coords.yzwx) * 0.5;\n";
        if (matSize >= 3)
            op << "    tmp[2] = ${VEC_TYPE}(coords.zwxy) * 0.25;\n";
        if (matSize >= 4)
            op << "    tmp[3] = ${VEC_TYPE}(coords.wxyz) * 0.125;\n";
    }
    else if (writeAccess == INDEXACCESS_DYNAMIC)
    {
        op << "    tmp[ui_zero]  = ${VEC_TYPE}(coords);\n";
        if (matSize >= 2)
            op << "    tmp[ui_one]   = ${VEC_TYPE}(coords.yzwx) * 0.5;\n";
        if (matSize >= 3)
            op << "    tmp[ui_two]   = ${VEC_TYPE}(coords.zwxy) * 0.25;\n";
        if (matSize >= 4)
            op << "    tmp[ui_three] = ${VEC_TYPE}(coords.wxyz) * 0.125;\n";
    }
    else if (writeAccess == INDEXACCESS_STATIC_LOOP)
    {
        op << "    for (int i = 0; i < " << matSize << "; i++)\n";
        op << "    {\n";
        op << "        tmp[i] = ${VEC_TYPE}(coords);\n";
        op << "        coords = coords.yzwx * 0.5;\n";
        op << "    }\n";
    }
    else
    {
        DE_ASSERT(writeAccess == INDEXACCESS_DYNAMIC_LOOP);
        op << "    for (int i = 0; i < " << matSizeName << "; i++)\n";
        op << "    {\n";
        op << "        tmp[i] = ${VEC_TYPE}(coords);\n";
        op << "        coords = coords.yzwx * 0.5;\n";
        op << "    }\n";
    }

    // Read matrix.
    op << "    ${PRECISION} ${VEC_TYPE} res = ${VEC_TYPE}(0.0);\n";
    if (readAccess == INDEXACCESS_STATIC)
    {
        op << "    res += tmp[0];\n";
        if (matSize >= 2)
            op << "    res += tmp[1];\n";
        if (matSize >= 3)
            op << "    res += tmp[2];\n";
        if (matSize >= 4)
            op << "    res += tmp[3];\n";
    }
    else if (readAccess == INDEXACCESS_DYNAMIC)
    {
        op << "    res += tmp[ui_zero];\n";
        if (matSize >= 2)
            op << "    res += tmp[ui_one];\n";
        if (matSize >= 3)
            op << "    res += tmp[ui_two];\n";
        if (matSize >= 4)
            op << "    res += tmp[ui_three];\n";
    }
    else if (readAccess == INDEXACCESS_STATIC_LOOP)
    {
        op << "    for (int i = 0; i < " << matSize << "; i++)\n";
        op << "        res += tmp[i];\n";
    }
    else
    {
        DE_ASSERT(readAccess == INDEXACCESS_DYNAMIC_LOOP);
        op << "    for (int i = 0; i < " << matSizeName << "; i++)\n";
        op << "        res += tmp[i];\n";
    }

    if (isVertexCase)
    {
        vtx << "    v_color = vec4(res${PADDING});\n";
        frag << "    gl_FragColor = v_color;\n";
    }
    else
    {
        vtx << "    v_coords = a_coords;\n";
        frag << "    gl_FragColor = vec4(res${PADDING});\n";
    }

    vtx << "}\n";
    frag << "}\n";

    // Fill in shader templates.
    map<string, string> params;
    params.insert(pair<string, string>("MAT_TYPE", getDataTypeName(varType)));
    params.insert(pair<string, string>("VEC_TYPE", getDataTypeName(vecType)));
    params.insert(pair<string, string>("PRECISION", "mediump"));

    if (matSize == 2)
        params.insert(pair<string, string>("PADDING", ", 0.0, 1.0"));
    else if (matSize == 3)
        params.insert(pair<string, string>("PADDING", ", 1.0"));
    else
        params.insert(pair<string, string>("PADDING", ""));

    StringTemplate vertTemplate(vtx.str().c_str());
    StringTemplate fragTemplate(frag.str().c_str());
    string vertexShaderSource   = vertTemplate.specialize(params);
    string fragmentShaderSource = fragTemplate.specialize(params);

    ShaderEvalFunc evalFunc = getMatrixSubscriptEvalFunc(varType);
    uint32_t requirements   = 0;

    if (readAccess == INDEXACCESS_DYNAMIC || writeAccess == INDEXACCESS_DYNAMIC)
        requirements |= REQUIREMENT_UNIFORM_INDEXING;

    if (readAccess == INDEXACCESS_DYNAMIC_LOOP || writeAccess == INDEXACCESS_DYNAMIC_LOOP)
        requirements |= (isVertexCase ? REQUIREMENT_VERTEX_UNIFORM_LOOPS : REQUIREMENT_FRAGMENT_UNIFORM_LOOPS) |
                        REQUIREMENT_UNIFORM_INDEXING;

    return new ShaderIndexingCase(context, caseName, description, isVertexCase, varType, evalFunc, requirements,
                                  vertexShaderSource.c_str(), fragmentShaderSource.c_str());
}

// ShaderIndexingTests.

ShaderIndexingTests::ShaderIndexingTests(Context &context) : TestCaseGroup(context, "indexing", "Indexing Tests")
{
}

ShaderIndexingTests::~ShaderIndexingTests(void)
{
}

void ShaderIndexingTests::init(void)
{
    static const ShaderType s_shaderTypes[] = {SHADERTYPE_VERTEX, SHADERTYPE_FRAGMENT};

    static const DataType s_floatAndVecTypes[] = {TYPE_FLOAT, TYPE_FLOAT_VEC2, TYPE_FLOAT_VEC3, TYPE_FLOAT_VEC4};

    // Varying array access cases.
    {
        TestCaseGroup *varyingGroup = new TestCaseGroup(m_context, "varying_array", "Varying array access tests.");
        addChild(varyingGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(s_floatAndVecTypes); typeNdx++)
        {
            DataType varType = s_floatAndVecTypes[typeNdx];
            for (int vertAccess = 0; vertAccess < INDEXACCESS_CONST; vertAccess++)
            {
                for (int fragAccess = 0; fragAccess < INDEXACCESS_CONST; fragAccess++)
                {
                    const char *vertAccessName = getIndexAccessTypeName((IndexAccessType)vertAccess);
                    const char *fragAccessName = getIndexAccessTypeName((IndexAccessType)fragAccess);
                    string name =
                        string(getDataTypeName(varType)) + "_" + vertAccessName + "_write_" + fragAccessName + "_read";
                    string desc = string("Varying array with ") + vertAccessName + " write in vertex shader and " +
                                  fragAccessName + " read in fragment shader.";
                    varyingGroup->addChild(createVaryingArrayCase(m_context, name.c_str(), desc.c_str(), varType,
                                                                  (IndexAccessType)vertAccess,
                                                                  (IndexAccessType)fragAccess));
                }
            }
        }
    }

    // Uniform array access cases.
    {
        TestCaseGroup *uniformGroup = new TestCaseGroup(m_context, "uniform_array", "Uniform array access tests.");
        addChild(uniformGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(s_floatAndVecTypes); typeNdx++)
        {
            DataType varType = s_floatAndVecTypes[typeNdx];
            for (int readAccess = 0; readAccess < INDEXACCESS_CONST; readAccess++)
            {
                const char *readAccessName = getIndexAccessTypeName((IndexAccessType)readAccess);
                for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(s_shaderTypes); shaderTypeNdx++)
                {
                    ShaderType shaderType      = s_shaderTypes[shaderTypeNdx];
                    const char *shaderTypeName = getShaderTypeName(shaderType);
                    string name = string(getDataTypeName(varType)) + "_" + readAccessName + "_read_" + shaderTypeName;
                    string desc =
                        string("Uniform array with ") + readAccessName + " read in " + shaderTypeName + " shader.";
                    bool isVertexCase = ((ShaderType)shaderType == SHADERTYPE_VERTEX);
                    uniformGroup->addChild(createUniformArrayCase(m_context, name.c_str(), desc.c_str(), isVertexCase,
                                                                  varType, (IndexAccessType)readAccess));
                }
            }
        }
    }

    // Temporary array access cases.
    {
        TestCaseGroup *tmpGroup = new TestCaseGroup(m_context, "tmp_array", "Temporary array access tests.");
        addChild(tmpGroup);

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(s_floatAndVecTypes); typeNdx++)
        {
            DataType varType = s_floatAndVecTypes[typeNdx];
            for (int writeAccess = 0; writeAccess < INDEXACCESS_LAST; writeAccess++)
            {
                for (int readAccess = 0; readAccess < INDEXACCESS_CONST; readAccess++)
                {
                    const char *writeAccessName = getIndexAccessTypeName((IndexAccessType)writeAccess);
                    const char *readAccessName  = getIndexAccessTypeName((IndexAccessType)readAccess);

                    for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(s_shaderTypes); shaderTypeNdx++)
                    {
                        ShaderType shaderType      = s_shaderTypes[shaderTypeNdx];
                        const char *shaderTypeName = getShaderTypeName(shaderType);
                        string name = string(getDataTypeName(varType)) + "_" + writeAccessName + "_write_" +
                                      readAccessName + "_read_" + shaderTypeName;
                        string desc = string("Temporary array with ") + writeAccessName + " write and " +
                                      readAccessName + " read in " + shaderTypeName + " shader.";
                        bool isVertexCase = ((ShaderType)shaderType == SHADERTYPE_VERTEX);
                        tmpGroup->addChild(createTmpArrayCase(m_context, name.c_str(), desc.c_str(), isVertexCase,
                                                              varType, (IndexAccessType)writeAccess,
                                                              (IndexAccessType)readAccess));
                    }
                }
            }
        }
    }

    // Vector indexing with subscripts.
    {
        TestCaseGroup *vecGroup = new TestCaseGroup(m_context, "vector_subscript", "Vector subscript indexing.");
        addChild(vecGroup);

        static const DataType s_vectorTypes[] = {TYPE_FLOAT_VEC2, TYPE_FLOAT_VEC3, TYPE_FLOAT_VEC4};

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(s_vectorTypes); typeNdx++)
        {
            DataType varType = s_vectorTypes[typeNdx];
            for (int writeAccess = 0; writeAccess < VECTORACCESS_LAST; writeAccess++)
            {
                for (int readAccess = 0; readAccess < VECTORACCESS_LAST; readAccess++)
                {
                    const char *writeAccessName = getVectorAccessTypeName((VectorAccessType)writeAccess);
                    const char *readAccessName  = getVectorAccessTypeName((VectorAccessType)readAccess);

                    for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(s_shaderTypes); shaderTypeNdx++)
                    {
                        ShaderType shaderType      = s_shaderTypes[shaderTypeNdx];
                        const char *shaderTypeName = getShaderTypeName(shaderType);
                        string name = string(getDataTypeName(varType)) + "_" + writeAccessName + "_write_" +
                                      readAccessName + "_read_" + shaderTypeName;
                        string desc = string("Vector subscript access with ") + writeAccessName + " write and " +
                                      readAccessName + " read in " + shaderTypeName + " shader.";
                        bool isVertexCase = ((ShaderType)shaderType == SHADERTYPE_VERTEX);
                        vecGroup->addChild(
                            createVectorSubscriptCase(m_context, name.c_str(), desc.c_str(), isVertexCase, varType,
                                                      (VectorAccessType)writeAccess, (VectorAccessType)readAccess));
                    }
                }
            }
        }
    }

    // Matrix indexing with subscripts.
    {
        TestCaseGroup *matGroup = new TestCaseGroup(m_context, "matrix_subscript", "Matrix subscript indexing.");
        addChild(matGroup);

        static const DataType s_matrixTypes[] = {TYPE_FLOAT_MAT2, TYPE_FLOAT_MAT3, TYPE_FLOAT_MAT4};

        for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(s_matrixTypes); typeNdx++)
        {
            DataType varType = s_matrixTypes[typeNdx];
            for (int writeAccess = 0; writeAccess < INDEXACCESS_CONST; writeAccess++)
            {
                for (int readAccess = 0; readAccess < INDEXACCESS_CONST; readAccess++)
                {
                    const char *writeAccessName = getIndexAccessTypeName((IndexAccessType)writeAccess);
                    const char *readAccessName  = getIndexAccessTypeName((IndexAccessType)readAccess);

                    for (int shaderTypeNdx = 0; shaderTypeNdx < DE_LENGTH_OF_ARRAY(s_shaderTypes); shaderTypeNdx++)
                    {
                        ShaderType shaderType      = s_shaderTypes[shaderTypeNdx];
                        const char *shaderTypeName = getShaderTypeName(shaderType);
                        string name = string(getDataTypeName(varType)) + "_" + writeAccessName + "_write_" +
                                      readAccessName + "_read_" + shaderTypeName;
                        string desc = string("Vector subscript access with ") + writeAccessName + " write and " +
                                      readAccessName + " read in " + shaderTypeName + " shader.";
                        bool isVertexCase = ((ShaderType)shaderType == SHADERTYPE_VERTEX);
                        matGroup->addChild(
                            createMatrixSubscriptCase(m_context, name.c_str(), desc.c_str(), isVertexCase, varType,
                                                      (IndexAccessType)writeAccess, (IndexAccessType)readAccess));
                    }
                }
            }
        }
    }
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
