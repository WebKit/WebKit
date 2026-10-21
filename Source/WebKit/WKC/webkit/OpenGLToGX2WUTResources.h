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
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace WKC {
namespace GLGX2 {

// Explicit bridge from a GL attribute index to the name of the input in
// the already-linked GX2 vertex shader. GX2AttribVar::location is authoritative;
// GL indices must never be assumed to equal GX2 shader locations.
struct WUTLinkedAttribute {
    uint8_t attributeIndex { 0 };
    const char* shaderInputName { nullptr };
};

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
    struct AttributeBinding {
        uint8_t attributeIndex { 0 };
        std::string shaderInputName;
    };
    std::unordered_map<uint8_t, AttributeDescriptor> attributes;

    // WebGL's generic (array-disabled) vertex attribute values are context
    // state, not part of a VAO. Start with the GL-defined (0, 0, 0, 1) value.
    std::array<std::array<float, 4>, 32> currentAttributeValues = [] {
        std::array<std::array<float, 4>, 32> values { };
        for (auto& value : values)
            value = { 0.0f, 0.0f, 0.0f, 1.0f };
        return values;
    }();

    // These are the active linked-program objects. The owner keeps the shader
    // objects alive; this structure owns only the fetch-shader program memory.
    const GX2VertexShader* vertexShader { nullptr };
    const GX2PixelShader* pixelShader { nullptr };
    GX2ShaderMode shaderMode { GX2_SHADER_MODE_UNIFORM_REGISTER };
    std::vector<AttributeBinding> linkedAttributes;
    GX2FetchShader fetchShader {};
    void* fetchShaderProgramBuffer { nullptr };
    uint32_t fetchShaderProgramBufferSize { 0 };
    bool fetchShaderDirty { true };

    WUTBufferResources() = default;
    ~WUTBufferResources();
    WUTBufferResources(const WUTBufferResources&) = delete;
    WUTBufferResources& operator=(const WUTBufferResources&) = delete;
};

BufferBackend makeWUTBufferBackend(WUTBufferResources&);
VertexArrayBackend makeWUTVertexArrayBackend(WUTBufferResources&);

// Translate a GL-facing vertex attribute format to the closest native GX2
// fetch format. Returns false for combinations GX2 cannot represent directly.
bool translateVertexAttributeFormat(const VertexAttribute&, GX2AttribFormat&);

// Install the native result of a successfully linked WebGL program. Names
// are copied because fetch-shader creation can happen on later draw calls.
bool setWUTLinkedProgram(WUTBufferResources&, const GX2VertexShader&, const GX2PixelShader&,
    GX2ShaderMode, const WUTLinkedAttribute* bindings, size_t bindingCount);

// Bind the active vertex/pixel shaders and lazily (re)build the fetch program
// from the current VAO descriptors. Called immediately before every GX2 draw.
bool prepareWUTProgramForDraw(WUTBufferResources&);

// Execute the native indexed draw path used by the VAO backend. This performs
// index widening, attribute-range validation/staging and restores GX2 fetch
// state after the draw completes.
bool drawVertexArrayIndexed(void* context, const IndexedDraw&, BufferName elementBuffer);
bool drawVertexArrayArrays(void* context, Primitive, uint32_t first, uint32_t count, uint32_t instances);

// Materialize enabled GL attribute state into GX2 fetch-shader descriptors.
// The caller supplies linked shader locations/compatibility before initializing
// and binding the actual GX2FetchShader.
bool buildWUTAttributeStreams(const WUTBufferResources&, std::vector<GX2AttribStream>&);

bool buildWUTAttributeStreams(const WUTBufferResources&, const GX2VertexShader&,
    const WUTLinkedAttribute* bindings, size_t bindingCount, std::vector<GX2AttribStream>& output);

// Build the fetch program against the same linked vertex shader, then bind the
// vertex shader and fetch shader as a coordinated pair. Shader and program
// storage remain caller-owned and must outlive their use by GX2.
bool initializeWUTFetchShader(const WUTBufferResources&, const GX2VertexShader&,
    const WUTLinkedAttribute* bindings, size_t bindingCount, GX2FetchShader&,
    void* programBuffer, uint32_t bufferSize,
    GX2FetchShaderType type = GX2_FETCH_SHADER_TESSELLATION_NONE,
    GX2TessellationMode tessellationMode = GX2_TESSELLATION_MODE_DISCRETE);

} // namespace GLGX2
} // namespace WKC
#endif
