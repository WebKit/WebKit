/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief Reference context utils
 *//*--------------------------------------------------------------------*/

#include "sglrReferenceUtils.hpp"
#include "glwEnums.hpp"

namespace sglr
{
namespace rr_util
{

rr::VertexAttribType mapGLPureIntegerVertexAttributeType(uint32_t type)
{
    switch (type)
    {
    case GL_UNSIGNED_BYTE:
        return rr::VERTEXATTRIBTYPE_PURE_UINT8;
    case GL_UNSIGNED_SHORT:
        return rr::VERTEXATTRIBTYPE_PURE_UINT16;
    case GL_UNSIGNED_INT:
        return rr::VERTEXATTRIBTYPE_PURE_UINT32;
    case GL_BYTE:
        return rr::VERTEXATTRIBTYPE_PURE_INT8;
    case GL_SHORT:
        return rr::VERTEXATTRIBTYPE_PURE_INT16;
    case GL_INT:
        return rr::VERTEXATTRIBTYPE_PURE_INT32;
    default:
        DE_ASSERT(false);
        return rr::VERTEXATTRIBTYPE_LAST;
    }
}

rr::VertexAttribType mapGLFloatVertexAttributeType(uint32_t type, bool normalizedInteger, int size,
                                                   glu::ContextType ctxType)
{
    const bool useClampingNormalization = (ctxType.getProfile() == glu::PROFILE_ES && ctxType.getMajorVersion() >= 3) ||
                                          (ctxType.getMajorVersion() == 4 && ctxType.getMinorVersion() >= 2);
    const bool bgraComponentOrder = (size == GL_BGRA);

    switch (type)
    {
    case GL_FLOAT:
        return rr::VERTEXATTRIBTYPE_FLOAT;

    case GL_HALF_FLOAT:
        return rr::VERTEXATTRIBTYPE_HALF;

    case GL_FIXED:
        return rr::VERTEXATTRIBTYPE_FIXED;

    case GL_DOUBLE:
        return rr::VERTEXATTRIBTYPE_DOUBLE;

    case GL_UNSIGNED_BYTE:
        if (!normalizedInteger)
            return rr::VERTEXATTRIBTYPE_NONPURE_UINT8;
        else
            return (!bgraComponentOrder) ? (rr::VERTEXATTRIBTYPE_NONPURE_UNORM8) :
                                           (rr::VERTEXATTRIBTYPE_NONPURE_UNORM8_BGRA);

    case GL_UNSIGNED_SHORT:
        if (!normalizedInteger)
            return rr::VERTEXATTRIBTYPE_NONPURE_UINT16;
        else
            return rr::VERTEXATTRIBTYPE_NONPURE_UNORM16;

    case GL_UNSIGNED_INT:
        if (!normalizedInteger)
            return rr::VERTEXATTRIBTYPE_NONPURE_UINT32;
        else
            return rr::VERTEXATTRIBTYPE_NONPURE_UNORM32;

    case GL_UNSIGNED_INT_2_10_10_10_REV:
        if (!normalizedInteger)
            return rr::VERTEXATTRIBTYPE_NONPURE_UINT_2_10_10_10_REV;
        else
            return (!bgraComponentOrder) ? (rr::VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV) :
                                           (rr::VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV_BGRA);

    case GL_BYTE:
        if (!normalizedInteger)
            return rr::VERTEXATTRIBTYPE_NONPURE_INT8;
        else if (useClampingNormalization)
            return rr::VERTEXATTRIBTYPE_NONPURE_SNORM8_CLAMP;
        else
            return rr::VERTEXATTRIBTYPE_NONPURE_SNORM8_SCALE;

    case GL_SHORT:
        if (!normalizedInteger)
            return rr::VERTEXATTRIBTYPE_NONPURE_INT16;
        else if (useClampingNormalization)
            return rr::VERTEXATTRIBTYPE_NONPURE_SNORM16_CLAMP;
        else
            return rr::VERTEXATTRIBTYPE_NONPURE_SNORM16_SCALE;

    case GL_INT:
        if (!normalizedInteger)
            return rr::VERTEXATTRIBTYPE_NONPURE_INT32;
        else if (useClampingNormalization)
            return rr::VERTEXATTRIBTYPE_NONPURE_SNORM32_CLAMP;
        else
            return rr::VERTEXATTRIBTYPE_NONPURE_SNORM32_SCALE;

    case GL_INT_2_10_10_10_REV:
        if (!normalizedInteger)
            return rr::VERTEXATTRIBTYPE_NONPURE_INT_2_10_10_10_REV;
        else if (useClampingNormalization)
            return (!bgraComponentOrder) ? (rr::VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP) :
                                           (rr::VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP_BGRA);
        else
            return (!bgraComponentOrder) ? (rr::VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE) :
                                           (rr::VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE_BGRA);

    default:
        DE_ASSERT(false);
        return rr::VERTEXATTRIBTYPE_LAST;
    }
}

int mapGLSize(int size)
{
    switch (size)
    {
    case 1:
        return 1;
    case 2:
        return 2;
    case 3:
        return 3;
    case 4:
        return 4;
    case GL_BGRA:
        return 4;

    default:
        DE_ASSERT(false);
        return 0;
    }
}

rr::PrimitiveType mapGLPrimitiveType(uint32_t type)
{
    switch (type)
    {
    case GL_TRIANGLES:
        return rr::PRIMITIVETYPE_TRIANGLES;
    case GL_TRIANGLE_STRIP:
        return rr::PRIMITIVETYPE_TRIANGLE_STRIP;
    case GL_TRIANGLE_FAN:
        return rr::PRIMITIVETYPE_TRIANGLE_FAN;
    case GL_LINES:
        return rr::PRIMITIVETYPE_LINES;
    case GL_LINE_STRIP:
        return rr::PRIMITIVETYPE_LINE_STRIP;
    case GL_LINE_LOOP:
        return rr::PRIMITIVETYPE_LINE_LOOP;
    case GL_POINTS:
        return rr::PRIMITIVETYPE_POINTS;
    case GL_LINES_ADJACENCY:
        return rr::PRIMITIVETYPE_LINES_ADJACENCY;
    case GL_LINE_STRIP_ADJACENCY:
        return rr::PRIMITIVETYPE_LINE_STRIP_ADJACENCY;
    case GL_TRIANGLES_ADJACENCY:
        return rr::PRIMITIVETYPE_TRIANGLES_ADJACENCY;
    case GL_TRIANGLE_STRIP_ADJACENCY:
        return rr::PRIMITIVETYPE_TRIANGLE_STRIP_ADJACENCY;
    default:
        DE_ASSERT(false);
        return rr::PRIMITIVETYPE_LAST;
    }
}

rr::IndexType mapGLIndexType(uint32_t type)
{
    switch (type)
    {
    case GL_UNSIGNED_BYTE:
        return rr::INDEXTYPE_UINT8;
    case GL_UNSIGNED_SHORT:
        return rr::INDEXTYPE_UINT16;
    case GL_UNSIGNED_INT:
        return rr::INDEXTYPE_UINT32;
    default:
        DE_ASSERT(false);
        return rr::INDEXTYPE_LAST;
    }
}

rr::GeometryShaderOutputType mapGLGeometryShaderOutputType(uint32_t primitive)
{
    switch (primitive)
    {
    case GL_POINTS:
        return rr::GEOMETRYSHADEROUTPUTTYPE_POINTS;
    case GL_LINE_STRIP:
        return rr::GEOMETRYSHADEROUTPUTTYPE_LINE_STRIP;
    case GL_TRIANGLE_STRIP:
        return rr::GEOMETRYSHADEROUTPUTTYPE_TRIANGLE_STRIP;
    default:
        DE_ASSERT(false);
        return rr::GEOMETRYSHADEROUTPUTTYPE_LAST;
    }
}

rr::GeometryShaderInputType mapGLGeometryShaderInputType(uint32_t primitive)
{
    switch (primitive)
    {
    case GL_POINTS:
        return rr::GEOMETRYSHADERINPUTTYPE_POINTS;
    case GL_LINES:
        return rr::GEOMETRYSHADERINPUTTYPE_LINES;
    case GL_LINE_STRIP:
        return rr::GEOMETRYSHADERINPUTTYPE_LINES;
    case GL_LINE_LOOP:
        return rr::GEOMETRYSHADERINPUTTYPE_LINES;
    case GL_TRIANGLES:
        return rr::GEOMETRYSHADERINPUTTYPE_TRIANGLES;
    case GL_TRIANGLE_STRIP:
        return rr::GEOMETRYSHADERINPUTTYPE_TRIANGLES;
    case GL_TRIANGLE_FAN:
        return rr::GEOMETRYSHADERINPUTTYPE_TRIANGLES;
    case GL_LINES_ADJACENCY:
        return rr::GEOMETRYSHADERINPUTTYPE_LINES_ADJACENCY;
    case GL_LINE_STRIP_ADJACENCY:
        return rr::GEOMETRYSHADERINPUTTYPE_LINES_ADJACENCY;
    case GL_TRIANGLES_ADJACENCY:
        return rr::GEOMETRYSHADERINPUTTYPE_TRIANGLES_ADJACENCY;
    case GL_TRIANGLE_STRIP_ADJACENCY:
        return rr::GEOMETRYSHADERINPUTTYPE_TRIANGLES_ADJACENCY;
    default:
        DE_ASSERT(false);
        return rr::GEOMETRYSHADERINPUTTYPE_LAST;
    }
}

rr::TestFunc mapGLTestFunc(uint32_t func)
{
    switch (func)
    {
    case GL_ALWAYS:
        return rr::TESTFUNC_ALWAYS;
    case GL_EQUAL:
        return rr::TESTFUNC_EQUAL;
    case GL_GEQUAL:
        return rr::TESTFUNC_GEQUAL;
    case GL_GREATER:
        return rr::TESTFUNC_GREATER;
    case GL_LEQUAL:
        return rr::TESTFUNC_LEQUAL;
    case GL_LESS:
        return rr::TESTFUNC_LESS;
    case GL_NEVER:
        return rr::TESTFUNC_NEVER;
    case GL_NOTEQUAL:
        return rr::TESTFUNC_NOTEQUAL;
    default:
        DE_ASSERT(false);
        return rr::TESTFUNC_LAST;
    }
}

rr::StencilOp mapGLStencilOp(uint32_t op)
{
    switch (op)
    {
    case GL_KEEP:
        return rr::STENCILOP_KEEP;
    case GL_ZERO:
        return rr::STENCILOP_ZERO;
    case GL_REPLACE:
        return rr::STENCILOP_REPLACE;
    case GL_INCR:
        return rr::STENCILOP_INCR;
    case GL_DECR:
        return rr::STENCILOP_DECR;
    case GL_INCR_WRAP:
        return rr::STENCILOP_INCR_WRAP;
    case GL_DECR_WRAP:
        return rr::STENCILOP_DECR_WRAP;
    case GL_INVERT:
        return rr::STENCILOP_INVERT;
    default:
        DE_ASSERT(false);
        return rr::STENCILOP_LAST;
    }
}

rr::BlendEquation mapGLBlendEquation(uint32_t equation)
{
    switch (equation)
    {
    case GL_FUNC_ADD:
        return rr::BLENDEQUATION_ADD;
    case GL_FUNC_SUBTRACT:
        return rr::BLENDEQUATION_SUBTRACT;
    case GL_FUNC_REVERSE_SUBTRACT:
        return rr::BLENDEQUATION_REVERSE_SUBTRACT;
    case GL_MIN:
        return rr::BLENDEQUATION_MIN;
    case GL_MAX:
        return rr::BLENDEQUATION_MAX;
    default:
        DE_ASSERT(false);
        return rr::BLENDEQUATION_LAST;
    }
}

rr::BlendEquationAdvanced mapGLBlendEquationAdvanced(uint32_t equation)
{
    switch (equation)
    {
    case GL_MULTIPLY_KHR:
        return rr::BLENDEQUATION_ADVANCED_MULTIPLY;
    case GL_SCREEN_KHR:
        return rr::BLENDEQUATION_ADVANCED_SCREEN;
    case GL_OVERLAY_KHR:
        return rr::BLENDEQUATION_ADVANCED_OVERLAY;
    case GL_DARKEN_KHR:
        return rr::BLENDEQUATION_ADVANCED_DARKEN;
    case GL_LIGHTEN_KHR:
        return rr::BLENDEQUATION_ADVANCED_LIGHTEN;
    case GL_COLORDODGE_KHR:
        return rr::BLENDEQUATION_ADVANCED_COLORDODGE;
    case GL_COLORBURN_KHR:
        return rr::BLENDEQUATION_ADVANCED_COLORBURN;
    case GL_HARDLIGHT_KHR:
        return rr::BLENDEQUATION_ADVANCED_HARDLIGHT;
    case GL_SOFTLIGHT_KHR:
        return rr::BLENDEQUATION_ADVANCED_SOFTLIGHT;
    case GL_DIFFERENCE_KHR:
        return rr::BLENDEQUATION_ADVANCED_DIFFERENCE;
    case GL_EXCLUSION_KHR:
        return rr::BLENDEQUATION_ADVANCED_EXCLUSION;
    case GL_HSL_HUE_KHR:
        return rr::BLENDEQUATION_ADVANCED_HSL_HUE;
    case GL_HSL_SATURATION_KHR:
        return rr::BLENDEQUATION_ADVANCED_HSL_SATURATION;
    case GL_HSL_COLOR_KHR:
        return rr::BLENDEQUATION_ADVANCED_HSL_COLOR;
    case GL_HSL_LUMINOSITY_KHR:
        return rr::BLENDEQUATION_ADVANCED_HSL_LUMINOSITY;
    default:
        DE_ASSERT(false);
        return rr::BLENDEQUATION_ADVANCED_LAST;
    }
}

rr::BlendFunc mapGLBlendFunc(uint32_t func)
{
    switch (func)
    {
    case GL_ZERO:
        return rr::BLENDFUNC_ZERO;
    case GL_ONE:
        return rr::BLENDFUNC_ONE;
    case GL_SRC_COLOR:
        return rr::BLENDFUNC_SRC_COLOR;
    case GL_ONE_MINUS_SRC_COLOR:
        return rr::BLENDFUNC_ONE_MINUS_SRC_COLOR;
    case GL_DST_COLOR:
        return rr::BLENDFUNC_DST_COLOR;
    case GL_ONE_MINUS_DST_COLOR:
        return rr::BLENDFUNC_ONE_MINUS_DST_COLOR;
    case GL_SRC_ALPHA:
        return rr::BLENDFUNC_SRC_ALPHA;
    case GL_ONE_MINUS_SRC_ALPHA:
        return rr::BLENDFUNC_ONE_MINUS_SRC_ALPHA;
    case GL_DST_ALPHA:
        return rr::BLENDFUNC_DST_ALPHA;
    case GL_ONE_MINUS_DST_ALPHA:
        return rr::BLENDFUNC_ONE_MINUS_DST_ALPHA;
    case GL_CONSTANT_COLOR:
        return rr::BLENDFUNC_CONSTANT_COLOR;
    case GL_ONE_MINUS_CONSTANT_COLOR:
        return rr::BLENDFUNC_ONE_MINUS_CONSTANT_COLOR;
    case GL_CONSTANT_ALPHA:
        return rr::BLENDFUNC_CONSTANT_ALPHA;
    case GL_ONE_MINUS_CONSTANT_ALPHA:
        return rr::BLENDFUNC_ONE_MINUS_CONSTANT_ALPHA;
    case GL_SRC_ALPHA_SATURATE:
        return rr::BLENDFUNC_SRC_ALPHA_SATURATE;
    case GL_SRC1_COLOR:
        return rr::BLENDFUNC_SRC1_COLOR;
    case GL_ONE_MINUS_SRC1_COLOR:
        return rr::BLENDFUNC_ONE_MINUS_SRC1_COLOR;
    case GL_SRC1_ALPHA:
        return rr::BLENDFUNC_SRC1_ALPHA;
    case GL_ONE_MINUS_SRC1_ALPHA:
        return rr::BLENDFUNC_ONE_MINUS_SRC1_ALPHA;
    default:
        DE_ASSERT(false);
        return rr::BLENDFUNC_LAST;
    }
}

} // namespace rr_util
} // namespace sglr
