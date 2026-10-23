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
        const auto insertion = m_arrays.emplace(output, State {});
        if (!insertion.second) {
            output = 0;
            return VertexArrayResult::InvalidName;
        }
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
        // Switching to the default VAO must restore all global GX2 vertex
        // descriptors as well as the element buffer. Clearing only the index
        // buffer leaves deleted-VAO attributes active in the native backend.
        VertexArrayResult result = bind(0);
        if (result != VertexArrayResult::Ok)
            return result;
    }
    m_arrays.erase(it);
    return VertexArrayResult::Ok;
}

VertexArrayResult VertexArrays::bind(VertexArrayName name)
{
    auto it = m_arrays.find(name);
    if (it == m_arrays.end())
        return VertexArrayResult::InvalidName;
    const State* destination = &it->second;
    const State* previous = currentState();

    // GX2 keeps vertex-buffer slots and attribute descriptors globally bound,
    // unlike GL's per-VAO state. Apply a complete destination state and keep
    // the logical selection unchanged until every native operation succeeds.
    auto applyState = [this](const State* state) -> VertexArrayResult {
        if (m_backend.setAttribute) {
            for (uint16_t index = 0; index < 32; ++index) {
                VertexAttribute attribute;
                if (state) {
                    auto found = state->attributes.find(static_cast<uint8_t>(index));
                    if (found != state->attributes.end())
                        attribute = found->second;
                }
                if (!m_backend.setAttribute(m_backend.context, static_cast<uint8_t>(index), attribute))
                    return VertexArrayResult::BackendFailure;
            }
        }

        if (m_backend.setElementBuffer) {
            BufferName elementBuffer = state ? state->elementBuffer : 0;
            if (!m_backend.setElementBuffer(m_backend.context, elementBuffer))
                return VertexArrayResult::BackendFailure;
        } else if (state && state->elementBuffer)
            return VertexArrayResult::MissingBackend;

        if (state) {
            for (const auto& entry : state->attributes) {
                const uint8_t index = entry.first;
                const VertexAttribute& attribute = entry.second;
                if (!attribute.buffer)
                    continue;
                if (!m_backend.bindVertexBuffer)
                    return VertexArrayResult::MissingBackend;
                if (!m_backend.bindVertexBuffer(m_backend.context, index, attribute.buffer,
                        attribute.stride, attribute.offset))
                    return VertexArrayResult::BackendFailure;
            }
        }
        return VertexArrayResult::Ok;
    };

    VertexArrayResult result = applyState(destination);
    if (result != VertexArrayResult::Ok) {
        // Best-effort native rollback. Keep m_bound unchanged even if a backend
        // callback also fails during rollback, so callers never observe a
        // logical VAO switch that was not fully committed.
        applyState(previous);
        return result;
    }

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
        if (!m_backend.bindVertexBuffer)
            return VertexArrayResult::MissingBackend;
    }
    State* state = currentState();
    if (!state)
        return VertexArrayResult::InvalidName;

    VertexAttribute value = input;
    auto old = state->attributes.find(index);
    const bool hadOld = old != state->attributes.end();
    VertexAttribute previous;
    if (hadOld) {
        previous = old->second;
        value.enabled = previous.enabled;
    }

    // Reserve the logical slot before changing native GX2 state so allocation
    // failure cannot leave a descriptor installed without matching GL state.
    if (!hadOld) {
        try {
            old = state->attributes.emplace(index, value).first;
        } catch (...) {
            return VertexArrayResult::BackendFailure;
        }
    } else
        old->second = value;

    auto restoreLogical = [&] {
        if (hadOld)
            state->attributes[index] = previous;
        else
            state->attributes.erase(index);
    };
    auto restoreBackend = [&] {
        if (m_backend.setAttribute) {
            VertexAttribute rollback;
            if (hadOld)
                rollback = previous;
            m_backend.setAttribute(m_backend.context, index, rollback);
        }
        if (m_backend.bindVertexBuffer) {
            const BufferName buffer = hadOld ? previous.buffer : 0;
            const uint32_t stride = hadOld ? previous.stride : 0;
            const size_t offset = hadOld ? previous.offset : 0;
            m_backend.bindVertexBuffer(m_backend.context, index, buffer, stride, offset);
        }
    };

    if (m_backend.setAttribute && !m_backend.setAttribute(m_backend.context, index, value)) {
        restoreBackend();
        restoreLogical();
        return VertexArrayResult::BackendFailure;
    }
    if (value.buffer && !m_backend.bindVertexBuffer(m_backend.context, index, value.buffer,
            value.stride, value.offset)) {
        restoreBackend();
        restoreLogical();
        return VertexArrayResult::BackendFailure;
    }
    return VertexArrayResult::Ok;
}

VertexArrayResult VertexArrays::enableAttribute(uint8_t index, bool enabled)
{
    State* state = currentState();
    if (!state)
        return VertexArrayResult::InvalidName;

    auto it = state->attributes.find(index);
    const bool hadOld = it != state->attributes.end();
    VertexAttribute previous;
    VertexAttribute value;
    if (hadOld) {
        previous = it->second;
        value = previous;
    }
    value.enabled = enabled;

    if (!hadOld) {
        try {
            it = state->attributes.emplace(index, value).first;
        } catch (...) {
            return VertexArrayResult::BackendFailure;
        }
    } else
        it->second = value;

    if (m_backend.setAttribute && !m_backend.setAttribute(m_backend.context, index, value)) {
        if (hadOld)
            state->attributes.find(index)->second = previous;
        else
            state->attributes.erase(index);
        // Restore the native descriptor too in case the backend callback
        // partially applied the requested enable-state change.
        if (m_backend.setAttribute) {
            VertexAttribute rollback;
            if (hadOld)
                rollback = previous;
            m_backend.setAttribute(m_backend.context, index, rollback);
        }
        return VertexArrayResult::BackendFailure;
    }
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
        size_t available = vertexInfo.size - attr.offset;
        if (elementSize > available)
            return VertexArrayResult::BufferRangeError;

        // Indexed vertex bounds depend on the actual index values in the
        // element buffer, not draw.count. Do not reject valid sparse-index
        // draws here; the native backend must scan/validate indices before
        // submitting GX2 fetches. Divisor-backed attributes can be checked
        // from the instance count alone.
        if (attr.divisor && draw.instances > 0) {
            size_t lastInstance = (draw.instances - 1) / attr.divisor;
            if (lastInstance > (available - elementSize) / stride)
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
