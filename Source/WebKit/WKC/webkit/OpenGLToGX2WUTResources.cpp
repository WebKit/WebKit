#include "OpenGLToGX2WUTResources.h"
#include "OpenGLToGX2VertexArrays.h"

#if defined(__WUT__)
#include <gx2r/resource.h>
#include <gx2/draw.h>

#include <cstring>
#include <limits>
#include <new>

namespace WKC {
namespace GLGX2 {
namespace {

bool createBuffer(void* context, BufferName name, size_t size, BufferUsage)
{
    if (!name || size > std::numeric_limits<uint32_t>::max())
        return false;

    auto& resources = *static_cast<WUTBufferResources*>(context);
    auto existing = resources.buffers.find(name);
    if (existing != resources.buffers.end())
        return false;

    auto* buffer = new (std::nothrow) GX2RBuffer {};
    if (!buffer)
        return false;

    // The allocation is writable by the CPU and readable by the GPU. Declare
    // all common GL buffer roles up front because glBufferData can precede the
    // first target-specific bind in the GL object model.
    buffer->flags = static_cast<GX2RResourceFlags>(
        GX2R_RESOURCE_BIND_VERTEX_BUFFER
        | GX2R_RESOURCE_BIND_INDEX_BUFFER
        | GX2R_RESOURCE_BIND_UNIFORM_BLOCK
        | GX2R_RESOURCE_USAGE_CPU_WRITE
        | GX2R_RESOURCE_USAGE_CPU_READ
        | GX2R_RESOURCE_USAGE_GPU_READ);
    buffer->elemSize = 1;
    // GL permits a zero-byte data store. GX2R expects a real allocation, so
    // reserve one inert byte natively while BufferObjects retains the logical
    // zero-byte size and rejects any attempt to access it.
    buffer->elemCount = static_cast<uint32_t>(size ? size : 1);

    if (!GX2RCreateBuffer(buffer)) {
        delete buffer;
        return false;
    }

    try {
        resources.buffers.emplace(name, buffer);
    } catch (const std::bad_alloc&) {
        GX2RDestroyBufferEx(buffer, buffer->flags);
        delete buffer;
        return false;
    }
    return true;
}

bool uploadBuffer(void* context, BufferName name, size_t offset, const void* data, size_t size)
{
    auto& resources = *static_cast<WUTBufferResources*>(context);
    auto it = resources.buffers.find(name);
    if (it == resources.buffers.end() || (size && !data))
        return false;

    GX2RBuffer* buffer = it->second;
    const size_t capacity = static_cast<size_t>(buffer->elemSize) * buffer->elemCount;
    if (offset > capacity || size > capacity - offset)
        return false;
    if (!size)
        return true;

    void* memory = GX2RLockBufferEx(buffer,
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));
    if (!memory)
        return false;
    std::memcpy(static_cast<uint8_t*>(memory) + offset, data, size);
    GX2RUnlockBufferEx(buffer,
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));
    return true;
}

bool destroyBuffer(void* context, BufferName name)
{
    auto& resources = *static_cast<WUTBufferResources*>(context);
    auto it = resources.buffers.find(name);
    if (it == resources.buffers.end())
        return false;

    GX2RBuffer* buffer = it->second;
    GX2RDestroyBufferEx(buffer, buffer->flags);
    delete buffer;
    resources.buffers.erase(it);
    for (auto& binding : resources.bindings) {
        if (binding.second == name)
            binding.second = 0;
    }
    return true;
}

bool bindBuffer(void* context, BufferTarget target, BufferName name)
{
    auto& resources = *static_cast<WUTBufferResources*>(context);
    if (name && resources.buffers.find(name) == resources.buffers.end())
        return false;
    resources.bindings[static_cast<uint8_t>(target)] = name;
    return true;
}

// Expose the actual GX2R allocation address to the indexed draw path. GX2's
// indexed API takes the index stream pointer separately from its byte offset;
// passing nullptr here would draw from no index data even when an element
// buffer object had been bound in the GL-facing state.
bool drawIndexed(void* context, const IndexedDraw& draw, BufferName elementBuffer)
{
    if (!elementBuffer || draw.baseVertex != 0)
        return false;

    auto& resources = *static_cast<WUTBufferResources*>(context);
    auto it = resources.buffers.find(elementBuffer);
    if (it == resources.buffers.end() || !it->second || !it->second->buffer)
        return false;
    if (draw.indexBytes == 1)
        return false; // GX2 has no U8 index type; widen in a staging buffer first.

    GX2PrimitiveMode mode;
    switch (draw.primitive) {
    case Primitive::Points: mode = GX2_PRIMITIVE_MODE_POINTS; break;
    case Primitive::Lines: mode = GX2_PRIMITIVE_MODE_LINES; break;
    case Primitive::LineStrip: mode = GX2_PRIMITIVE_MODE_LINE_STRIP; break;
    case Primitive::Triangles: mode = GX2_PRIMITIVE_MODE_TRIANGLES; break;
    case Primitive::TriangleStrip: mode = GX2_PRIMITIVE_MODE_TRIANGLE_STRIP; break;
    case Primitive::TriangleFan: mode = GX2_PRIMITIVE_MODE_TRIANGLE_FAN; break;
    default: return false;
    }

    GX2IndexType type;
    if (draw.indexBytes == 2)
        type = GX2_INDEX_TYPE_U16_LE;
    else if (draw.indexBytes == 4)
        type = GX2_INDEX_TYPE_U32_LE;
    else
        return false;

    GX2DrawIndexedEx(mode, draw.count, type, it->second->buffer,
        static_cast<uint32_t>(draw.indexOffset), draw.instances);
    return true;
}

} // namespace

BufferBackend makeWUTBufferBackend(WUTBufferResources& resources)
{
    BufferBackend backend;
    backend.context = &resources;
    backend.create = createBuffer;
    backend.upload = uploadBuffer;
    backend.destroy = destroyBuffer;
    backend.bind = bindBuffer;
    return backend;
}

bool bindVertexBuffer(void* context, uint8_t attribute, BufferName name,
    uint32_t stride, size_t offset)
{
    auto& resources = *static_cast<WUTBufferResources*>(context);
    auto it = resources.buffers.find(name);
    if (it == resources.buffers.end() || !it->second || !it->second->buffer)
        return false;

    GX2RBuffer* buffer = it->second;
    const size_t capacity = static_cast<size_t>(buffer->elemSize) * buffer->elemCount;
    if (offset > capacity || offset > UINT32_MAX || capacity - offset > UINT32_MAX)
        return false;
    GX2SetAttribBuffer(attribute, static_cast<uint32_t>(capacity - offset), stride,
        static_cast<const uint8_t*>(buffer->buffer) + offset);
    return true;
}

bool translateVertexAttributeFormat(const VertexAttribute& value, GX2AttribFormat& output)
{
    if (value.components < 1 || value.components > 4)
        return false;

    const bool signedType = value.type == AttributeType::Byte
        || value.type == AttributeType::Short || value.type == AttributeType::Int;
    const bool unsignedType = value.type == AttributeType::UnsignedByte
        || value.type == AttributeType::UnsignedShort || value.type == AttributeType::UnsignedInt;
    const bool floatingType = value.type == AttributeType::Float
        || value.type == AttributeType::HalfFloat;
    if (!signedType && !unsignedType && !floatingType)
        return false;

    // GX2 lacks native three-component packed 8-bit and 16-bit formats.
    // Use the corresponding four-component fetch format as a temporary mapping;
    // a later staging/expansion path must supply the missing fourth component
    // without reading beyond the GL attribute's declared data.

    uint32_t typeBits = 0;
    switch (value.type) {
    case AttributeType::Byte:
    case AttributeType::UnsignedByte:
        switch (value.components) {
        case 1: typeBits = GX2_ATTRIB_TYPE_8; break;
        case 2: typeBits = GX2_ATTRIB_TYPE_8_8; break;
        case 3: // Temporary GX2 four-component format; expansion path still needed.
        case 4: typeBits = GX2_ATTRIB_TYPE_8_8_8_8; break;
        default: return false;
        }
        break;
    case AttributeType::Short:
    case AttributeType::UnsignedShort:
        switch (value.components) {
        case 1: typeBits = GX2_ATTRIB_TYPE_16; break;
        case 2: typeBits = GX2_ATTRIB_TYPE_16_16; break;
        case 3: // Temporary GX2 four-component format; expansion path still needed.
        case 4: typeBits = GX2_ATTRIB_TYPE_16_16_16_16; break;
        default: return false;
        }
        break;
    case AttributeType::Int:
    case AttributeType::UnsignedInt:
        switch (value.components) {
        case 1: typeBits = GX2_ATTRIB_TYPE_32; break;
        case 2: typeBits = GX2_ATTRIB_TYPE_32_32; break;
        case 3: typeBits = GX2_ATTRIB_TYPE_32_32_32; break;
        case 4: typeBits = GX2_ATTRIB_TYPE_32_32_32_32; break;
        default: return false;
        }
        break;
    case AttributeType::Float:
        switch (value.components) {
        case 1: typeBits = GX2_ATTRIB_TYPE_32_FLOAT; break;
        case 2: typeBits = GX2_ATTRIB_TYPE_32_32_FLOAT; break;
        case 3: typeBits = GX2_ATTRIB_TYPE_32_32_32_FLOAT; break;
        case 4: typeBits = GX2_ATTRIB_TYPE_32_32_32_32_FLOAT; break;
        default: return false;
        }
        break;
    case AttributeType::HalfFloat:
        switch (value.components) {
        case 1: typeBits = GX2_ATTRIB_TYPE_16_FLOAT; break;
        case 2: typeBits = GX2_ATTRIB_TYPE_16_16_FLOAT; break;
        case 3: // Temporary GX2 four-component format; expansion path still needed.
        case 4: typeBits = GX2_ATTRIB_TYPE_16_16_16_16_FLOAT; break;
        default: return false;
        }
        break;
    }

    uint32_t flags = 0;
    if (value.type == AttributeType::Float || value.type == AttributeType::HalfFloat) {
        output = static_cast<GX2AttribFormat>(typeBits
            | (value.type == AttributeType::Float ? GX2_ATTRIB_FLAG_SCALED : 0));
        return true;
    }
    if (value.normalized) {
        if (signedType)
            flags |= GX2_ATTRIB_FLAG_SIGNED;
    } else {
        flags |= GX2_ATTRIB_FLAG_INTEGER;
        if (signedType)
            flags |= GX2_ATTRIB_FLAG_SIGNED;
    }
    output = static_cast<GX2AttribFormat>(typeBits | flags);
    return true;
}

bool setAttribute(void* context, uint8_t attribute, const VertexAttribute& value)
{
    GX2AttribFormat format;
    if (attribute >= 32 || !translateVertexAttributeFormat(value, format))
        return false;

    auto& resources = *static_cast<WUTBufferResources*>(context);
    WUTBufferResources::AttributeDescriptor descriptor;
    descriptor.format = format;
    descriptor.buffer = value.buffer;
    descriptor.components = value.components;
    descriptor.sourceType = value.type;
    descriptor.normalized = value.normalized;
    descriptor.stride = value.stride;
    descriptor.offset = value.offset;
    descriptor.componentMask = value.components == 1 ? GX2_CHANNEL_MASK_R
        : value.components == 2 ? GX2_CHANNEL_MASK_RG
        : value.components == 3 ? GX2_CHANNEL_MASK_RGB : GX2_CHANNEL_MASK_RGBA;
    descriptor.divisor = value.divisor;
    descriptor.indexType = value.divisor
        ? GX2_ATTRIB_INDEX_PER_INSTANCE : GX2_ATTRIB_INDEX_PER_VERTEX;
    descriptor.enabled = value.enabled;
    resources.attributes[attribute] = descriptor;
    return true;
}

bool buildWUTAttributeStreams(const WUTBufferResources& resources, std::vector<GX2AttribStream>& output)
{
    output.clear();
    try {
        output.reserve(resources.attributes.size());
        for (const auto& entry : resources.attributes) {
            const uint8_t location = entry.first;
            const auto& attribute = entry.second;
            if (!attribute.enabled)
                continue;
            if (!attribute.buffer || resources.buffers.find(attribute.buffer) == resources.buffers.end()) {
                output.clear();
                return false;
            }
            GX2AttribStream stream {};
            stream.location = location;
            stream.buffer = location;
            stream.offset = 0; // bindVertexBuffer already applies the GL byte offset.
            stream.format = attribute.format;
            stream.type = attribute.indexType;
            stream.aluDivisor = attribute.divisor;
            stream.mask = attribute.componentMask;
            stream.endianSwap = GX2_ENDIAN_SWAP_DEFAULT;
            output.push_back(stream);
        }
    } catch (...) {
        output.clear();
        return false;
    }
    return true;
}

bool initializeWUTFetchShader(const WUTBufferResources& resources, GX2FetchShader& shader,
    void* programBuffer, uint32_t bufferSize, GX2FetchShaderType type,
    GX2TessellationMode tessellationMode)
{
    if (!programBuffer)
        return false;

    std::vector<GX2AttribStream> streams;
    if (!buildWUTAttributeStreams(resources, streams))
        return false;
    if (streams.empty())
        return false;

    const uint32_t requiredSize = GX2CalcFetchShaderSizeEx(
        static_cast<uint32_t>(streams.size()), type, tessellationMode);
    if (!requiredSize || bufferSize < requiredSize
        || (reinterpret_cast<uintptr_t>(programBuffer) & (GX2_SHADER_PROGRAM_ALIGNMENT - 1)))
        return false;

    GX2InitFetchShaderEx(&shader, static_cast<uint8_t*>(programBuffer),
        static_cast<uint32_t>(streams.size()), streams.data(), type, tessellationMode);
    GX2SetFetchShader(&shader);
    return true;
}

bool setElementBuffer(void* context, BufferName name)
{
    if (!name)
        return true;
    auto& resources = *static_cast<WUTBufferResources*>(context);
    auto it = resources.buffers.find(name);
    return it != resources.buffers.end() && it->second && it->second->buffer;
}

bool drawVertexArrayIndexed(void* context, const IndexedDraw& draw, BufferName elementBuffer)
{
    if (!elementBuffer || draw.indexOffset > UINT32_MAX
        || (draw.indexBytes != 1 && draw.indexBytes != 2 && draw.indexBytes != 4)
        || draw.count > UINT32_MAX / sizeof(uint32_t))
        return false;

    auto& resources = *static_cast<WUTBufferResources*>(context);
    auto it = resources.buffers.find(elementBuffer);
    if (it == resources.buffers.end() || !it->second || !it->second->buffer)
        return false;
    GX2RBuffer* source = it->second;
    const size_t capacity = static_cast<size_t>(source->elemSize) * source->elemCount;
    const size_t indexDataSize = static_cast<size_t>(draw.count) * draw.indexBytes;
    if (draw.indexOffset > capacity || indexDataSize > capacity - draw.indexOffset)
        return false;

    GX2PrimitiveMode mode;
    switch (draw.primitive) {
    case Primitive::Points: mode = GX2_PRIMITIVE_MODE_POINTS; break;
    case Primitive::Lines: mode = GX2_PRIMITIVE_MODE_LINES; break;
    case Primitive::LineStrip: mode = GX2_PRIMITIVE_MODE_LINE_STRIP; break;
    case Primitive::Triangles: mode = GX2_PRIMITIVE_MODE_TRIANGLES; break;
    case Primitive::TriangleStrip: mode = GX2_PRIMITIVE_MODE_TRIANGLE_STRIP; break;
    case Primitive::TriangleFan: mode = GX2_PRIMITIVE_MODE_TRIANGLE_FAN; break;
    default: return false;
    }

    // GX2 does not support GL's U8 indices or base-vertex draw semantics.
    // Normalize every index to U32 in a temporary GX2R index buffer, applying
    // baseVertex while staging. This also ensures the submitted byte offset is
    // zero and the index pointer always refers to a real GPU-visible buffer.
    void* sourceMemory = GX2RLockBufferEx(source,
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_READ));
    if (!sourceMemory)
        return false;

    std::vector<uint32_t> indices;
    try {
        indices.resize(draw.count);
    } catch (...) {
        GX2RUnlockBufferEx(source,
            static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_READ));
        return false;
    }

    const auto* bytes = static_cast<const uint8_t*>(sourceMemory) + draw.indexOffset;
    uint32_t maximumIndex = 0;
    for (uint32_t i = 0; i < draw.count; ++i) {
        uint32_t index = 0;
        if (draw.indexBytes == 1) {
            index = bytes[i];
        } else if (draw.indexBytes == 2) {
            uint16_t value;
            std::memcpy(&value, bytes + static_cast<size_t>(i) * 2, sizeof(value));
            index = value;
        } else {
            std::memcpy(&index, bytes + static_cast<size_t>(i) * 4, sizeof(index));
        }
        const int64_t adjusted = static_cast<int64_t>(index) + draw.baseVertex;
        if (adjusted < 0 || adjusted > UINT32_MAX) {
            GX2RUnlockBufferEx(source,
                static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_READ));
            return false;
        }
        indices[i] = static_cast<uint32_t>(adjusted);
        if (indices[i] > maximumIndex)
            maximumIndex = indices[i];
    }
    GX2RUnlockBufferEx(source,
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_READ));

    // Validate the actual referenced vertex range, rather than approximating
    // it with draw.count. Instance-divisor attributes use instance indices.
    for (const auto& entry : resources.attributes) {
        const auto& attribute = entry.second;
        if (!attribute.enabled)
            continue;
        auto vertexBuffer = resources.buffers.find(attribute.buffer);
        if (!attribute.buffer || vertexBuffer == resources.buffers.end() || !vertexBuffer->second)
            return false;
        const size_t vertexCapacity = static_cast<size_t>(vertexBuffer->second->elemSize)
            * vertexBuffer->second->elemCount;
        const size_t scalarSize = attribute.sourceType == AttributeType::Byte
            || attribute.sourceType == AttributeType::UnsignedByte ? 1
            : attribute.sourceType == AttributeType::Short
                || attribute.sourceType == AttributeType::UnsignedShort
                || attribute.sourceType == AttributeType::HalfFloat ? 2 : 4;
        const size_t elementSize = scalarSize * attribute.components;
        const size_t stride = attribute.stride ? attribute.stride : elementSize;
        if (attribute.offset > vertexCapacity || elementSize > vertexCapacity - attribute.offset)
            return false;
        const size_t available = vertexCapacity - attribute.offset;
        const uint64_t lastElement = attribute.divisor
            ? (draw.instances ? (draw.instances - 1) / attribute.divisor : 0)
            : maximumIndex;
        if (lastElement > (available - elementSize) / stride)
            return false;
    }

    GX2RBuffer staged {};
    staged.flags = static_cast<GX2RResourceFlags>(
        GX2R_RESOURCE_BIND_INDEX_BUFFER
        | GX2R_RESOURCE_USAGE_CPU_WRITE
        | GX2R_RESOURCE_USAGE_GPU_READ);
    staged.elemSize = sizeof(uint32_t);
    staged.elemCount = draw.count;
    if (!GX2RCreateBuffer(&staged))
        return false;
    void* stagedMemory = GX2RLockBufferEx(&staged,
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));
    if (!stagedMemory) {
        GX2RDestroyBufferEx(&staged, staged.flags);
        return false;
    }
    std::memcpy(stagedMemory, indices.data(), indices.size() * sizeof(uint32_t));
    GX2RUnlockBufferEx(&staged,
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));

    GX2DrawIndexedEx(mode, draw.count, GX2_INDEX_TYPE_U32_LE,
        staged.buffer, 0, draw.instances);
    GX2RDestroyBufferEx(&staged, staged.flags);
    return true;
}

VertexArrayBackend makeWUTVertexArrayBackend(WUTBufferResources& resources)
{
    VertexArrayBackend backend;
    backend.context = &resources;
    backend.bindVertexBuffer = bindVertexBuffer;
    backend.setAttribute = setAttribute;
    backend.setElementBuffer = setElementBuffer;
    backend.drawIndexed = drawVertexArrayIndexed;
    return backend;
}

} // namespace GLGX2
} // namespace WKC
#endif
