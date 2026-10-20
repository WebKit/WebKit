/*
 * OpenGL-style vertex array and indexed draw translation interface.
 *
 * Vertex format conversion and the final GX2 draw call belong to the platform
 * backend. This layer validates the GL-facing state and computes safe byte
 * ranges without assuming GX2's vertex-fetch layout.
 */
#pragma once

#include "OpenGLToGX2Driver.h"
#include "OpenGLToGX2Buffers.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace WKC {
namespace GLGX2 {

using VertexArrayName = uint32_t;

enum class AttributeType : uint8_t {
    Byte,
    UnsignedByte,
    Short,
    UnsignedShort,
    Int,
    UnsignedInt,
    Float,
    HalfFloat,
};

struct VertexAttribute {
    bool enabled { false };
    BufferName buffer { 0 };
    uint8_t components { 4 };
    AttributeType type { AttributeType::Float };
    bool normalized { false };
    uint32_t stride { 0 };
    size_t offset { 0 };
    uint32_t divisor { 0 };
};

struct IndexedDraw {
    Primitive primitive { Primitive::Triangles };
    uint32_t count { 0 };
    uint32_t instances { 1 };
    uint8_t indexBytes { 2 };
    size_t indexOffset { 0 };
    int32_t baseVertex { 0 };
};

struct VertexArrayBackend {
    void* context { nullptr };
    bool (*bindVertexBuffer)(void*, uint8_t attribute, BufferName, uint32_t stride, size_t offset) { nullptr };
    bool (*setAttribute)(void*, uint8_t attribute, const VertexAttribute&) { nullptr };
    bool (*setElementBuffer)(void*, BufferName) { nullptr };
    bool (*drawIndexed)(void*, const IndexedDraw&, BufferName elementBuffer) { nullptr };
};

enum class VertexArrayResult : uint8_t {
    Ok,
    InvalidArgument,
    InvalidName,
    MissingBackend,
    MissingBuffer,
    BufferRangeError,
    BackendFailure,
    Unsupported,
};

class VertexArrays final {
public:
    explicit VertexArrays(const VertexArrayBackend&);

    VertexArrayResult create(VertexArrayName&);
    VertexArrayResult destroy(VertexArrayName);
    VertexArrayResult bind(VertexArrayName);
    VertexArrayName bound() const;

    VertexArrayResult setAttribute(uint8_t index, const VertexAttribute&, const BufferObjects&);
    VertexArrayResult enableAttribute(uint8_t index, bool enabled);
    VertexArrayResult setElementBuffer(BufferName, const BufferObjects&);
    VertexArrayResult drawIndexed(const IndexedDraw&, const BufferObjects&);

    bool attribute(uint8_t index, VertexAttribute&) const;
    BufferName elementBuffer() const;

private:
    struct State {
        std::unordered_map<uint8_t, VertexAttribute> attributes;
        BufferName elementBuffer { 0 };
    };

    VertexArrayName nextName();
    State* currentState();
    const State* currentState() const;

    VertexArrayBackend m_backend;
    VertexArrayName m_nextName { 1 };
    VertexArrayName m_bound { 0 };
    std::unordered_map<VertexArrayName, State> m_arrays;
};

} // namespace GLGX2
} // namespace WKC
