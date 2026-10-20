/*
 * OpenGL 4.6 feature classification for the Wii U GX2 compatibility layer.
 *
 * This is deliberately a capability contract, not a claim that desktop GL
 * 4.6 exists on the console. Each operation is classified by the backend at
 * runtime so callers can fail clearly rather than silently render incorrectly.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace WKC {
namespace GLGX2 {

enum class Feature : uint8_t {
    ViewportScissor,
    BasicRasterState,
    VertexBuffers,
    VertexArrays,
    IndexedDrawing,
    InstancedDrawing,
    VertexFragmentShaders,
    Uniforms,
    Texture2D,
    TextureCube,
    Texture3D,
    TextureArrays,
    Samplers,
    Framebuffers,
    Renderbuffers,
    Blending,
    DepthTesting,
    StencilTesting,
    Multisampling,
    TransformFeedback,
    GeometryShaders,
    TessellationShaders,
    ComputeShaders,
    ImageLoadStore,
    ShaderStorageBuffers,
    AtomicCounters,
    IndirectDrawing,
    MultiDrawIndirect,
    BindlessTextures,
    SparseTextures,
    ClipControl,
    DebugOutput,
    TimerQueries,
    SyncObjects,
    Robustness,
    DirectStateAccess,
    ProgramPipelines,
    SeparablePrograms,
    SpirV,
    TextureBarrier,
    Count
};

enum class Support : uint8_t {
    Unsupported,
    Emulated,
    Native
};

struct FeatureInfo {
    Feature feature;
    Support support;
    const char* name;
    const char* explanation;
};

struct CapabilityBackend {
    void* context { nullptr };
    // Return false when the backend cannot classify a feature. The caller
    // must then treat it as unsupported, never as an optimistic default.
    bool (*query)(void*, Feature, Support&) { nullptr };
};

class Capabilities final {
public:
    explicit Capabilities(const CapabilityBackend& backend)
        : m_backend(backend)
    {
    }

    Support support(Feature feature) const;
    bool isAvailable(Feature feature) const
    {
        return support(feature) != Support::Unsupported;
    }

    static const FeatureInfo& describe(Feature);
    static size_t featureCount()
    {
        return static_cast<size_t>(Feature::Count);
    }

private:
    CapabilityBackend m_backend;
};

} // namespace GLGX2
} // namespace WKC
