#ifndef _RRVERTEXATTRIB_HPP
#define _RRVERTEXATTRIB_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Reference Renderer
 * -----------------------------------------------
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
 * \brief Vertex attribute fetch.
 *//*--------------------------------------------------------------------*/

#include "rrDefs.hpp"
#include "rrGenericVector.hpp"
#include "tcuVector.hpp"

namespace rr
{

enum VertexAttribType
{
    // Can only be read as floats
    VERTEXATTRIBTYPE_FLOAT = 0,
    VERTEXATTRIBTYPE_HALF,
    VERTEXATTRIBTYPE_FIXED,
    VERTEXATTRIBTYPE_DOUBLE,

    // Can only be read as floats, will be normalized
    VERTEXATTRIBTYPE_NONPURE_UNORM8,
    VERTEXATTRIBTYPE_NONPURE_UNORM16,
    VERTEXATTRIBTYPE_NONPURE_UNORM32,
    VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV, //!< Packed format, only size = 4 is allowed

    // Clamped formats, GLES3-style conversion: max{c / (2^(b-1) - 1), -1 }
    VERTEXATTRIBTYPE_NONPURE_SNORM8_CLAMP,
    VERTEXATTRIBTYPE_NONPURE_SNORM16_CLAMP,
    VERTEXATTRIBTYPE_NONPURE_SNORM32_CLAMP,
    VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP, //!< Packed format, only size = 4 is allowed

    // Scaled formats, GLES2-style conversion: (2c + 1) / (2^b - 1)
    VERTEXATTRIBTYPE_NONPURE_SNORM8_SCALE,
    VERTEXATTRIBTYPE_NONPURE_SNORM16_SCALE,
    VERTEXATTRIBTYPE_NONPURE_SNORM32_SCALE,
    VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE, //!< Packed format, only size = 4 is allowed

    // can only be read as float, will not be normalized
    VERTEXATTRIBTYPE_NONPURE_UINT8,
    VERTEXATTRIBTYPE_NONPURE_UINT16,
    VERTEXATTRIBTYPE_NONPURE_UINT32,

    VERTEXATTRIBTYPE_NONPURE_INT8,
    VERTEXATTRIBTYPE_NONPURE_INT16,
    VERTEXATTRIBTYPE_NONPURE_INT32,

    VERTEXATTRIBTYPE_NONPURE_UINT_2_10_10_10_REV, //!< Packed format, only size = 4 is allowed
    VERTEXATTRIBTYPE_NONPURE_INT_2_10_10_10_REV,  //!< Packed format, only size = 4 is allowed

    // can only be read as integers
    VERTEXATTRIBTYPE_PURE_UINT8,
    VERTEXATTRIBTYPE_PURE_UINT16,
    VERTEXATTRIBTYPE_PURE_UINT32,

    VERTEXATTRIBTYPE_PURE_INT8,
    VERTEXATTRIBTYPE_PURE_INT16,
    VERTEXATTRIBTYPE_PURE_INT32,

    // reordered formats of GL_ARB_vertex_array_bgra
    VERTEXATTRIBTYPE_NONPURE_UNORM8_BGRA,
    VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV_BGRA,
    VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP_BGRA,
    VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE_BGRA,

    // can be read as anything
    VERTEXATTRIBTYPE_DONT_CARE, //!< Do not enforce type checking when reading GENERIC attribute. Used for current client side attributes.

    VERTEXATTRIBTYPE_LAST
};

/*--------------------------------------------------------------------*//*!
 * \brief Vertex attribute slot
 *
 * Vertex attribute type specifies component type for attribute and it
 * includes signed & normalized bits as well.
 *
 * Attribute size specifies how many components there are per vertex.
 * If size is 0, no components are fetched, ie. vertex attribute slot
 * is disabled.
 *
 * Divisor specifies the rate at which vertex attribute advances. If it is
 * zero, attribute is advanced per vertex. If divisor is non-zero, attribute
 * advances once per instanceDivisor instances.
 *
 * Pointer is used if not null, otherwise generic attribute is used instead
 * and in such case only DONT_CARE is valid attribute type.
 *//*--------------------------------------------------------------------*/
struct VertexAttrib
{
    VertexAttribType type; //!< Attribute component type.
    int size;              //!< Number of components, valid range is [0,4].
    int stride; //!< Number of bytes two consecutive elements differ by. Zero works as in GL. Valid range is [0, inf).
    int instanceDivisor; //!< Vertex attribute divisor.
    const void *pointer; //!< Data pointer.
    GenericVec4 generic; //!< Generic attribute, used if pointer is null.

    VertexAttrib(void) : type(VERTEXATTRIBTYPE_FLOAT), size(0), stride(0), instanceDivisor(0), pointer(nullptr)
    {
    }

    VertexAttrib(VertexAttribType type_, int size_, int stride_, int instanceDivisor_, const void *pointer_)
        : type(type_)
        , size(size_)
        , stride(stride_)
        , instanceDivisor(instanceDivisor_)
        , pointer(pointer_)
    {
    }

    template <typename ScalarType>
    explicit VertexAttrib(const tcu::Vector<ScalarType, 4> &generic_)
        : type(VERTEXATTRIBTYPE_DONT_CARE)
        , size(0)
        , stride(0)
        , instanceDivisor(0)
        , pointer(nullptr)
        , generic(generic_)
    {
    }
} DE_WARN_UNUSED_TYPE;

bool isValidVertexAttrib(const VertexAttrib &vertexAttrib);
// \todo [2013-04-01 pyry] Queries: isReadFloatValid(), isReadIntValid() ...

void readVertexAttrib(tcu::Vec4 &dst, const VertexAttrib &vertexAttrib, const int instanceNdx, const int vertexNdx,
                      const int baseInstanceNdx = 0);
void readVertexAttrib(tcu::IVec4 &dst, const VertexAttrib &vertexAttrib, const int instanceNdx, const int vertexNdx,
                      const int baseInstanceNdx = 0);
void readVertexAttrib(tcu::UVec4 &dst, const VertexAttrib &vertexAttrib, const int instanceNdx, const int vertexNdx,
                      const int baseInstanceNdx = 0);

// Helpers that return by value (trivial for compiler to optimize).

inline tcu::Vec4 readVertexAttribFloat(const VertexAttrib &vertexAttrib, const int instanceNdx, const int vertexNdx,
                                       const int baseInstanceNdx = 0)
{
    tcu::Vec4 v;
    readVertexAttrib(v, vertexAttrib, instanceNdx, vertexNdx, baseInstanceNdx);
    return v;
}

inline tcu::IVec4 readVertexAttribInt(const VertexAttrib &vertexAttrib, const int instanceNdx, const int vertexNdx,
                                      const int baseInstanceNdx = 0)
{
    tcu::IVec4 v;
    readVertexAttrib(v, vertexAttrib, instanceNdx, vertexNdx, baseInstanceNdx);
    return v;
}

inline tcu::UVec4 readVertexAttribUint(const VertexAttrib &vertexAttrib, const int instanceNdx, const int vertexNdx,
                                       const int baseInstanceNdx = 0)
{
    tcu::UVec4 v;
    readVertexAttrib(v, vertexAttrib, instanceNdx, vertexNdx, baseInstanceNdx);
    return v;
}

} // namespace rr

#endif // _RRVERTEXATTRIB_HPP
