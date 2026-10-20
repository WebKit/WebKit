/*
 * Native GX2R buffer storage for the OpenGL compatibility layer.
 *
 * This adapter owns GPU-visible buffer allocations and performs CPU-write
 * cache maintenance through GX2R. Vertex fetch layout remains the responsibility
 * of the fetch-shader/vertex-input adapter.
 */
#pragma once

#include "OpenGLToGX2Buffers.h"
#include "OpenGLToGX2VertexArrays.h"

#if defined(__WUT__)
#include <gx2/enum.h>
#include <gx2/shaders.h>
#include <gx2r/buffer.h>
#include <unordered_map>
#include <vector>

namespace WKC {
namespace GLGX2 {

struct WUTBufferResources {
    std::unordered_map<BufferName, GX2RBuffer*> buffers;
    std::unordered_map<uint8_t, BufferName> bindings;
    // Native fetch metadata retained independently from buffer addresses so
    // shader-link integration can build GX2AttribStream descriptors later.
    struct AttributeDescriptor {
        GX2AttribFormat format { GX2_ATTRIB_TYPE_32_FLOAT };
        uint32_t componentMask { GX2_CHANNEL_MASK_RGBA };
        BufferName buffer { 0 };
        uint8_t components { 4 };
        AttributeType sourceType { AttributeType::Float };
        bool normalized { false };
        uint32_t stride { 0 };
        size_t offset { 0 };
        uint32_t divisor { 0 };
        GX2AttribIndexType indexType { GX2_ATTRIB_INDEX_PER_VERTEX };
        bool enabled { false };
    };
    std::unordered_map<uint8_t, AttributeDescriptor> attributes;
};

BufferBackend makeWUTBufferBackend(WUTBufferResources&);
VertexArrayBackend makeWUTVertexArrayBackend(WUTBufferResources&);

// Translate a GL-facing vertex attribute format to the closest native GX2
// fetch format. Returns false for combinations GX2 cannot represent directly.
bool translateVertexAttributeFormat(const VertexAttribute&, GX2AttribFormat&);

// Materialize enabled GL attribute state into GX2 fetch-shader descriptors.
// The caller supplies linked shader locations/compatibility before initializing
// and binding the actual GX2FetchShader.
bool buildWUTAttributeStreams(const WUTBufferResources&, std::vector<GX2AttribStream>&);

// Initialize and bind a native fetch shader using caller-owned, GX2-aligned
// program memory. bufferSize must be at least GX2CalcFetchShaderSizeEx(...).
bool initializeWUTFetchShader(const WUTBufferResources&, GX2FetchShader&,
    void* programBuffer, uint32_t bufferSize,
    GX2FetchShaderType type = GX2_FETCH_SHADER_TESSELLATION_NONE,
    GX2TessellationMode tessellationMode = GX2_TESSELLATION_MODE_DISCRETE);

} // namespace GLGX2
} // namespace WKC
#endif
