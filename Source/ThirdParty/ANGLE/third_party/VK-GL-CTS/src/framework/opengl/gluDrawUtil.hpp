#ifndef _GLUDRAWUTIL_HPP
#define _GLUDRAWUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL Utilities
 * ---------------------------------------------
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
 * \brief Draw call utilities.
 *
 * Draw call utilities provide an abstraction for commonly used draw calls.
 * The objective of that abstraction is to allow moving data to buffers
 * and state to VAOs automatically if target context doesn't support
 * user pointers or default VAOs.
 *//*--------------------------------------------------------------------*/

#include "gluDefs.hpp"

#include <string>

namespace glu
{

class RenderContext;

enum VertexComponentType
{
    // Standard types: all conversion types apply.
    VTX_COMP_UNSIGNED_INT8 = 0,
    VTX_COMP_UNSIGNED_INT16,
    VTX_COMP_UNSIGNED_INT32,
    VTX_COMP_SIGNED_INT8,
    VTX_COMP_SIGNED_INT16,
    VTX_COMP_SIGNED_INT32,

    // Special types: only CONVERT_NONE is allowed.
    VTX_COMP_FIXED,
    VTX_COMP_HALF_FLOAT,
    VTX_COMP_FLOAT,

    VTX_COMP_TYPE_LAST
};

enum VertexComponentConversion
{
    VTX_COMP_CONVERT_NONE = 0,           //!< No conversion: integer types, or floating-point values.
    VTX_COMP_CONVERT_NORMALIZE_TO_FLOAT, //!< Normalize integers to range [0,1] or [-1,1] depending on type.
    VTX_COMP_CONVERT_CAST_TO_FLOAT,      //!< Convert to floating-point directly.

    VTX_COMP_CONVERT_LAST
};

enum IndexType
{
    INDEXTYPE_UINT8,
    INDEXTYPE_UINT16,
    INDEXTYPE_UINT32,

    INDEXTYPE_LAST
};

enum PrimitiveType
{
    PRIMITIVETYPE_TRIANGLES = 0,
    PRIMITIVETYPE_TRIANGLE_STRIP,
    PRIMITIVETYPE_TRIANGLE_FAN,

    PRIMITIVETYPE_LINES,
    PRIMITIVETYPE_LINE_STRIP,
    PRIMITIVETYPE_LINE_LOOP,

    PRIMITIVETYPE_POINTS,

    PRIMITIVETYPE_PATCHES,

    PRIMITIVETYPE_LAST
};

struct BindingPoint
{
    enum Type
    {
        BPTYPE_LOCATION = 0, //!< Binding by numeric location.
        BPTYPE_NAME,         //!< Binding by input name.

        BPTYPE_LAST
    };

    Type type;        //!< Binding type (name or location).
    std::string name; //!< Input name, or empty if is not binding by name.
    int location;     //!< Input location, or offset to named location if binding by name.

    BindingPoint(void) : type(BPTYPE_LAST), location(0)
    {
    }
    explicit BindingPoint(int location_) : type(BPTYPE_LOCATION), location(location_)
    {
    }
    explicit BindingPoint(const std::string &name_, int location_ = 0)
        : type(BPTYPE_NAME)
        , name(name_)
        , location(location_)
    {
    }
};

struct VertexArrayPointer
{
    VertexComponentType componentType; //!< Component type.
    VertexComponentConversion convert; //!< Component conversion type.
    int numComponents;                 //!< Number of components per element.
    int numElements;                   //!< Number of elements in total.
    int stride;                        //!< Element stride.

    const void *data; //!< Data pointer.

    VertexArrayPointer(VertexComponentType componentType_, VertexComponentConversion convert_, int numComponents_,
                       int numElements_, int stride_, const void *data_)
        : componentType(componentType_)
        , convert(convert_)
        , numComponents(numComponents_)
        , numElements(numElements_)
        , stride(stride_)
        , data(data_)
    {
    }

    VertexArrayPointer(void)
        : componentType(VTX_COMP_TYPE_LAST)
        , convert(VTX_COMP_CONVERT_LAST)
        , numComponents(0)
        , numElements(0)
        , stride(0)
        , data(0)
    {
    }
} DE_WARN_UNUSED_TYPE;

struct VertexArrayBinding
{
    BindingPoint binding;
    VertexArrayPointer pointer;

    VertexArrayBinding(const BindingPoint &binding_, const VertexArrayPointer &pointer_)
        : binding(binding_)
        , pointer(pointer_)
    {
    }

    VertexArrayBinding(void)
    {
    }
} DE_WARN_UNUSED_TYPE;

struct PrimitiveList
{
    PrimitiveType type;  //!< Primitive type.
    int numElements;     //!< Number of elements to be drawn.
    IndexType indexType; //!< Index type or INDEXTYPE_LAST if not used
    const void *indices; //!< Index list or nullptr if not used.

    PrimitiveList(PrimitiveType type_, int numElements_)
        : type(type_)
        , numElements(numElements_)
        , indexType(INDEXTYPE_LAST)
        , indices(0)
    {
    }

    PrimitiveList(PrimitiveType type_, int numElements_, IndexType indexType_, const void *indices_)
        : type(type_)
        , numElements(numElements_)
        , indexType(indexType_)
        , indices(indices_)
    {
    }

    PrimitiveList(void) : type(PRIMITIVETYPE_LAST), numElements(0), indexType(INDEXTYPE_LAST), indices(0)
    {
    }
} DE_WARN_UNUSED_TYPE;

class DrawUtilCallback
{
public:
    virtual void beforeDrawCall(void)
    {
    }
    virtual void afterDrawCall(void)
    {
    }
};

void draw(const RenderContext &context, uint32_t program, int numVertexArrays, const VertexArrayBinding *vertexArrays,
          const PrimitiveList &primitives, DrawUtilCallback *callback = nullptr);

void drawFromUserPointers(const RenderContext &context, uint32_t program, int numVertexArrays,
                          const VertexArrayBinding *vertexArrays, const PrimitiveList &primitives,
                          DrawUtilCallback *callback = nullptr);
void drawFromBuffers(const RenderContext &context, uint32_t program, int numVertexArrays,
                     const VertexArrayBinding *vertexArrays, const PrimitiveList &primitives,
                     DrawUtilCallback *callback = nullptr);
void drawFromVAOBuffers(const RenderContext &context, uint32_t program, int numVertexArrays,
                        const VertexArrayBinding *vertexArrays, const PrimitiveList &primitives,
                        DrawUtilCallback *callback = nullptr);

// Shorthands for PrimitiveList
namespace pr
{

#define DECLARE_PR_CTOR(NAME, TYPE)                                         \
    inline PrimitiveList NAME(int numElements)                              \
    {                                                                       \
        return PrimitiveList(TYPE, numElements);                            \
    }                                                                       \
    inline PrimitiveList NAME(int numElements, const uint8_t *indices)      \
    {                                                                       \
        return PrimitiveList(TYPE, numElements, INDEXTYPE_UINT8, indices);  \
    }                                                                       \
    inline PrimitiveList NAME(int numElements, const uint16_t *indices)     \
    {                                                                       \
        return PrimitiveList(TYPE, numElements, INDEXTYPE_UINT16, indices); \
    }                                                                       \
    inline PrimitiveList NAME(int numElements, const uint32_t *indices)     \
    {                                                                       \
        return PrimitiveList(TYPE, numElements, INDEXTYPE_UINT32, indices); \
    }                                                                       \
    struct DeclarePRCtor##NAME##Unused_s                                    \
    {                                                                       \
        int unused;                                                         \
    }

DECLARE_PR_CTOR(Triangles, PRIMITIVETYPE_TRIANGLES);
DECLARE_PR_CTOR(TriangleStrip, PRIMITIVETYPE_TRIANGLE_STRIP);
DECLARE_PR_CTOR(TriangleFan, PRIMITIVETYPE_TRIANGLE_FAN);

DECLARE_PR_CTOR(Lines, PRIMITIVETYPE_LINES);
DECLARE_PR_CTOR(LineStrip, PRIMITIVETYPE_LINE_STRIP);
DECLARE_PR_CTOR(LineLineLoop, PRIMITIVETYPE_LINE_LOOP);

DECLARE_PR_CTOR(Points, PRIMITIVETYPE_POINTS);

DECLARE_PR_CTOR(Patches, PRIMITIVETYPE_PATCHES);

} // namespace pr

// Shorthands for VertexArrayBinding
namespace va
{

#define DECLARE_VA_CTOR(NAME, DATATYPE, TYPE, CONVERT)                                                                 \
    inline VertexArrayBinding NAME(const std::string &name, int offset, int numComponents, int numElements,            \
                                   int stride, const DATATYPE *data)                                                   \
    {                                                                                                                  \
        return VertexArrayBinding(BindingPoint(name, offset),                                                          \
                                  VertexArrayPointer(TYPE, CONVERT, numComponents, numElements, stride, data));        \
    }                                                                                                                  \
    inline VertexArrayBinding NAME(const std::string &name, int numComponents, int numElements, int stride,            \
                                   const DATATYPE *data)                                                               \
    {                                                                                                                  \
        return NAME(name, 0, numComponents, numElements, stride, data);                                                \
    }                                                                                                                  \
    inline VertexArrayBinding NAME(int location, int numComponents, int numElements, int stride, const DATATYPE *data) \
    {                                                                                                                  \
        return VertexArrayBinding(BindingPoint(location),                                                              \
                                  VertexArrayPointer(TYPE, CONVERT, numComponents, numElements, stride, data));        \
    }                                                                                                                  \
    struct DeclareVACtor##NAME##Unused_s                                                                               \
    {                                                                                                                  \
        int unused;                                                                                                    \
    }

// Integer types
DECLARE_VA_CTOR(Uint8, uint8_t, VTX_COMP_UNSIGNED_INT8, VTX_COMP_CONVERT_NONE);
DECLARE_VA_CTOR(Uint16, uint16_t, VTX_COMP_UNSIGNED_INT16, VTX_COMP_CONVERT_NONE);
DECLARE_VA_CTOR(Uint32, uint32_t, VTX_COMP_UNSIGNED_INT32, VTX_COMP_CONVERT_NONE);
DECLARE_VA_CTOR(Int8, int8_t, VTX_COMP_SIGNED_INT8, VTX_COMP_CONVERT_NONE);
DECLARE_VA_CTOR(Int16, int16_t, VTX_COMP_SIGNED_INT16, VTX_COMP_CONVERT_NONE);
DECLARE_VA_CTOR(Int32, int32_t, VTX_COMP_SIGNED_INT32, VTX_COMP_CONVERT_NONE);

// Normalized integers.
DECLARE_VA_CTOR(Unorm8, uint8_t, VTX_COMP_UNSIGNED_INT8, VTX_COMP_CONVERT_NORMALIZE_TO_FLOAT);
DECLARE_VA_CTOR(Unorm16, uint16_t, VTX_COMP_UNSIGNED_INT16, VTX_COMP_CONVERT_NORMALIZE_TO_FLOAT);
DECLARE_VA_CTOR(Unorm32, uint32_t, VTX_COMP_UNSIGNED_INT32, VTX_COMP_CONVERT_NORMALIZE_TO_FLOAT);
DECLARE_VA_CTOR(Snorm8, int8_t, VTX_COMP_SIGNED_INT8, VTX_COMP_CONVERT_NORMALIZE_TO_FLOAT);
DECLARE_VA_CTOR(Snorm16, int16_t, VTX_COMP_SIGNED_INT16, VTX_COMP_CONVERT_NORMALIZE_TO_FLOAT);
DECLARE_VA_CTOR(Snorm32, int32_t, VTX_COMP_SIGNED_INT32, VTX_COMP_CONVERT_NORMALIZE_TO_FLOAT);

// Integers converted to float.
DECLARE_VA_CTOR(Uint8Float, uint8_t, VTX_COMP_UNSIGNED_INT8, VTX_COMP_CONVERT_CAST_TO_FLOAT);
DECLARE_VA_CTOR(Uint16Float, uint16_t, VTX_COMP_UNSIGNED_INT16, VTX_COMP_CONVERT_CAST_TO_FLOAT);
DECLARE_VA_CTOR(Uint32Float, uint32_t, VTX_COMP_UNSIGNED_INT32, VTX_COMP_CONVERT_CAST_TO_FLOAT);
DECLARE_VA_CTOR(Int8Float, int8_t, VTX_COMP_SIGNED_INT8, VTX_COMP_CONVERT_CAST_TO_FLOAT);
DECLARE_VA_CTOR(Int16Float, int16_t, VTX_COMP_SIGNED_INT16, VTX_COMP_CONVERT_CAST_TO_FLOAT);
DECLARE_VA_CTOR(Int32Float, int32_t, VTX_COMP_SIGNED_INT32, VTX_COMP_CONVERT_CAST_TO_FLOAT);

// Special types.
DECLARE_VA_CTOR(Fixed, void, VTX_COMP_FIXED, VTX_COMP_CONVERT_NONE);
DECLARE_VA_CTOR(Half, void, VTX_COMP_HALF_FLOAT, VTX_COMP_CONVERT_NONE);
DECLARE_VA_CTOR(Float, float, VTX_COMP_FLOAT, VTX_COMP_CONVERT_NONE);

#undef DECLARE_VA_CTOR

} // namespace va

} // namespace glu

#endif // _GLUDRAWUTIL_HPP
