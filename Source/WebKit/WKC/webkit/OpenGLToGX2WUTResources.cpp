#include "OpenGLToGX2WUTResources.h"
#include "OpenGLToGX2VertexArrays.h"

#if defined(__WUT__)
#include <gx2r/resource.h>
#include <gx2/draw.h>
#include <gx2/event.h>
#include <gx2/shaders.h>
#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace WKC {
namespace GLGX2 {
namespace {

bool getBufferCapacity(const GX2RBuffer& buffer, size_t& capacity)
{
    const size_t elementSize = buffer.elemSize;
    const size_t elementCount = buffer.elemCount;
    if (elementCount && elementSize > std::numeric_limits<size_t>::max() / elementCount)
        return false;
    capacity = elementSize * elementCount;
    return true;
}

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
    size_t capacity = 0;
    if (!getBufferCapacity(*buffer, capacity))
        return false;
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
    // GL permits deleting a buffer immediately after submitting a draw that
    // references it. GX2 can still be fetching from the allocation after the
    // CPU-side draw call returns, so retire queued GPU work before freeing the
    // backing storage rather than leaving an in-flight fetch with a dangling
    // resource address.
    GX2DrawDone();
    GX2RDestroyBufferEx(buffer, buffer->flags);
    delete buffer;
    resources.buffers.erase(it);
    for (auto& binding : resources.bindings) {
        if (binding.second == name)
            binding.second = 0;
    }
    for (auto& attribute : resources.attributes) {
        if (attribute.second.buffer == name) {
            attribute.second.buffer = 0;
            attribute.second.enabled = false;
            resources.fetchShaderDirty = true;
        }
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

// Indexed drawing is implemented by drawVertexArrayIndexed below. That path
// widens unsupported index formats, applies baseVertex, validates every buffer
// range, and keeps staging allocations alive until GX2DrawDone().

} // namespace

WUTBufferResources::WUTBufferResources()
{
    for (auto& value : currentAttributeValues)
        value[3] = 1.0f;
}

WUTBufferResources::~WUTBufferResources()
{
    // GX2 may still be fetching this program after the CPU-side draw call has
    // returned. Drain queued work before releasing shader program storage.
    if (fetchShaderProgramBuffer) {
        GX2DrawDone();
        MEMFreeToDefaultHeap(fetchShaderProgramBuffer);
    }
    fetchShaderProgramBuffer = nullptr;
    fetchShaderProgramBufferSize = 0;
}

bool setWUTLinkedProgram(WUTBufferResources& resources, const GX2VertexShader& vertexShader,
    const GX2PixelShader& pixelShader, GX2ShaderMode mode,
    const WUTLinkedAttribute* bindings, size_t bindingCount)
{
    if (!vertexShader.program || !vertexShader.size || !pixelShader.program || !pixelShader.size
        || vertexShader.mode != mode || pixelShader.mode != mode
        || (bindingCount && !bindings))
        return false;

    std::vector<WUTBufferResources::AttributeBinding> linked;
    try {
        linked.reserve(bindingCount);
        for (size_t i = 0; i < bindingCount; ++i) {
            // Attribute indices are later used to index the context's generic
            // attribute-value array and to bind GX2 fetch streams. Reject
            // invalid GL locations here instead of risking an out-of-bounds
            // access immediately before a draw.
            if (bindings[i].attributeIndex >= resources.currentAttributeValues.size()
                || !bindings[i].shaderInputName || !*bindings[i].shaderInputName)
                return false;
            for (size_t j = 0; j < i; ++j) {
                if (bindings[j].attributeIndex == bindings[i].attributeIndex
                    || !std::strcmp(bindings[j].shaderInputName, bindings[i].shaderInputName))
                    return false;
            }
            linked.push_back({ bindings[i].attributeIndex, bindings[i].shaderInputName });
        }
    } catch (...) {
        return false;
    }

    if (vertexShader.attribVarCount && !vertexShader.attribVars)
        return false;

    // Every compiler-visible GX2 vertex input must have an explicit GL-index
    // mapping. Requiring a complete map avoids accidentally drawing with no
    // streams just because GL locations differ from GX2's locations.
    for (uint32_t i = 0; i < vertexShader.attribVarCount; ++i) {
        const char* inputName = vertexShader.attribVars[i].name;
        if (!inputName)
            return false;
        bool found = false;
        for (const auto& binding : linked) {
            if (binding.shaderInputName == inputName) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }

    // The caller retains ownership of the compiler's GX2 shader objects. This
    // context keeps references until the linked program is replaced or deleted.
    resources.vertexShader = &vertexShader;
    resources.pixelShader = &pixelShader;
    resources.shaderMode = mode;
    resources.linkedAttributes = std::move(linked);
    resources.fetchShaderDirty = true;
    return true;
}

bool prepareWUTProgramForDraw(WUTBufferResources& resources)
{
    if (!resources.vertexShader || !resources.pixelShader
        || !resources.vertexShader->program || !resources.vertexShader->size
        || !resources.pixelShader->program || !resources.pixelShader->size
        || resources.vertexShader->mode != resources.shaderMode
        || resources.pixelShader->mode != resources.shaderMode)
        return false;

    if (resources.fetchShaderDirty) {
        // Fetch shader code may still be in use by prior submitted draws. Do not
        // rewrite or free its backing allocation until GX2 has finished with it.
        if (resources.fetchShaderProgramBuffer)
            GX2DrawDone();

        std::vector<WUTLinkedAttribute> bindings;
        std::vector<GX2AttribStream> streams;
        try {
            bindings.reserve(resources.linkedAttributes.size());
            for (const auto& binding : resources.linkedAttributes)
                bindings.push_back({ binding.attributeIndex, binding.shaderInputName.c_str() });
        } catch (...) {
            return false;
        }

        // An empty vertex-input interface is legal (for example, shaders that
        // synthesize positions). Build an empty fetch program rather than
        // assuming there must be at least one enabled GL attribute.
        if (!buildWUTAttributeStreams(resources, *resources.vertexShader,
                bindings.data(), bindings.size(), streams)) {
            if (!streams.empty())
                return false;
            for (const auto& entry : resources.attributes) {
                if (entry.second.enabled)
                    return false;
            }
        }

        const auto fetchType = GX2_FETCH_SHADER_TESSELLATION_NONE;
        const auto tessellationMode = GX2_TESSELLATION_MODE_DISCRETE;
        const uint32_t requiredSize = GX2CalcFetchShaderSizeEx(
            static_cast<uint32_t>(streams.size()), fetchType, tessellationMode);
        if (!requiredSize)
            return false;

        if (!resources.fetchShaderProgramBuffer
            || resources.fetchShaderProgramBufferSize < requiredSize) {
            void* replacement = MEMAllocFromDefaultHeapEx(requiredSize, GX2_SHADER_PROGRAM_ALIGNMENT);
            if (!replacement)
                return false;
            if (resources.fetchShaderProgramBuffer)
                MEMFreeToDefaultHeap(resources.fetchShaderProgramBuffer);
            resources.fetchShaderProgramBuffer = replacement;
            resources.fetchShaderProgramBufferSize = requiredSize;
        }

        if (reinterpret_cast<uintptr_t>(resources.fetchShaderProgramBuffer)
            & (GX2_SHADER_PROGRAM_ALIGNMENT - 1))
            return false;

        GX2InitFetchShaderEx(&resources.fetchShader,
            static_cast<uint8_t*>(resources.fetchShaderProgramBuffer),
            static_cast<uint32_t>(streams.size()), streams.data(), fetchType, tessellationMode);
        DCFlushRange(resources.fetchShaderProgramBuffer, requiredSize);
        resources.fetchShaderDirty = false;
    }

    GX2SetShaderMode(resources.shaderMode);
    GX2SetVertexShader(resources.vertexShader);
    GX2SetPixelShader(resources.pixelShader);
    GX2SetFetchShader(&resources.fetchShader);

    // Disabled arrays use GL's current generic attribute values. They are
    // context state rather than VAO state, so refresh the zero-stride streams
    // immediately before each draw and after other VAOs have reused these slots.
    for (const auto& binding : resources.linkedAttributes) {
        auto attribute = resources.attributes.find(binding.attributeIndex);
        if (attribute != resources.attributes.end() && attribute->second.enabled)
            continue;
        auto& value = resources.currentAttributeValues[binding.attributeIndex];
        DCFlushRange(value.data(), sizeof(value));
        GX2SetAttribBuffer(binding.attributeIndex, static_cast<uint32_t>(sizeof(value)), 0, value.data());
    }
    return true;
}

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
    size_t capacity = 0;
    if (!getBufferCapacity(*buffer, capacity)
        || offset > capacity || offset > UINT32_MAX || capacity - offset > UINT32_MAX
        || !buffer->buffer)
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
    resources.fetchShaderDirty = true;
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

bool buildWUTAttributeStreams(const WUTBufferResources& resources,
    const GX2VertexShader& vertexShader, const WUTLinkedAttribute* bindings,
    size_t bindingCount, std::vector<GX2AttribStream>& output)
{
    output.clear();
    if ((bindingCount && !bindings) || (vertexShader.attribVarCount && !vertexShader.attribVars))
        return false;

    try {
        output.reserve(bindingCount);
        for (size_t index = 0; index < bindingCount; ++index) {
            const auto& binding = bindings[index];
            if (binding.attributeIndex >= resources.currentAttributeValues.size()
                || !binding.shaderInputName || !*binding.shaderInputName)
                return false;

            const GX2AttribVar* linkedInput = nullptr;
            for (uint32_t i = 0; i < vertexShader.attribVarCount; ++i) {
                const GX2AttribVar& candidate = vertexShader.attribVars[i];
                if (!candidate.name || std::strcmp(candidate.name, binding.shaderInputName))
                    continue;
                if (linkedInput)
                    return false;
                linkedInput = &candidate;
            }
            // Keep the fetch stream tied to the actual linked shader input,
            // not the GL attribute index.
            if (!linkedInput)
                return false;

            auto attributeIt = resources.attributes.find(binding.attributeIndex);
            const bool enabled = attributeIt != resources.attributes.end()
                && attributeIt->second.enabled;
            GX2AttribStream stream {};
            stream.location = linkedInput->location;
            stream.buffer = binding.attributeIndex;
            stream.offset = 0; // GX2SetAttribBuffer applies each GL byte offset.
            stream.endianSwap = GX2_ENDIAN_SWAP_DEFAULT;

            if (enabled) {
                const auto& attribute = attributeIt->second;
                if (!attribute.buffer || resources.buffers.find(attribute.buffer) == resources.buffers.end()) {
                    output.clear();
                    return false;
                }
                stream.format = attribute.format;
                stream.type = attribute.indexType;
                stream.aluDivisor = attribute.divisor;
                stream.mask = attribute.componentMask;
            } else {
                // A disabled GL attribute reads its current generic vec4. GX2
                // has no direct "constant input" fetch descriptor, so back it
                // with a zero-stride four-float stream updated before each draw.
                stream.format = GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32;
                stream.type = GX2_ATTRIB_INDEX_PER_VERTEX;
                stream.aluDivisor = 0;
                stream.mask = GX2_CHANNEL_MASK_RGBA;
            }
            output.push_back(stream);
        }
    } catch (...) {
        output.clear();
        return false;
    }

    for (size_t i = 0; i < output.size(); ++i) {
        for (size_t j = i + 1; j < output.size(); ++j) {
            if (output[i].location == output[j].location) {
                output.clear();
                return false;
            }
        }
    }
    return true;
}

bool initializeWUTFetchShader(const WUTBufferResources& resources,
    const GX2VertexShader& vertexShader, const WUTLinkedAttribute* bindings,
    size_t bindingCount, GX2FetchShader& shader, void* programBuffer,
    uint32_t bufferSize, GX2FetchShaderType type, GX2TessellationMode tessellationMode)
{
    if (!programBuffer || !vertexShader.program || !vertexShader.size
        || (reinterpret_cast<uintptr_t>(programBuffer) & (GX2_SHADER_PROGRAM_ALIGNMENT - 1)))
        return false;

    std::vector<GX2AttribStream> streams;
    if (!buildWUTAttributeStreams(resources, vertexShader, bindings, bindingCount, streams))
        return false;

    const uint32_t requiredSize = GX2CalcFetchShaderSizeEx(
        static_cast<uint32_t>(streams.size()), type, tessellationMode);
    if (!requiredSize || bufferSize < requiredSize)
        return false;

    GX2InitFetchShaderEx(&shader, static_cast<uint8_t*>(programBuffer),
        static_cast<uint32_t>(streams.size()), streams.data(), type, tessellationMode);
    // GX2 reads the generated fetch program from memory. Publish the CPU writes
    // before binding it, matching the cache maintenance used by the main draw path.
    DCFlushRange(programBuffer, requiredSize);
    // Keep vertex shader linkage and fetch program synchronized: GX2 locations
    // in the streams above came from this exact vertex shader's attribVars.
    GX2SetVertexShader(&vertexShader);
    GX2SetFetchShader(&shader);
    return true;
}

bool setElementBuffer(void* context, BufferName name)
{
    auto& resources = *static_cast<WUTBufferResources*>(context);
    if (!name) {
        resources.bindings[static_cast<uint8_t>(BufferTarget::ElementArray)] = 0;
        return true;
    }

    auto it = resources.buffers.find(name);
    if (it == resources.buffers.end() || !it->second || !it->second->buffer)
        return false;

    // Keep the active GL element-array binding visible to the direct driver
    // draw entry point as well as to the VAO object's captured state.
    resources.bindings[static_cast<uint8_t>(BufferTarget::ElementArray)] = name;
    return true;
}

bool drawVertexArrayIndexed(void* context, const IndexedDraw& draw, BufferName elementBuffer)
{
    // GL draws with no indices or no instances are valid no-ops. Handle them
    // before creating GX2R staging resources (GX2R rejects zero element counts).
    if (!draw.count || !draw.instances)
        return true;

    if (!elementBuffer || draw.indexOffset > UINT32_MAX
        || (draw.indexBytes != 1 && draw.indexBytes != 2 && draw.indexBytes != 4)
        || draw.count > UINT32_MAX / sizeof(uint32_t))
        return false;

    auto& resources = *static_cast<WUTBufferResources*>(context);
    auto it = resources.buffers.find(elementBuffer);
    if (it == resources.buffers.end() || !it->second || !it->second->buffer)
        return false;
    GX2RBuffer* source = it->second;
    size_t capacity = 0;
    if (!getBufferCapacity(*source, capacity))
        return false;
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

    // Preserve the original (base-vertex-adjusted) element indices even when
    // no enabled per-vertex attribute is linked. Replacing them with sequential
    // indices changes gl_VertexID and other shader-visible indexed-draw behavior.
    // Generic-attribute staging below uses maximumIndex to cover those indices.
    
    struct StagedAttribute {
        uint8_t location { 0 };
        GX2RBuffer buffer {};
        uint32_t stride { 0 };
        BufferName registeredName { 0 };
    };
    struct AttributeOverride {
        uint8_t index { 0 };
        bool hadOriginal { false };
        WUTBufferResources::AttributeDescriptor original {};
    };
    std::vector<StagedAttribute> stagedAttributes;
    std::vector<AttributeOverride> attributeOverrides;
    BufferName nextTemporaryBufferName = std::numeric_limits<BufferName>::max();
    auto allocateTemporaryBufferName = [&]() -> BufferName {
        while (nextTemporaryBufferName
            && resources.buffers.find(nextTemporaryBufferName) != resources.buffers.end())
            --nextTemporaryBufferName;
        if (!nextTemporaryBufferName)
            return 0;
        const BufferName name = nextTemporaryBufferName;
        --nextTemporaryBufferName;
        return name;
    };
    auto destroyStagedAttributes = [&]() {
        for (const auto& overrideState : attributeOverrides) {
            if (overrideState.hadOriginal)
                resources.attributes[overrideState.index] = overrideState.original;
            else
                resources.attributes.erase(overrideState.index);
        }
        if (!attributeOverrides.empty())
            resources.fetchShaderDirty = true;
        attributeOverrides.clear();

        for (auto& staged : stagedAttributes) {
            if (staged.registeredName) {
                auto registered = resources.buffers.find(staged.registeredName);
                if (registered != resources.buffers.end() && registered->second == &staged.buffer)
                    resources.buffers.erase(registered);
            }
            if (staged.buffer.buffer)
                GX2RDestroyBufferEx(&staged.buffer, staged.buffer.flags);
        }
        stagedAttributes.clear();
    };

    // Validate referenced vertex ranges and stage formats for which GX2 has no
    // three-component fetch encoding. Each attribute gets a packed four-value
    // stream, so no fetch can read the absent fourth source component.
    if (resources.linkedAttributes.size()
        > std::numeric_limits<size_t>::max() - resources.attributes.size())
        return false;
    try {
        stagedAttributes.reserve(resources.attributes.size() + resources.linkedAttributes.size());
        attributeOverrides.reserve(resources.linkedAttributes.size());
    } catch (...) {
        return false;
    }
    auto hasLinkedInput = [&](uint8_t index) {
        return std::any_of(resources.linkedAttributes.begin(), resources.linkedAttributes.end(),
            [&](const WUTBufferResources::AttributeBinding& binding) {
                return binding.attributeIndex == index;
            });
    };
    for (const auto& entry : resources.attributes) {
        const uint8_t location = entry.first;
        const auto& attribute = entry.second;
        if (!attribute.enabled || !hasLinkedInput(location))
            continue;
        auto vertexBuffer = resources.buffers.find(attribute.buffer);
        if (!attribute.buffer || vertexBuffer == resources.buffers.end() || !vertexBuffer->second) {
            destroyStagedAttributes();
            return false;
        }

        size_t vertexCapacity = 0;
        if (!getBufferCapacity(*vertexBuffer->second, vertexCapacity)) {
            destroyStagedAttributes();
            return false;
        }
        const size_t scalarSize = attribute.sourceType == AttributeType::Byte
            || attribute.sourceType == AttributeType::UnsignedByte ? 1
            : attribute.sourceType == AttributeType::Short
                || attribute.sourceType == AttributeType::UnsignedShort
                || attribute.sourceType == AttributeType::HalfFloat ? 2 : 4;
        const size_t elementSize = scalarSize * attribute.components;
        const size_t stride = attribute.stride ? attribute.stride : elementSize;
        if (!stride || attribute.offset > vertexCapacity || elementSize > vertexCapacity - attribute.offset) {
            destroyStagedAttributes();
            return false;
        }
        const size_t available = vertexCapacity - attribute.offset;
        const uint64_t lastElement = attribute.divisor
            ? (draw.instances ? (draw.instances - 1) / attribute.divisor : 0)
            : maximumIndex;
        if (lastElement > (available - elementSize) / stride) {
            destroyStagedAttributes();
            return false;
        }

        const bool needsExpansion = attribute.components == 3
            && (attribute.sourceType == AttributeType::Byte
                || attribute.sourceType == AttributeType::UnsignedByte
                || attribute.sourceType == AttributeType::Short
                || attribute.sourceType == AttributeType::UnsignedShort
                || attribute.sourceType == AttributeType::HalfFloat);
        if (!needsExpansion)
            continue;

        const size_t outputStride = scalarSize * 4;
        const size_t entryCount = static_cast<size_t>(lastElement) + 1;
        if (!entryCount || entryCount > UINT32_MAX / outputStride) {
            destroyStagedAttributes();
            return false;
        }

        void* sourceVertices = GX2RLockBufferEx(vertexBuffer->second,
            static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_READ));
        if (!sourceVertices) {
            destroyStagedAttributes();
            return false;
        }

        StagedAttribute staged;
        staged.location = location;
        staged.stride = static_cast<uint32_t>(outputStride);
        staged.buffer.flags = static_cast<GX2RResourceFlags>(
            GX2R_RESOURCE_BIND_VERTEX_BUFFER
            | GX2R_RESOURCE_USAGE_CPU_WRITE
            | GX2R_RESOURCE_USAGE_GPU_READ);
        staged.buffer.elemSize = static_cast<uint32_t>(outputStride);
        staged.buffer.elemCount = static_cast<uint32_t>(entryCount);
        if (!GX2RCreateBuffer(&staged.buffer)) {
            GX2RUnlockBufferEx(vertexBuffer->second,
                static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_READ));
            destroyStagedAttributes();
            return false;
        }

        void* destinationVertices = GX2RLockBufferEx(&staged.buffer,
            static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));
        if (!destinationVertices) {
            GX2RUnlockBufferEx(vertexBuffer->second,
                static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_READ));
            GX2RDestroyBufferEx(&staged.buffer, staged.buffer.flags);
            destroyStagedAttributes();
            return false;
        }

        const auto* sourceBytes = static_cast<const uint8_t*>(sourceVertices);
        auto* destinationBytes = static_cast<uint8_t*>(destinationVertices);
        uint32_t fourthComponent = 1;
        if (attribute.normalized) {
            if (attribute.sourceType == AttributeType::Byte)
                fourthComponent = 0x7f;
            else if (attribute.sourceType == AttributeType::UnsignedByte)
                fourthComponent = 0xff;
            else if (attribute.sourceType == AttributeType::Short)
                fourthComponent = 0x7fff;
            else if (attribute.sourceType == AttributeType::UnsignedShort)
                fourthComponent = 0xffff;
        }
        for (size_t vertex = 0; vertex < entryCount; ++vertex) {
            const uint8_t* input = sourceBytes + attribute.offset + vertex * stride;
            uint8_t* output = destinationBytes + vertex * outputStride;
            std::memcpy(output, input, elementSize);
            if (attribute.sourceType == AttributeType::HalfFloat) {
                const uint16_t one = 0x3c00;
                std::memcpy(output + elementSize, &one, sizeof(one));
            } else if (scalarSize == 1) {
                output[elementSize] = static_cast<uint8_t>(fourthComponent);
            } else if (scalarSize == 2) {
                const uint16_t one = static_cast<uint16_t>(fourthComponent);
                std::memcpy(output + elementSize, &one, sizeof(one));
            } else {
                std::memcpy(output + elementSize, &fourthComponent, sizeof(fourthComponent));
            }
        }
        GX2RUnlockBufferEx(&staged.buffer,
            static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));
        GX2RUnlockBufferEx(vertexBuffer->second,
            static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_READ));

        try {
            stagedAttributes.push_back(staged);
        } catch (...) {
            GX2RDestroyBufferEx(&staged.buffer, staged.buffer.flags);
            destroyStagedAttributes();
            return false;
        }
    }

    for (const auto& binding : resources.linkedAttributes) {
        const uint8_t glIndex = binding.attributeIndex;
        auto existing = resources.attributes.find(glIndex);
        if (existing != resources.attributes.end() && existing->second.enabled)
            continue;
        if (glIndex >= resources.currentAttributeValues.size()) {
            destroyStagedAttributes();
            return false;
        }

        const uint64_t entryCount64 = static_cast<uint64_t>(maximumIndex) + 1;
        constexpr uint32_t genericStride = sizeof(std::array<float, 4>);
        if (!entryCount64 || entryCount64 > UINT32_MAX / genericStride) {
            destroyStagedAttributes();
            return false;
        }

        StagedAttribute staged;
        staged.location = glIndex;
        staged.stride = genericStride;
        staged.buffer.flags = static_cast<GX2RResourceFlags>(
            GX2R_RESOURCE_BIND_VERTEX_BUFFER
            | GX2R_RESOURCE_USAGE_CPU_WRITE
            | GX2R_RESOURCE_USAGE_GPU_READ);
        staged.buffer.elemSize = genericStride;
        staged.buffer.elemCount = static_cast<uint32_t>(entryCount64);
        if (!GX2RCreateBuffer(&staged.buffer)) {
            destroyStagedAttributes();
            return false;
        }

        void* memory = GX2RLockBufferEx(&staged.buffer,
            static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));
        if (!memory) {
            GX2RDestroyBufferEx(&staged.buffer, staged.buffer.flags);
            staged.buffer = {};
            destroyStagedAttributes();
            return false;
        }
        const auto& value = resources.currentAttributeValues[glIndex];
        auto* bytes = static_cast<uint8_t*>(memory);
        for (uint32_t vertex = 0; vertex < staged.buffer.elemCount; ++vertex)
            std::memcpy(bytes + static_cast<size_t>(vertex) * genericStride, value.data(), genericStride);
        GX2RUnlockBufferEx(&staged.buffer,
            static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));

        try {
            stagedAttributes.push_back(staged);
        } catch (...) {
            GX2RDestroyBufferEx(&staged.buffer, staged.buffer.flags);
            destroyStagedAttributes();
            return false;
        }

        auto& saved = stagedAttributes.back();
        const BufferName temporaryName = allocateTemporaryBufferName();
        if (!temporaryName) {
            destroyStagedAttributes();
            return false;
        }
        try {
            resources.buffers.emplace(temporaryName, &saved.buffer);
        } catch (...) {
            destroyStagedAttributes();
            return false;
        }
        saved.registeredName = temporaryName;

        AttributeOverride overrideState;
        overrideState.index = glIndex;
        if (existing != resources.attributes.end()) {
            overrideState.hadOriginal = true;
            overrideState.original = existing->second;
        }
        try {
            attributeOverrides.push_back(overrideState);
        } catch (...) {
            destroyStagedAttributes();
            return false;
        }

        WUTBufferResources::AttributeDescriptor descriptor {};
        descriptor.format = GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32;
        descriptor.componentMask = GX2_CHANNEL_MASK_RGBA;
        descriptor.buffer = temporaryName;
        descriptor.components = 4;
        descriptor.sourceType = AttributeType::Float;
        descriptor.normalized = false;
        descriptor.stride = genericStride;
        descriptor.offset = 0;
        descriptor.divisor = 0;
        descriptor.indexType = GX2_ATTRIB_INDEX_PER_VERTEX;
        descriptor.enabled = true;
        try {
            resources.attributes[glIndex] = descriptor;
        } catch (...) {
            destroyStagedAttributes();
            return false;
        }
        resources.fetchShaderDirty = true;
    }

    GX2RBuffer stagedIndices {};
    stagedIndices.flags = static_cast<GX2RResourceFlags>(
        GX2R_RESOURCE_BIND_INDEX_BUFFER
        | GX2R_RESOURCE_USAGE_CPU_WRITE
        | GX2R_RESOURCE_USAGE_GPU_READ);
    stagedIndices.elemSize = sizeof(uint32_t);
    stagedIndices.elemCount = draw.count;
    if (!GX2RCreateBuffer(&stagedIndices)) {
        destroyStagedAttributes();
        return false;
    }
    void* stagedIndexMemory = GX2RLockBufferEx(&stagedIndices,
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));
    if (!stagedIndexMemory) {
        GX2RDestroyBufferEx(&stagedIndices, stagedIndices.flags);
        destroyStagedAttributes();
        return false;
    }
    std::memcpy(stagedIndexMemory, indices.data(), indices.size() * sizeof(uint32_t));
    GX2RUnlockBufferEx(&stagedIndices,
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));

    if (!prepareWUTProgramForDraw(resources)) {
        GX2RDestroyBufferEx(&stagedIndices, stagedIndices.flags);
        destroyStagedAttributes();
        return false;
    }

    for (const auto& staged : stagedAttributes) {
        size_t stagedCapacity = 0;
        if (!getBufferCapacity(staged.buffer, stagedCapacity)
            || stagedCapacity > UINT32_MAX || !staged.buffer.buffer) {
            GX2RDestroyBufferEx(&stagedIndices, stagedIndices.flags);
            destroyStagedAttributes();
            return false;
        }
        GX2SetAttribBuffer(staged.location, static_cast<uint32_t>(stagedCapacity),
            staged.stride, staged.buffer.buffer);
    }

    GX2DrawIndexedEx(mode, draw.count, GX2_INDEX_TYPE_U32_LE,
        stagedIndices.buffer, 0, draw.instances);

    // The index stream is always staged into a fresh GX2R buffer, even when
    // the incoming indices are already 32-bit. Never free it before GX2 has
    // consumed the submitted draw; staged vertex buffers share this lifetime.
    GX2DrawDone();

    // Restore the original attribute pointers only after completion.
    GX2RDestroyBufferEx(&stagedIndices, stagedIndices.flags);
    destroyStagedAttributes();

    for (const auto& entry : resources.attributes) {
        const auto& attribute = entry.second;
        if (attribute.enabled && attribute.buffer
            && !bindVertexBuffer(&resources, entry.first, attribute.buffer,
                attribute.stride, attribute.offset))
            return false;
    }
    return true;
}


bool drawVertexArrayArrays(void* context, Primitive primitive, uint32_t first,
    uint32_t count, uint32_t instances)
{
    if (!count || !instances)
        return true;
    if (count > UINT32_MAX / sizeof(uint32_t)
        || static_cast<uint64_t>(first) + count - 1 > UINT32_MAX)
        return false;

    auto& resources = *static_cast<WUTBufferResources*>(context);
    GX2PrimitiveMode mode;
    switch (primitive) {
    case Primitive::Points: mode = GX2_PRIMITIVE_MODE_POINTS; break;
    case Primitive::Lines: mode = GX2_PRIMITIVE_MODE_LINES; break;
    case Primitive::LineStrip: mode = GX2_PRIMITIVE_MODE_LINE_STRIP; break;
    case Primitive::Triangles: mode = GX2_PRIMITIVE_MODE_TRIANGLES; break;
    case Primitive::TriangleStrip: mode = GX2_PRIMITIVE_MODE_TRIANGLE_STRIP; break;
    case Primitive::TriangleFan: mode = GX2_PRIMITIVE_MODE_TRIANGLE_FAN; break;
    default: return false;
    }

    const uint32_t lastVertex = first + count - 1;
    bool requiresAttributeExpansion = false;
    bool requiresGenericAttributeStaging = false;
    for (const auto& binding : resources.linkedAttributes) {
        auto attribute = resources.attributes.find(binding.attributeIndex);
        if (attribute == resources.attributes.end() || !attribute->second.enabled) {
            requiresGenericAttributeStaging = true;
            break;
        }
    }
    auto hasLinkedInput = [&](uint8_t index) {
        return std::any_of(resources.linkedAttributes.begin(), resources.linkedAttributes.end(),
            [&](const WUTBufferResources::AttributeBinding& binding) {
                return binding.attributeIndex == index;
            });
    };

    // Validate every enabled vertex/instance stream before submitting work.
    // Packed U8/I8/U16/I16/half-float vec3 attributes need a staged fourth
    // component because GX2 has no native three-lane fetch encoding for them.
    for (const auto& entry : resources.attributes) {
        const auto& attribute = entry.second;
        if (!attribute.enabled || !hasLinkedInput(entry.first))
            continue;

        auto vertexBuffer = resources.buffers.find(attribute.buffer);
        if (!attribute.buffer || vertexBuffer == resources.buffers.end()
            || !vertexBuffer->second || !vertexBuffer->second->buffer)
            return false;

        size_t vertexCapacity = 0;
        if (!getBufferCapacity(*vertexBuffer->second, vertexCapacity))
            return false;
        const size_t scalarSize = attribute.sourceType == AttributeType::Byte
            || attribute.sourceType == AttributeType::UnsignedByte ? 1
            : attribute.sourceType == AttributeType::Short
                || attribute.sourceType == AttributeType::UnsignedShort
                || attribute.sourceType == AttributeType::HalfFloat ? 2 : 4;
        const size_t elementSize = scalarSize * attribute.components;
        const size_t stride = attribute.stride ? attribute.stride : elementSize;
        if (!stride || attribute.offset > vertexCapacity
            || elementSize > vertexCapacity - attribute.offset)
            return false;

        const size_t available = vertexCapacity - attribute.offset;
        const uint64_t lastElement = attribute.divisor
            ? (static_cast<uint64_t>(instances) - 1) / attribute.divisor
            : lastVertex;
        if (lastElement > (available - elementSize) / stride)
            return false;

        requiresAttributeExpansion |= attribute.components == 3
            && (attribute.sourceType == AttributeType::Byte
                || attribute.sourceType == AttributeType::UnsignedByte
                || attribute.sourceType == AttributeType::Short
                || attribute.sourceType == AttributeType::UnsignedShort
                || attribute.sourceType == AttributeType::HalfFloat);
    }

    if (!requiresAttributeExpansion && !requiresGenericAttributeStaging) {
        if (!prepareWUTProgramForDraw(resources))
            return false;

        // Rebind every enabled stream for this draw. The fetch shader records
        // stream slots, not the backing CPU/GX2 addresses, and another VAO or
        // a prior staging draw may have changed those slot bindings.
        for (const auto& entry : resources.attributes) {
            const auto& attribute = entry.second;
            if (!attribute.enabled || !hasLinkedInput(entry.first))
                continue;
            if (!bindVertexBuffer(&resources, entry.first, attribute.buffer,
                    attribute.stride, attribute.offset))
                return false;
        }

        GX2DrawEx(mode, count, first, instances);
        return true;
    }

    // Reuse the indexed staging path for precisely the cases that GX2 cannot
    // fetch safely in-place. Sequential indices preserve drawArrays semantics,
    // including first, while the shared path widens indices, expands vec3 data,
    // binds the active linked shaders/fetch program, and drains GX2 before it
    // frees temporary GPU buffers.
    GX2RBuffer sequentialIndices {};
    sequentialIndices.flags = static_cast<GX2RResourceFlags>(
        GX2R_RESOURCE_BIND_INDEX_BUFFER
        | GX2R_RESOURCE_USAGE_CPU_READ
        | GX2R_RESOURCE_USAGE_CPU_WRITE
        | GX2R_RESOURCE_USAGE_GPU_READ);
    sequentialIndices.elemSize = sizeof(uint32_t);
    sequentialIndices.elemCount = count;
    if (!GX2RCreateBuffer(&sequentialIndices))
        return false;

    void* indexMemory = GX2RLockBufferEx(&sequentialIndices,
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));
    if (!indexMemory) {
        GX2RDestroyBufferEx(&sequentialIndices, sequentialIndices.flags);
        return false;
    }

    auto* indices = static_cast<uint32_t*>(indexMemory);
    for (uint32_t i = 0; i < count; ++i)
        indices[i] = first + i;
    GX2RUnlockBufferEx(&sequentialIndices,
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));

    BufferName temporaryName = std::numeric_limits<BufferName>::max();
    while (!temporaryName || resources.buffers.find(temporaryName) != resources.buffers.end()) {
        if (temporaryName == 1) {
            GX2RDestroyBufferEx(&sequentialIndices, sequentialIndices.flags);
            return false;
        }
        --temporaryName;
    }

    try {
        resources.buffers.emplace(temporaryName, &sequentialIndices);
    } catch (...) {
        GX2RDestroyBufferEx(&sequentialIndices, sequentialIndices.flags);
        return false;
    }

    IndexedDraw draw;
    draw.primitive = primitive;
    draw.count = count;
    draw.instances = instances;
    draw.indexBytes = sizeof(uint32_t);
    draw.indexOffset = 0;
    draw.baseVertex = 0;
    const bool succeeded = drawVertexArrayIndexed(&resources, draw, temporaryName);

    resources.buffers.erase(temporaryName);
    GX2RDestroyBufferEx(&sequentialIndices, sequentialIndices.flags);
    return succeeded;
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
