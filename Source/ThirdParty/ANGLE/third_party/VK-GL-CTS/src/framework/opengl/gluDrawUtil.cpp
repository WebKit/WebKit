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
 *//*--------------------------------------------------------------------*/

#include "gluDrawUtil.hpp"
#include "gluRenderContext.hpp"
#include "gluObjectWrapper.hpp"
#include "glwFunctions.hpp"
#include "glwEnums.hpp"
#include "deInt32.h"
#include "deMemory.h"

#include <vector>
#include <set>
#include <iterator>

namespace glu
{
namespace
{

struct VertexAttributeDescriptor
{
    int location;
    VertexComponentType componentType;
    VertexComponentConversion convert;
    int numComponents;
    int numElements;
    int stride;          //!< Stride or 0 if using default stride.
    const void *pointer; //!< Pointer or offset.

    VertexAttributeDescriptor(int location_, VertexComponentType componentType_, VertexComponentConversion convert_,
                              int numComponents_, int numElements_, int stride_, const void *pointer_)
        : location(location_)
        , componentType(componentType_)
        , convert(convert_)
        , numComponents(numComponents_)
        , numElements(numElements_)
        , stride(stride_)
        , pointer(pointer_)
    {
    }

    VertexAttributeDescriptor(void)
        : location(0)
        , componentType(VTX_COMP_TYPE_LAST)
        , convert(VTX_COMP_CONVERT_LAST)
        , numComponents(0)
        , numElements(0)
        , stride(0)
        , pointer(0)
    {
    }
};

struct VertexBufferLayout
{
    int size;
    std::vector<VertexAttributeDescriptor> attributes;

    VertexBufferLayout(int size_ = 0) : size(size_)
    {
    }
};

struct VertexBufferDescriptor
{
    uint32_t buffer;
    std::vector<VertexAttributeDescriptor> attributes;

    VertexBufferDescriptor(uint32_t buffer_ = 0) : buffer(buffer_)
    {
    }
};

class VertexBuffer : public Buffer
{
public:
    enum Type
    {
        TYPE_PLANAR = 0, //!< Data for each vertex array resides in a separate contiguous block in buffer.
        TYPE_STRIDED,    //!< Vertex arrays are interleaved.

        TYPE_LAST
    };

    VertexBuffer(const RenderContext &context, int numBindings, const VertexArrayBinding *bindings,
                 Type type = TYPE_PLANAR);
    ~VertexBuffer(void);

    const VertexBufferDescriptor &getDescriptor(void) const
    {
        return m_layout;
    }

private:
    VertexBuffer(const VertexBuffer &other);
    VertexBuffer &operator=(const VertexBuffer &other);

    VertexBufferDescriptor m_layout;
};

class IndexBuffer : public Buffer
{
public:
    IndexBuffer(const RenderContext &context, IndexType indexType, int numIndices, const void *indices);
    ~IndexBuffer(void);

private:
    IndexBuffer(const IndexBuffer &other);
    IndexBuffer &operator=(const IndexBuffer &other);
};

static uint32_t getVtxCompGLType(VertexComponentType type)
{
    switch (type)
    {
    case VTX_COMP_UNSIGNED_INT8:
        return GL_UNSIGNED_BYTE;
    case VTX_COMP_UNSIGNED_INT16:
        return GL_UNSIGNED_SHORT;
    case VTX_COMP_UNSIGNED_INT32:
        return GL_UNSIGNED_INT;
    case VTX_COMP_SIGNED_INT8:
        return GL_BYTE;
    case VTX_COMP_SIGNED_INT16:
        return GL_SHORT;
    case VTX_COMP_SIGNED_INT32:
        return GL_INT;
    case VTX_COMP_FIXED:
        return GL_FIXED;
    case VTX_COMP_HALF_FLOAT:
        return GL_HALF_FLOAT;
    case VTX_COMP_FLOAT:
        return GL_FLOAT;
    default:
        DE_ASSERT(false);
        return GL_NONE;
    }
}

static int getVtxCompSize(VertexComponentType type)
{
    switch (type)
    {
    case VTX_COMP_UNSIGNED_INT8:
        return 1;
    case VTX_COMP_UNSIGNED_INT16:
        return 2;
    case VTX_COMP_UNSIGNED_INT32:
        return 4;
    case VTX_COMP_SIGNED_INT8:
        return 1;
    case VTX_COMP_SIGNED_INT16:
        return 2;
    case VTX_COMP_SIGNED_INT32:
        return 4;
    case VTX_COMP_FIXED:
        return 4;
    case VTX_COMP_HALF_FLOAT:
        return 2;
    case VTX_COMP_FLOAT:
        return 4;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

static uint32_t getIndexGLType(IndexType type)
{
    switch (type)
    {
    case INDEXTYPE_UINT8:
        return GL_UNSIGNED_BYTE;
    case INDEXTYPE_UINT16:
        return GL_UNSIGNED_SHORT;
    case INDEXTYPE_UINT32:
        return GL_UNSIGNED_INT;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

static int getIndexSize(IndexType type)
{
    switch (type)
    {
    case INDEXTYPE_UINT8:
        return 1;
    case INDEXTYPE_UINT16:
        return 2;
    case INDEXTYPE_UINT32:
        return 4;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

static uint32_t getPrimitiveGLType(PrimitiveType type)
{
    switch (type)
    {
    case PRIMITIVETYPE_TRIANGLES:
        return GL_TRIANGLES;
    case PRIMITIVETYPE_TRIANGLE_STRIP:
        return GL_TRIANGLE_STRIP;
    case PRIMITIVETYPE_TRIANGLE_FAN:
        return GL_TRIANGLE_FAN;
    case PRIMITIVETYPE_LINES:
        return GL_LINES;
    case PRIMITIVETYPE_LINE_STRIP:
        return GL_LINE_STRIP;
    case PRIMITIVETYPE_LINE_LOOP:
        return GL_LINE_LOOP;
    case PRIMITIVETYPE_POINTS:
        return GL_POINTS;
    case PRIMITIVETYPE_PATCHES:
        return GL_PATCHES;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

//! Lower named bindings to locations and eliminate bindings that are not used by program.
template <typename InputIter, typename OutputIter>
static OutputIter namedBindingsToProgramLocations(const glw::Functions &gl, uint32_t program, InputIter first,
                                                  InputIter end, OutputIter out)
{
    for (InputIter cur = first; cur != end; ++cur)
    {
        const BindingPoint &binding = cur->binding;
        if (binding.type == BindingPoint::BPTYPE_NAME)
        {
            DE_ASSERT(binding.location >= 0);
            int location = gl.getAttribLocation(program, binding.name.c_str());
            if (location >= 0)
            {
                // Add binding.location as an offset to accommodate matrices.
                *out = VertexArrayBinding(BindingPoint(location + binding.location), cur->pointer);
                ++out;
            }
        }
        else
        {
            *out = *cur;
            ++out;
        }
    }

    return out;
}

static uint32_t getMinimumAlignment(const VertexArrayPointer &pointer)
{
    // \todo [2013-05-07 pyry] What is the actual min?
    DE_UNREF(pointer);
    return (uint32_t)sizeof(float);
}

template <typename BindingIter>
static bool areVertexArrayLocationsValid(BindingIter first, BindingIter end)
{
    std::set<int> usedLocations;
    for (BindingIter cur = first; cur != end; ++cur)
    {
        const BindingPoint &binding = cur->binding;

        if (binding.type != BindingPoint::BPTYPE_LOCATION)
            return false;

        if (usedLocations.find(binding.location) != usedLocations.end())
            return false;

        usedLocations.insert(binding.location);
    }

    return true;
}

// \todo [2013-05-08 pyry] Buffer upload should try to match pointers to reduce dataset size.

static void appendAttributeNonStrided(VertexBufferLayout &layout, const VertexArrayBinding &va)
{
    const int offset      = deAlign32(layout.size, getMinimumAlignment(va.pointer));
    const int elementSize = getVtxCompSize(va.pointer.componentType) * va.pointer.numComponents;
    const int size        = elementSize * va.pointer.numElements;

    // Must be assigned to location at this point.
    DE_ASSERT(va.binding.type == BindingPoint::BPTYPE_LOCATION);

    layout.attributes.push_back(VertexAttributeDescriptor(va.binding.location, va.pointer.componentType,
                                                          va.pointer.convert, va.pointer.numComponents,
                                                          va.pointer.numElements,
                                                          0, // default stride
                                                          (const void *)(uintptr_t)offset));
    layout.size = offset + size;
}

template <typename BindingIter>
static void computeNonStridedBufferLayout(VertexBufferLayout &layout, BindingIter first, BindingIter end)
{
    for (BindingIter iter = first; iter != end; ++iter)
        appendAttributeNonStrided(layout, *iter);
}

static void copyToLayout(void *dstBasePtr, const VertexAttributeDescriptor &dstVA, const VertexArrayPointer &srcPtr)
{
    DE_ASSERT(dstVA.componentType == srcPtr.componentType && dstVA.numComponents == srcPtr.numComponents &&
              dstVA.numElements == srcPtr.numElements);

    const int elementSize         = getVtxCompSize(dstVA.componentType) * dstVA.numComponents;
    const bool srcHasCustomStride = srcPtr.stride != 0 && srcPtr.stride != elementSize;
    const bool dstHasCustomStride = dstVA.stride != 0 && dstVA.stride != elementSize;

    if (srcHasCustomStride || dstHasCustomStride)
    {
        const int dstStride = dstVA.stride != 0 ? dstVA.stride : elementSize;
        const int srcStride = srcPtr.stride != 0 ? srcPtr.stride : elementSize;

        for (int ndx = 0; ndx < dstVA.numElements; ndx++)
            deMemcpy((uint8_t *)dstBasePtr + (uintptr_t)dstVA.pointer + ndx * dstStride,
                     (const uint8_t *)srcPtr.data + ndx * srcStride, elementSize);
    }
    else
        deMemcpy((uint8_t *)dstBasePtr + (uintptr_t)dstVA.pointer, srcPtr.data, elementSize * dstVA.numElements);
}

void uploadBufferData(const glw::Functions &gl, uint32_t buffer, uint32_t usage, const VertexBufferLayout &layout,
                      const VertexArrayPointer *srcArrays)
{
    // Create temporary data buffer for upload.
    std::vector<uint8_t> localBuf(layout.size);

    for (int attrNdx = 0; attrNdx < (int)layout.attributes.size(); ++attrNdx)
        copyToLayout(&localBuf[0], layout.attributes[attrNdx], srcArrays[attrNdx]);

    gl.bindBuffer(GL_ARRAY_BUFFER, buffer);
    gl.bufferData(GL_ARRAY_BUFFER, (int)localBuf.size(), &localBuf[0], usage);
    gl.bindBuffer(GL_ARRAY_BUFFER, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Uploading buffer data failed");
}

// VertexBuffer

VertexBuffer::VertexBuffer(const RenderContext &context, int numBindings, const VertexArrayBinding *bindings, Type type)
    : Buffer(context)
{
    const glw::Functions &gl = context.getFunctions();
    const uint32_t usage     = GL_STATIC_DRAW;
    VertexBufferLayout layout;

    if (!areVertexArrayLocationsValid(bindings, bindings + numBindings))
        throw tcu::TestError("Invalid vertex array locations");

    if (type == TYPE_PLANAR)
        computeNonStridedBufferLayout(layout, bindings, bindings + numBindings);
    else
        throw tcu::InternalError("Strided layout is not yet supported");

    std::vector<VertexArrayPointer> srcPtrs(numBindings);
    for (int ndx = 0; ndx < numBindings; ndx++)
        srcPtrs[ndx] = bindings[ndx].pointer;

    DE_ASSERT(srcPtrs.size() == layout.attributes.size());
    if (!srcPtrs.empty())
        uploadBufferData(gl, m_object, usage, layout, &srcPtrs[0]);

    // Construct descriptor.
    m_layout.buffer     = m_object;
    m_layout.attributes = layout.attributes;
}

VertexBuffer::~VertexBuffer(void)
{
}

// IndexBuffer

IndexBuffer::IndexBuffer(const RenderContext &context, IndexType indexType, int numIndices, const void *indices)
    : Buffer(context)
{
    const glw::Functions &gl = context.getFunctions();
    const uint32_t usage     = GL_STATIC_DRAW;

    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_object);
    gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * getIndexSize(indexType), indices, usage);
    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    GLU_EXPECT_NO_ERROR(gl.getError(), "Uploading index data failed");
}

IndexBuffer::~IndexBuffer(void)
{
}

static inline VertexAttributeDescriptor getUserPointerDescriptor(const VertexArrayBinding &vertexArray)
{
    DE_ASSERT(vertexArray.binding.type == BindingPoint::BPTYPE_LOCATION);

    return VertexAttributeDescriptor(vertexArray.binding.location, vertexArray.pointer.componentType,
                                     vertexArray.pointer.convert, vertexArray.pointer.numComponents,
                                     vertexArray.pointer.numElements, vertexArray.pointer.stride,
                                     vertexArray.pointer.data);
}

//! Setup VA according to allocation spec. Assumes that other state (VAO binding, buffer) is set already.
static void setVertexAttribPointer(const glw::Functions &gl, const VertexAttributeDescriptor &va)
{
    const bool isIntType      = de::inRange<int>(va.componentType, VTX_COMP_UNSIGNED_INT8, VTX_COMP_SIGNED_INT32);
    const bool isSpecialType  = de::inRange<int>(va.componentType, VTX_COMP_FIXED, VTX_COMP_FLOAT);
    const uint32_t compTypeGL = getVtxCompGLType(va.componentType);

    DE_ASSERT(isIntType != isSpecialType);                       // Must be either int or special type.
    DE_ASSERT(isIntType || va.convert == VTX_COMP_CONVERT_NONE); // Conversion allowed only for special types.
    DE_UNREF(isSpecialType);

    gl.enableVertexAttribArray(va.location);

    if (isIntType && va.convert == VTX_COMP_CONVERT_NONE)
        gl.vertexAttribIPointer(va.location, va.numComponents, compTypeGL, va.stride, va.pointer);
    else
        gl.vertexAttribPointer(va.location, va.numComponents, compTypeGL,
                               va.convert == VTX_COMP_CONVERT_NORMALIZE_TO_FLOAT ? GL_TRUE : GL_FALSE, va.stride,
                               va.pointer);
}

//! Setup vertex buffer and attributes.
static void setVertexBufferAttributes(const glw::Functions &gl, const VertexBufferDescriptor &buffer)
{
    gl.bindBuffer(GL_ARRAY_BUFFER, buffer.buffer);

    for (std::vector<VertexAttributeDescriptor>::const_iterator vaIter = buffer.attributes.begin();
         vaIter != buffer.attributes.end(); ++vaIter)
        setVertexAttribPointer(gl, *vaIter);

    gl.bindBuffer(GL_ARRAY_BUFFER, 0);
}

static void disableVertexArrays(const glw::Functions &gl, const std::vector<VertexArrayBinding> &bindings)
{
    for (std::vector<VertexArrayBinding>::const_iterator vaIter = bindings.begin(); vaIter != bindings.end(); ++vaIter)
    {
        DE_ASSERT(vaIter->binding.type == BindingPoint::BPTYPE_LOCATION);
        gl.disableVertexAttribArray(vaIter->binding.location);
    }
}

#if defined(DE_DEBUG)
static bool isProgramActive(const RenderContext &context, uint32_t program)
{
    // \todo [2013-05-08 pyry] Is this query broken?
    /*    uint32_t activeProgram = 0;
        context.getFunctions().getIntegerv(GL_ACTIVE_PROGRAM, (int*)&activeProgram);
        GLU_EXPECT_NO_ERROR(context.getFunctions().getError(), "oh");
        return activeProgram == program;*/
    DE_UNREF(context);
    DE_UNREF(program);
    return true;
}

static bool isDrawCallValid(int numVertexArrays, const VertexArrayBinding *vertexArrays,
                            const PrimitiveList &primitives)
{
    if (numVertexArrays < 0)
        return false;

    if ((primitives.indexType == INDEXTYPE_LAST) != (primitives.indices == 0))
        return false;

    if (primitives.numElements < 0)
        return false;

    if (!primitives.indices)
    {
        for (int ndx = 0; ndx < numVertexArrays; ndx++)
        {
            if (primitives.numElements > vertexArrays[ndx].pointer.numElements)
                return false;
        }
    }
    // \todo [2013-05-08 pyry] We could walk whole index array and determine index range

    return true;
}
#endif // DE_DEBUG

static inline void drawNonIndexed(const glw::Functions &gl, PrimitiveType type, int numElements)
{
    uint32_t mode = getPrimitiveGLType(type);
    gl.drawArrays(mode, 0, numElements);
}

static inline void drawIndexed(const glw::Functions &gl, PrimitiveType type, int numElements, IndexType indexType,
                               const void *indexPtr)
{
    uint32_t mode        = getPrimitiveGLType(type);
    uint32_t indexGLType = getIndexGLType(indexType);

    gl.drawElements(mode, numElements, indexGLType, indexPtr);
}

} // namespace

void drawFromUserPointers(const RenderContext &context, uint32_t program, int numVertexArrays,
                          const VertexArrayBinding *vertexArrays, const PrimitiveList &primitives,
                          DrawUtilCallback *callback)
{
    const glw::Functions &gl = context.getFunctions();
    std::vector<VertexArrayBinding> bindingsWithLocations;

    DE_ASSERT(isDrawCallValid(numVertexArrays, vertexArrays, primitives));
    DE_ASSERT(isProgramActive(context, program));

    // Lower bindings to locations.
    namedBindingsToProgramLocations(gl, program, vertexArrays, vertexArrays + numVertexArrays,
                                    std::inserter(bindingsWithLocations, bindingsWithLocations.begin()));

    TCU_CHECK(areVertexArrayLocationsValid(bindingsWithLocations.begin(), bindingsWithLocations.end()));

    // Set VA state.
    for (std::vector<VertexArrayBinding>::const_iterator vaIter = bindingsWithLocations.begin();
         vaIter != bindingsWithLocations.end(); ++vaIter)
        setVertexAttribPointer(gl, getUserPointerDescriptor(*vaIter));

    if (callback)
        callback->beforeDrawCall();

    if (primitives.indices)
        drawIndexed(gl, primitives.type, primitives.numElements, primitives.indexType, primitives.indices);
    else
        drawNonIndexed(gl, primitives.type, primitives.numElements);

    if (callback)
        callback->afterDrawCall();

    // Disable attribute arrays or otherwise someone later on might get crash thanks to invalid pointers.
    disableVertexArrays(gl, bindingsWithLocations);
}

void drawFromBuffers(const RenderContext &context, uint32_t program, int numVertexArrays,
                     const VertexArrayBinding *vertexArrays, const PrimitiveList &primitives,
                     DrawUtilCallback *callback)
{
    const glw::Functions &gl = context.getFunctions();
    std::vector<VertexArrayBinding> bindingsWithLocations;

    DE_ASSERT(isDrawCallValid(numVertexArrays, vertexArrays, primitives));
    DE_ASSERT(isProgramActive(context, program));

    // Lower bindings to locations.
    namedBindingsToProgramLocations(gl, program, vertexArrays, vertexArrays + numVertexArrays,
                                    std::inserter(bindingsWithLocations, bindingsWithLocations.begin()));

    TCU_CHECK(areVertexArrayLocationsValid(bindingsWithLocations.begin(), bindingsWithLocations.end()));

    // Create buffers for duration of draw call.
    {
        VertexBuffer vertexBuffer(context, (int)bindingsWithLocations.size(),
                                  (bindingsWithLocations.empty()) ? (nullptr) : (&bindingsWithLocations[0]));

        // Set state.
        setVertexBufferAttributes(gl, vertexBuffer.getDescriptor());

        if (primitives.indices)
        {
            IndexBuffer indexBuffer(context, primitives.indexType, primitives.numElements, primitives.indices);

            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, *indexBuffer);

            if (callback)
                callback->beforeDrawCall();

            drawIndexed(gl, primitives.type, primitives.numElements, primitives.indexType, 0);

            if (callback)
                callback->afterDrawCall();

            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }
        else
        {
            if (callback)
                callback->beforeDrawCall();

            drawNonIndexed(gl, primitives.type, primitives.numElements);

            if (callback)
                callback->afterDrawCall();
        }
    }

    // Disable attribute arrays or otherwise someone later on might get crash thanks to invalid pointers.
    for (std::vector<VertexArrayBinding>::const_iterator vaIter = bindingsWithLocations.begin();
         vaIter != bindingsWithLocations.end(); ++vaIter)
        gl.disableVertexAttribArray(vaIter->binding.location);
}

void drawFromVAOBuffers(const RenderContext &context, uint32_t program, int numVertexArrays,
                        const VertexArrayBinding *vertexArrays, const PrimitiveList &primitives,
                        DrawUtilCallback *callback)
{
    const glw::Functions &gl = context.getFunctions();
    VertexArray vao(context);

    gl.bindVertexArray(*vao);
    drawFromBuffers(context, program, numVertexArrays, vertexArrays, primitives, callback);
    gl.bindVertexArray(0);
}

void draw(const RenderContext &context, uint32_t program, int numVertexArrays, const VertexArrayBinding *vertexArrays,
          const PrimitiveList &primitives, DrawUtilCallback *callback)
{
    const glu::ContextType ctxType = context.getType();

    if (isContextTypeGLCore(ctxType) || contextSupports(ctxType, ApiType::es(3, 1)))
        drawFromVAOBuffers(context, program, numVertexArrays, vertexArrays, primitives, callback);
    else
    {
        DE_ASSERT(isContextTypeES(ctxType));
        drawFromUserPointers(context, program, numVertexArrays, vertexArrays, primitives, callback);
    }
}

} // namespace glu
