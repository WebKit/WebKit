/*
 * Native Wii U GX2 backend for the OpenGL-to-GX2 state driver.
 *
 * This binds the backend-neutral Driver callbacks to libgx2 and the existing
 * WUT resource/fetch-shader path. The embedder supplies active color/depth
 * targets; ownership of those render targets remains with the caller.
 */
#pragma once

#include "OpenGLToGX2Driver.h"
#include "OpenGLToGX2WUTResources.h"

#if defined(__WUT__)
#include <gx2/surface.h>

namespace WKC {
namespace GLGX2 {

struct WUTDriverContext {
    WUTBufferResources resources;
    GX2ColorBuffer* colorBuffer { nullptr };
    GX2DepthBuffer* depthBuffer { nullptr };

    Viewport viewport;
    Scissor scissor;
    Color clearColor;
    ColorMask colorMask;

    bool blendEnabled { false };
    bool cullEnabled { false };
    bool depthTestEnabled { false };
    bool depthWriteEnabled { true };
    bool scissorEnabled { false };
    bool stencilEnabled { false };
    bool polygonOffsetEnabled { false };

    Face cullFace { Face::Back };
    Winding frontFace { Winding::CounterClockwise };
    CompareFunction depthFunction { CompareFunction::Less };
    BlendFactor sourceRGB { BlendFactor::One };
    BlendFactor destinationRGB { BlendFactor::Zero };
    BlendFactor sourceAlpha { BlendFactor::One };
    BlendFactor destinationAlpha { BlendFactor::Zero };
    BlendEquation equationRGB { BlendEquation::Add };
    BlendEquation equationAlpha { BlendEquation::Add };

    CompareFunction stencilFrontFunction { CompareFunction::Always };
    CompareFunction stencilBackFunction { CompareFunction::Always };
    int32_t stencilFrontReference { 0 };
    int32_t stencilBackReference { 0 };
    uint8_t stencilFrontValueMask { 0xff };
    uint8_t stencilBackValueMask { 0xff };
    uint8_t stencilFrontWriteMask { 0xff };
    uint8_t stencilBackWriteMask { 0xff };
    StencilOperation stencilFrontFail { StencilOperation::Keep };
    StencilOperation stencilFrontDepthFail { StencilOperation::Keep };
    StencilOperation stencilFrontDepthPass { StencilOperation::Keep };
    StencilOperation stencilBackFail { StencilOperation::Keep };
    StencilOperation stencilBackDepthFail { StencilOperation::Keep };
    StencilOperation stencilBackDepthPass { StencilOperation::Keep };

    float polygonOffsetFactor { 0.0f };
    float polygonOffsetUnits { 0.0f };
    float clearDepth { 1.0f };
    int32_t clearStencil { 0 };
    const char* lastUnsupportedOperation { nullptr };
};

Backend makeWUTDriverBackend(WUTDriverContext&);
void setWUTRenderTargets(WUTDriverContext&, GX2ColorBuffer*, GX2DepthBuffer*);

} // namespace GLGX2
} // namespace WKC
#endif
