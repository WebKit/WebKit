#include "OpenGLToGX2VertexArrays.h"

#include <limits>

namespace WKC {
namespace GLGX2 {

namespace {
size_t componentSize(AttributeType type)
{
    switch (type) {
    case AttributeType::Byte:
    case AttributeType::UnsignedByte:
        return 1;
    case AttributeType::Short:
    case AttributeType::UnsignedShort:
    case AttributeType::HalfFloat:
        return 2;
    case AttributeType::Int:
    case AttributeType::UnsignedInt:
    case AttributeType::Float:
        return 4;
    }
    return 0;
}

bool validPrimitive(Primitive primitive)
{
    switch (primitive) {
    case Primitive::Points:
    case Primitive::Lines:
    case Primitive::LineStrip:
    case Primitive::Triangles:
    case Primitive::TriangleStrip:
    case Primitive::TriangleFan:
        return true;
    }
    return false;
}
}

VertexArrays::VertexArrays(const VertexArrayBackend& backend)
    : m_backend(backend)
{
    // The default VAO (name zero) exists for GL-style contexts.
    m_arrays.emplace(0, State {});
}

VertexArrayName VertexArrays::nextName()
{
    for (uint64_t attempts = 0; attempts < std::numeric_limits<uint32_t>::max() - 1ULL; ++attempts) {
        VertexArrayName candidate = m_nextName++;
        if (!m_nextName)
            m_nextName = 1;
        if (candidate && m_arrays.find(candidate) == m_arrays.end())
            return candidate;
    }
    return 0;
}

VertexArrayResult VertexArrays::create(VertexArrayName& output)
{
    output = nextName();
    if (!output)
        return VertexArrayResult::InvalidName;
    try {
        m_arrays.emplace(output, State {});
    } catch (...) {
        output = 0;
        return VertexArrayResult::InvalidName;
    }
    return VertexArrayResult::Ok;
}

VertexArrayResult VertexArrays::destroy(VertexArrayName name)
{
    if (!name)
        return VertexArrayResult::InvalidName;
    auto it = m_arrays.find(name);
    if (it == m_arrays.end())
        return VertexArrayResult::InvalidName;
    if (m_bound == name) {
        m_bound = 0;
        if (m_backend.setElementBuffer && !m_backend.setElementBuffer(m_backend.context, 0))
            return VertexArrayResult::BackendFailure;
    }
    m_arrays.erase(it);
    return VertexArrayResult::Ok;
}

VertexArrayResult VertexArrays::bind(VertexArrayName name)
{
    if (m_arrays.find(name) == m_arrays.end())
        return VertexArrayResult::InvalidName;
    State* state = &m_arrays[name];
    if (m_backend.setElementBuffer && !m_backend.setElementBuffer(m_backend.context, state->elementBuffer))
        return VertexArrayResult::BackendFailure;
    m_bound = name;
    return VertexArrayResult::Ok;
}

VertexArrayName VertexArrays::bound() const
{
    return m_bound;
}

VertexArrays::State* VertexArrays::currentState()
{
    auto it = m_arrays.find(m_bound);
    return it == m_arrays.end() ? nullptr : &it->second;
}

const VertexArrays::State* VertexArrays::currentState() const
{
    auto it = m_arrays.find(m_bound);
    return it == m_arrays.end() ? nullptr : &it->second;
}

VertexArrayResult VertexArrays::setAttribute(uint8_t index, const VertexAttribute& input, const BufferObjects& buffers)
{
    if (input.components < 1 || input.components > 4 || !componentSize(input.type))
        return VertexArrayResult::InvalidArgument;
    if (input.buffer) {
        BufferInfo info;
        if (!buffers.info(input.buffer, info) || !info.allocated)
            return VertexArrayResult::MissingBuffer;
        if (input.offset > info.size)
            return VertexArrayResult::BufferRangeError;
    }
    State* state = currentState();
    if (!state)
        return VertexArrayResult::InvalidName;
    VertexAttribute value = input;
    auto old = state->attributes.find(index);
    if (old != state->attributes.end())
        value.enabled = old->second.enabled;
    if (m_backend.setAttribute && !m_backend.setAttribute(m_backend.context, index, value))
        return VertexArrayResult::BackendFailure;
    state->attributes[index] = value;
    if (value.buffer && m_backend.bindVertexBuffer
        && !m_backend.bindVertexBuffer(m_backend.context, index, value.buffer, value.stride, value.offset))
        return VertexArrayResult::BackendFailure;
    if (value.buffer && !m_backend.bindVertexBuffer)
        return VertexArrayResult::MissingBackend;
    return VertexArrayResult::Ok;
}

VertexArrayResult VertexArrays::enableAttribute(uint8_t index, bool enabled)
{
    State* state = currentState();
    if (!state)
        return VertexArrayResult::InvalidName;
    VertexAttribute value;
    auto it = state->attributes.find(index);
    if (it != state->attributes.end())
        value = it->second;
    value.enabled = enabled;
    if (m_backend.setAttribute && !m_backend.setAttribute(m_backend.context, index, value))
        return VertexArrayResult::BackendFailure;
    state->attributes[index] = value;
    return VertexArrayResult::Ok;
}

VertexArrayResult VertexArrays::setElementBuffer(BufferName name, const BufferObjects& buffers)
{
    if (name) {
        BufferInfo info;
        if (!buffers.info(name, info) || !info.allocated)
            return VertexArrayResult::MissingBuffer;
    }
    State* state = currentState();
    if (!state)
        return VertexArrayResult::InvalidName;
    if (m_backend.setElementBuffer && !m_backend.setElementBuffer(m_backend.context, name))
        return VertexArrayResult::BackendFailure;
    if (name && !m_backend.setElementBuffer)
        return VertexArrayResult::MissingBackend;
    state->elementBuffer = name;
    return VertexArrayResult::Ok;
}

VertexArrayResult VertexArrays::drawIndexed(const IndexedDraw& draw, const BufferObjects& buffers)
{
    if (!validPrimitive(draw.primitive) || (draw.indexBytes != 1 && draw.indexBytes != 2 && draw.indexBytes != 4))
        return VertexArrayResult::InvalidArgument;
    if (!draw.count || !draw.instances)
        return VertexArrayResult::Ok;
    const State* state = currentState();
    if (!state)
        return VertexArrayResult::InvalidName;
    if (!state->elementBuffer)
        return VertexArrayResult::MissingBuffer;

    BufferInfo elementInfo;
    if (!buffers.info(state->elementBuffer, elementInfo) || !elementInfo.allocated)
        return VertexArrayResult::MissingBuffer;
    if (draw.indexOffset > elementInfo.size
        || static_cast<size_t>(draw.count) > (elementInfo.size - draw.indexOffset) / draw.indexBytes)
        return VertexArrayResult::BufferRangeError;

    for (const auto& entry : state->attributes) {
        const VertexAttribute& attr = entry.second;
        if (!attr.enabled)
            continue;
        if (!attr.buffer)
            return VertexArrayResult::MissingBuffer;
        BufferInfo vertexInfo;
        if (!buffers.info(attr.buffer, vertexInfo) || !vertexInfo.allocated)
            return VertexArrayResult::MissingBuffer;
        size_t elementSize = componentSize(attr.type) * attr.components;
        size_t stride = attr.stride ? attr.stride : elementSize;
        if (!stride || attr.offset > vertexInfo.size)
            return VertexArrayResult::BufferRangeError;
        // This is a conservative bound for the non-instanced portion. The
        // backend must validate actual index values before GPU fetches.
        if (!attr.divisor && draw.count > 0) {
            size_t available = vertexInfo.size - attr.offset;
            if (elementSize > available || (draw.count - 1) > (available - elementSize) / stride)
                return VertexArrayResult::BufferRangeError;
        }
    }

    if (!m_backend.drawIndexed)
        return VertexArrayResult::MissingBackend;
    return m_backend.drawIndexed(m_backend.context, draw, state->elementBuffer)
        ? VertexArrayResult::Ok : VertexArrayResult::BackendFailure;
}

bool VertexArrays::attribute(uint8_t index, VertexAttribute& output) const
{
    const State* state = currentState();
    if (!state)
        return false;
    auto it = state->attributes.find(index);
    if (it == state->attributes.end())
        return false;
    output = it->second;
    return true;
}

BufferName VertexArrays::elementBuffer() const
{
    const State* state = currentState();
    return state ? state->elementBuffer : 0;
}

} // namespace GLGX2
} // namespace WKC
