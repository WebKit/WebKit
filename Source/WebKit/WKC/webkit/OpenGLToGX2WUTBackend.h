/*
 * Concrete WUT/GX2 bindings for the backend-neutral OpenGL compatibility layer.
 *
 * This adapter implements only operations that have a direct, known WUT API.
 * Unimplemented callbacks remain null so the caller receives MissingBackend
 * instead of believing an unsupported GL operation was emulated.
 */
#pragma once

#include "OpenGLToGX2Driver.h"
#include "OpenGLToGX2WUTResources.h"

#if defined(__WUT__)
#include <gx2/surface.h>

namespace WKC {
namespace GLGX2 {

struct WUTBackendContext {
    WUTBufferResources bufferResources;
    GX2ColorBuffer* colorBuffer { nullptr };
    GX2DepthBuffer* depthBuffer { nullptr };

    // Optional offscreen WebGL default framebuffer owned by this context.
    // Externally supplied render targets remain non-owning; resizeWUTRenderTarget
    // only creates/replaces the target when this context owns the current one.
    GX2ColorBuffer ownedColorBuffer {};
    GX2DepthBuffer ownedDepthBuffer {};
    void* depthHiZMemory { nullptr };
    uint32_t depthHiZSize { 0 };
    bool ownsRenderTarget { false };
    bool hasColorSurface { false };
    bool hasDepthSurface { false };
    uint32_t renderTargetWidth { 0 };
    uint32_t renderTargetHeight { 0 };

    WUTBackendContext() = default;
    ~WUTBackendContext();
    WUTBackendContext(const WUTBackendContext&) = delete;
    WUTBackendContext& operator=(const WUTBackendContext&) = delete;
    Color clearColor;
    Scissor scissor;
    bool scissorEnabled { false };
    float clearDepth { 1.0f };
    uint8_t clearStencil { 0 };
    Viewport viewport;
    bool depthTestEnabled { false };
    bool depthWriteEnabled { true };
    CompareFunction depthFunction { CompareFunction::Less };
    bool cullEnabled { false };
    bool polygonOffsetEnabled { false };
    float polygonOffsetFactor { 0.0f };
    float polygonOffsetUnits { 0.0f };
    Face cullFace { Face::Back };
    Winding frontFace { Winding::CounterClockwise };
    bool blendEnabled { false };
    BlendFactor sourceRGB { BlendFactor::One };
    BlendFactor destinationRGB { BlendFactor::Zero };
    BlendFactor sourceAlpha { BlendFactor::One };
    BlendFactor destinationAlpha { BlendFactor::Zero };
    BlendEquation blendEquationRGB { BlendEquation::Add };
    BlendEquation blendEquationAlpha { BlendEquation::Add };
    ColorMask colorMask;
    bool stencilEnabled { false };
    CompareFunction frontStencilFunction { CompareFunction::Always };
    CompareFunction backStencilFunction { CompareFunction::Always };
    int32_t frontStencilReference { 0 };
    int32_t backStencilReference { 0 };
    uint32_t frontStencilCompareMask { 0xff };
    uint32_t backStencilCompareMask { 0xff };
    uint32_t frontStencilWriteMask { 0xff };
    uint32_t backStencilWriteMask { 0xff };
    StencilOperation frontStencilFail { StencilOperation::Keep };
    StencilOperation frontDepthFail { StencilOperation::Keep };
    StencilOperation frontDepthPass { StencilOperation::Keep };
    StencilOperation backStencilFail { StencilOperation::Keep };
    StencilOperation backDepthFail { StencilOperation::Keep };
    StencilOperation backDepthPass { StencilOperation::Keep };
};

Backend makeWUTBackend(WUTBackendContext&);
BufferBackend makeWUTBufferBackend(WUTBackendContext&);
VertexArrayBackend makeWUTVertexArrayBackend(WUTBackendContext&);

// Allocate a GPU-backed RGBA8 + D24S8 offscreen default framebuffer. Resizing
// waits for queued GX2 work before freeing the old native surfaces.
bool resizeWUTRenderTarget(WUTBackendContext&, uint32_t width, uint32_t height);
void bindWUTRenderTarget(WUTBackendContext&);

} // namespace GLGX2
} // namespace WKC
#endif
