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
    Color clearColor;
    float clearDepth { 1.0f };
    uint8_t clearStencil { 0 };
    Viewport viewport;
    bool depthTestEnabled { false };
    bool depthWriteEnabled { true };
    CompareFunction depthFunction { CompareFunction::Less };
    bool cullEnabled { false };
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

} // namespace GLGX2
} // namespace WKC
#endif
