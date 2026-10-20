#include "OpenGLToGX2WUTBackend.h"

#if defined(__WUT__)
#include <gx2/clear.h>
#include <gx2/draw.h>
#include <gx2/registers.h>

#include <cstdint>

namespace WKC {
namespace GLGX2 {
namespace {

bool setViewport(void* context, const Viewport& viewport)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.viewport = viewport;
    GX2SetViewport(static_cast<float>(viewport.x), static_cast<float>(viewport.y),
        static_cast<float>(viewport.width), static_cast<float>(viewport.height),
        viewport.nearDepth, viewport.farDepth);
    return true;
}

bool setDepthRange(void* context, float nearDepth, float farDepth)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.viewport.nearDepth = nearDepth;
    state.viewport.farDepth = farDepth;
    const auto& viewport = state.viewport;
    GX2SetViewport(static_cast<float>(viewport.x), static_cast<float>(viewport.y),
        static_cast<float>(viewport.width), static_cast<float>(viewport.height),
        viewport.nearDepth, viewport.farDepth);
    return true;
}

bool applyDepthState(WUTBackendContext& state)
{
    GX2CompareFunction function;
    switch (state.depthFunction) {
    case CompareFunction::Never: function = GX2_COMPARE_FUNC_NEVER; break;
    case CompareFunction::Less: function = GX2_COMPARE_FUNC_LESS; break;
    case CompareFunction::Equal: function = GX2_COMPARE_FUNC_EQUAL; break;
    case CompareFunction::LessEqual: function = GX2_COMPARE_FUNC_LEQUAL; break;
    case CompareFunction::Greater: function = GX2_COMPARE_FUNC_GREATER; break;
    case CompareFunction::NotEqual: function = GX2_COMPARE_FUNC_NOT_EQUAL; break;
    case CompareFunction::GreaterEqual: function = GX2_COMPARE_FUNC_GEQUAL; break;
    case CompareFunction::Always: function = GX2_COMPARE_FUNC_ALWAYS; break;
    default: return false;
    }
    GX2SetDepthOnlyControl(state.depthTestEnabled ? TRUE : FALSE,
        state.depthWriteEnabled ? TRUE : FALSE, function);
    return true;
}

bool applyCullState(WUTBackendContext& state)
{
    const BOOL cullFront = state.cullEnabled
        && (state.cullFace == Face::Front || state.cullFace == Face::FrontAndBack);
    const BOOL cullBack = state.cullEnabled
        && (state.cullFace == Face::Back || state.cullFace == Face::FrontAndBack);
    GX2SetCullOnlyControl(state.frontFace == Winding::Clockwise
        ? GX2_FRONT_FACE_CW : GX2_FRONT_FACE_CCW, cullFront, cullBack);
    return true;
}

bool setCapability(void* context, Capability capability, bool enabled)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    switch (capability) {
    case Capability::DepthTest:
        state.depthTestEnabled = enabled;
        return applyDepthState(state);
    case Capability::CullFace:
        state.cullEnabled = enabled;
        return applyCullState(state);
    default:
        // Blend/stencil/scissor/polygon-offset state requires additional
        // independent GX2 state mapping; do not silently claim it worked.
        return false;
    }
}

bool setScissor(void*, const Scissor& scissor)
{
    if (scissor.x < 0 || scissor.y < 0)
        return false;
    GX2SetScissor(static_cast<uint32_t>(scissor.x), static_cast<uint32_t>(scissor.y),
        static_cast<uint32_t>(scissor.width), static_cast<uint32_t>(scissor.height));
    return true;
}

bool translatePrimitive(NativePrimitive primitive, GX2PrimitiveMode& output)
{
    switch (primitive) {
    case NativePrimitive::Points: output = GX2_PRIMITIVE_MODE_POINTS; return true;
    case NativePrimitive::Lines: output = GX2_PRIMITIVE_MODE_LINES; return true;
    case NativePrimitive::LineStrip: output = GX2_PRIMITIVE_MODE_LINE_STRIP; return true;
    case NativePrimitive::Triangles: output = GX2_PRIMITIVE_MODE_TRIANGLES; return true;
    case NativePrimitive::TriangleStrip: output = GX2_PRIMITIVE_MODE_TRIANGLE_STRIP; return true;
    case NativePrimitive::TriangleFan: output = GX2_PRIMITIVE_MODE_TRIANGLE_FAN; return true;
    }
    return false;
}

bool draw(void*, NativePrimitive primitive, uint32_t first, uint32_t count, uint32_t instances)
{
    GX2PrimitiveMode mode;
    if (!translatePrimitive(primitive, mode))
        return false;
    GX2DrawEx(mode, count, first, instances);
    return true;
}

bool drawIndexed(void*, NativePrimitive primitive, IndexType type, uint32_t count,
    uint64_t byteOffset, uint32_t instances)
{
    GX2PrimitiveMode mode;
    if (!translatePrimitive(primitive, mode) || byteOffset > UINT32_MAX)
        return false;

    GX2IndexType nativeType;
    switch (type) {
    case IndexType::UnsignedShort: nativeType = GX2_INDEX_TYPE_U16_LE; break;
    case IndexType::UnsignedInt: nativeType = GX2_INDEX_TYPE_U32_LE; break;
    case IndexType::UnsignedByte:
        // GX2 indexed drawing does not expose an 8-bit index type. The caller
        // must widen these indices in a staging buffer before dispatch.
        return false;
    default:
        return false;
    }

    // WUT's offset argument is a byte offset into the bound index stream.
    GX2DrawIndexedEx(mode, count, nativeType, nullptr,
        static_cast<uint32_t>(byteOffset), instances);
    return true;
}

bool setDepthFunction(void* context, CompareFunction function)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.depthFunction = function;
    return applyDepthState(state);
}

bool setDepthWrite(void* context, bool enabled)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.depthWriteEnabled = enabled;
    return applyDepthState(state);
}

bool setCullFace(void* context, Face face)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.cullFace = face;
    return applyCullState(state);
}

bool setFrontFace(void* context, Winding winding)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.frontFace = winding;
    return applyCullState(state);
}

bool setClearColor(void* context, const Color& color)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.clearColor = color;
    return true;
}

bool setClearDepth(void* context, float depth)
{
    static_cast<WUTBackendContext*>(context)->clearDepth = depth;
    return true;
}

bool setClearStencil(void* context, int32_t value)
{
    if (value < 0 || value > 255)
        return false;
    static_cast<WUTBackendContext*>(context)->clearStencil = static_cast<uint8_t>(value);
    return true;
}

bool clear(void* context, uint8_t mask)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    const bool clearColor = mask & ClearColorBuffer;
    const bool clearDepth = mask & ClearDepthBuffer;
    const bool clearStencil = mask & ClearStencilBuffer;

    if (clearColor && !state.colorBuffer)
        return false;
    if ((clearDepth || clearStencil) && !state.depthBuffer)
        return false;

    if (clearColor && (clearDepth || clearStencil)) {
        GX2ClearBuffersEx(state.colorBuffer, state.depthBuffer,
            state.clearColor.red, state.clearColor.green, state.clearColor.blue,
            state.clearColor.alpha, state.clearDepth, state.clearStencil,
            static_cast<GX2ClearFlags>((clearDepth ? GX2_CLEAR_FLAGS_DEPTH : 0)
                | (clearStencil ? GX2_CLEAR_FLAGS_STENCIL : 0)));
    } else {
        if (clearColor)
            GX2ClearColor(state.colorBuffer, state.clearColor.red, state.clearColor.green,
                state.clearColor.blue, state.clearColor.alpha);
        if (clearDepth || clearStencil)
            GX2ClearDepthStencilEx(state.depthBuffer, state.clearDepth, state.clearStencil,
                static_cast<GX2ClearFlags>((clearDepth ? GX2_CLEAR_FLAGS_DEPTH : 0)
                    | (clearStencil ? GX2_CLEAR_FLAGS_STENCIL : 0)));
    }
    return true;
}

} // namespace

Backend makeWUTBackend(WUTBackendContext& context)
{
    Backend backend;
    backend.context = &context;
    backend.setViewport = setViewport;
    backend.setScissor = setScissor;
    backend.setCapability = setCapability;
    backend.setDepthRange = setDepthRange;
    backend.draw = draw;
    backend.drawIndexed = drawIndexed;
    backend.setDepthFunction = setDepthFunction;
    backend.setDepthWrite = setDepthWrite;
    backend.setCullFace = setCullFace;
    backend.setFrontFace = setFrontFace;
    backend.setClearColor = setClearColor;
    backend.setClearDepth = setClearDepth;
    backend.setClearStencil = setClearStencil;
    backend.clear = clear;
    return backend;
}

} // namespace GLGX2
} // namespace WKC
#endif
