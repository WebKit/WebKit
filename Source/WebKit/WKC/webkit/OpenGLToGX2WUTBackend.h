/*
 * Concrete WUT/GX2 bindings for the backend-neutral OpenGL compatibility layer.
 *
 * This adapter implements only operations that have a direct, known WUT API.
 * Unimplemented callbacks remain null so the caller receives MissingBackend
 * instead of believing an unsupported GL operation was emulated.
 */
#pragma once

#include "OpenGLToGX2Driver.h"

#if defined(__WUT__)
#include <gx2/surface.h>

namespace WKC {
namespace GLGX2 {

struct WUTBackendContext {
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
};

Backend makeWUTBackend(WUTBackendContext&);

} // namespace GLGX2
} // namespace WKC
#endif
