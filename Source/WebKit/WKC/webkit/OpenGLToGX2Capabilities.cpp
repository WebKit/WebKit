#include "OpenGLToGX2Capabilities.h"

namespace WKC {
namespace GLGX2 {
namespace {

constexpr FeatureInfo featureTable[] = {
    { Feature::ViewportScissor, Support::Unsupported, "viewport/scissor", "Requires a wired GX2 state backend." },
    { Feature::BasicRasterState, Support::Unsupported, "basic raster state", "Blend, cull, depth and polygon-offset state need backend implementations." },
    { Feature::VertexBuffers, Support::Unsupported, "vertex buffers", "Buffer object management exists; native GX2 storage and binding are not wired." },
    { Feature::VertexArrays, Support::Unsupported, "vertex arrays", "Vertex attribute state exists; native GX2 fetch setup is not wired." },
    { Feature::IndexedDrawing, Support::Unsupported, "indexed drawing", "Requires index decoding/validation and GX2 index-buffer submission." },
    { Feature::InstancedDrawing, Support::Unsupported, "instanced drawing", "Requires GX2 instancing support or a correct emulation path." },
    { Feature::VertexFragmentShaders, Support::Unsupported, "vertex/fragment shaders", "Requires a GLSL-to-GX2 shader compiler or precompiled compatible shaders." },
    { Feature::Uniforms, Support::Unsupported, "uniforms", "Uniform storage exists; native shader constant upload is not wired." },
    { Feature::Texture2D, Support::Unsupported, "2D textures", "Requires texture allocation, upload, sampler and lifetime bindings." },
    { Feature::TextureCube, Support::Unsupported, "cube textures", "Requires cube-face allocation and sampler binding." },
    { Feature::Texture3D, Support::Unsupported, "3D textures", "Requires 3D texture support or explicit emulation." },
    { Feature::TextureArrays, Support::Unsupported, "texture arrays", "Requires array texture storage and shader-compatible sampling." },
    { Feature::Samplers, Support::Unsupported, "sampler objects", "Requires independent sampler state and texture-unit binding." },
    { Feature::Framebuffers, Support::Unsupported, "framebuffer objects", "Requires attachment validation and GX2 render-target setup." },
    { Feature::Renderbuffers, Support::Unsupported, "renderbuffers", "Requires native renderbuffer allocation and attachment handling." },
    { Feature::Blending, Support::Unsupported, "blending", "Requires blend equation and factor translation." },
    { Feature::DepthTesting, Support::Unsupported, "depth testing", "Requires depth compare/write state translation." },
    { Feature::StencilTesting, Support::Unsupported, "stencil testing", "Requires stencil compare, operation and mask translation." },
    { Feature::Multisampling, Support::Unsupported, "multisampling", "Requires compatible GX2 surface/sample configuration." },
    { Feature::TransformFeedback, Support::Unsupported, "transform feedback", "No implementation registered." },
    { Feature::GeometryShaders, Support::Unsupported, "geometry shaders", "GX2 does not provide a drop-in desktop geometry-shader stage." },
    { Feature::TessellationShaders, Support::Unsupported, "tessellation shaders", "No complete tessellation-stage translation or emulation path." },
    { Feature::ComputeShaders, Support::Unsupported, "compute shaders", "No general-purpose compute pipeline is implemented." },
    { Feature::ImageLoadStore, Support::Unsupported, "image load/store", "No equivalent general image access path is registered." },
    { Feature::ShaderStorageBuffers, Support::Unsupported, "shader storage buffers", "Requires shader-visible storage emulation and synchronization." },
    { Feature::AtomicCounters, Support::Unsupported, "atomic counters", "No compatible atomic counter implementation." },
    { Feature::IndirectDrawing, Support::Unsupported, "indirect drawing", "Requires indirect command decoding or a native equivalent." },
    { Feature::MultiDrawIndirect, Support::Unsupported, "multi-draw indirect", "Requires command iteration/emulation or native support." },
    { Feature::BindlessTextures, Support::Unsupported, "bindless textures", "Not part of the available GX2 abstraction." },
    { Feature::SparseTextures, Support::Unsupported, "sparse textures", "No sparse residency model available." },
    { Feature::ClipControl, Support::Unsupported, "clip control", "Requires coordinate/depth convention adaptation." },
    { Feature::DebugOutput, Support::Unsupported, "debug output", "Requires a registered GL debug-message bridge." },
    { Feature::TimerQueries, Support::Unsupported, "timer queries", "Requires GPU timestamp support and query lifecycle." },
    { Feature::SyncObjects, Support::Unsupported, "sync objects", "Requires a correct fence/wait bridge." },
    { Feature::Robustness, Support::Unsupported, "robustness", "No GL robustness/reset-status implementation." },
    { Feature::DirectStateAccess, Support::Unsupported, "direct state access", "No complete DSA entry-point implementation." },
    { Feature::ProgramPipelines, Support::Unsupported, "program pipelines", "Requires separable program stage binding." },
    { Feature::SeparablePrograms, Support::Unsupported, "separable programs", "Requires independent shader-stage program linking." },
    { Feature::SpirV, Support::Unsupported, "SPIR-V", "No SPIR-V ingestion/translation path." },
    { Feature::TextureBarrier, Support::Unsupported, "texture barrier", "Requires feedback-loop hazard handling." }
};

static_assert(sizeof(featureTable) / sizeof(featureTable[0])
    == static_cast<size_t>(Feature::Count), "Feature table must cover every feature");
}

Support Capabilities::support(Feature feature) const
{
    const size_t index = static_cast<size_t>(feature);
    if (index >= featureCount() || !m_backend.query)
        return Support::Unsupported;

    Support result = Support::Unsupported;
    if (!m_backend.query(m_backend.context, feature, result))
        return Support::Unsupported;

    // A backend may only advertise a feature it actually implements. The
    // callback is the authority; this conservative table is documentation.
    return result;
}

const FeatureInfo& Capabilities::describe(Feature feature)
{
    const size_t index = static_cast<size_t>(feature);
    if (index < featureCount())
        return featureTable[index];

    static constexpr FeatureInfo invalid {
        Feature::Count, Support::Unsupported, "invalid", "Invalid feature identifier."
    };
    return invalid;
}

} // namespace GLGX2
} // namespace WKC
