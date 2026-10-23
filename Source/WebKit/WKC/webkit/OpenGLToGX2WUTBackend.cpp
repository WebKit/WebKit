#include "OpenGLToGX2WUTBackend.h"

#if defined(__WUT__)
#include <gx2/clear.h>
#include <gx2/draw.h>
#include <gx2/registers.h>
#include <gx2r/surface.h>
#include <coreinit/memdefaultheap.h>

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace WKC {
namespace GLGX2 {
namespace {

constexpr GX2RResourceFlags colorSurfaceFlags = static_cast<GX2RResourceFlags>(
    GX2R_RESOURCE_BIND_COLOR_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ
    | GX2R_RESOURCE_USAGE_GPU_READ | GX2R_RESOURCE_USAGE_GPU_WRITE);
constexpr GX2RResourceFlags depthSurfaceFlags = static_cast<GX2RResourceFlags>(
    GX2R_RESOURCE_BIND_DEPTH_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ
    | GX2R_RESOURCE_USAGE_GPU_READ | GX2R_RESOURCE_USAGE_GPU_WRITE);

void destroyOwnedRenderTarget(WUTBackendContext& state)
{
    if (!state.ownsRenderTarget)
        return;

    // Do not release image or HiZ storage while submitted GX2 work can still
    // reference it (resize and context teardown share this path).
    GX2DrawDone();
    if (state.hasColorSurface)
        GX2RDestroySurfaceEx(&state.ownedColorBuffer.surface, colorSurfaceFlags);
    if (state.hasDepthSurface)
        GX2RDestroySurfaceEx(&state.ownedDepthBuffer.surface, depthSurfaceFlags);
    if (state.depthHiZMemory)
        MEMFreeToDefaultHeap(state.depthHiZMemory);

    state.depthHiZMemory = nullptr;
    state.depthHiZSize = 0;
    state.hasColorSurface = false;
    state.hasDepthSurface = false;
    state.renderTargetWidth = 0;
    state.renderTargetHeight = 0;
    state.colorBuffer = nullptr;
    state.depthBuffer = nullptr;
    state.ownsRenderTarget = false;
}

bool applyScissorState(WUTBackendContext& state)
{
    // The GX2 scissor register accepts only unsigned values. Clip the GL
    // rectangle against the active drawing buffer rather than rejecting
    // negative x/y or allowing integer wraparound. With scissor testing off,
    // the effective rectangle is the complete color target.
    if (!state.colorBuffer)
        return true;

    const int64_t targetWidth = state.colorBuffer->surface.width;
    const int64_t targetHeight = state.colorBuffer->surface.height;
    int64_t left = 0;
    int64_t top = 0;
    int64_t right = targetWidth;
    int64_t bottom = targetHeight;
    if (state.scissorEnabled) {
        left = state.scissor.x;
        top = state.scissor.y;
        right = left + static_cast<int64_t>(state.scissor.width);
        bottom = top + static_cast<int64_t>(state.scissor.height);
    }

    left = std::clamp<int64_t>(left, 0, targetWidth);
    top = std::clamp<int64_t>(top, 0, targetHeight);
    right = std::clamp<int64_t>(right, left, targetWidth);
    bottom = std::clamp<int64_t>(bottom, top, targetHeight);
    GX2SetScissor(static_cast<uint32_t>(left), static_cast<uint32_t>(top),
        static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top));
    return true;
}

bool setViewport(void* context, const Viewport& viewport)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.viewport = viewport;
    if (state.colorBuffer)
        bindWUTRenderTarget(state);
    GX2SetViewport(static_cast<float>(viewport.x), static_cast<float>(viewport.y),
        static_cast<float>(viewport.width), static_cast<float>(viewport.height),
        viewport.nearDepth, viewport.farDepth);
    return applyScissorState(state);
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

bool applyPolygonOffsetState(WUTBackendContext& state)
{
    const BOOL enabled = state.polygonOffsetEnabled ? TRUE : FALSE;
    // GL's factor is the slope-scaled term and units is the constant depth
    // offset. GX2 exposes these as front/back scale and offset respectively.
    GX2SetPolygonOffset(state.polygonOffsetUnits, state.polygonOffsetFactor,
        state.polygonOffsetUnits, state.polygonOffsetFactor, 0.0f);
    GX2SetPolygonControl(state.frontFace == Winding::Clockwise
        ? GX2_FRONT_FACE_CW : GX2_FRONT_FACE_CCW,
        GX2_POLYGON_MODE_TRIANGLE, GX2_POLYGON_MODE_TRIANGLE,
        enabled, enabled, FALSE);
    return true;
}

bool setPolygonOffset(void* context, float factor, float units)
{
    if (!std::isfinite(factor) || !std::isfinite(units))
        return false;
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.polygonOffsetFactor = factor;
    state.polygonOffsetUnits = units;
    return applyPolygonOffsetState(state);
}

bool translateBlendFactor(BlendFactor factor, GX2BlendMode& output)
{
    switch (factor) {
    case BlendFactor::Zero: output = GX2_BLEND_MODE_ZERO; return true;
    case BlendFactor::One: output = GX2_BLEND_MODE_ONE; return true;
    case BlendFactor::SourceColor: output = GX2_BLEND_MODE_SRC_COLOR; return true;
    case BlendFactor::OneMinusSourceColor: output = GX2_BLEND_MODE_INV_SRC_COLOR; return true;
    case BlendFactor::DestinationColor: output = GX2_BLEND_MODE_DST_COLOR; return true;
    case BlendFactor::OneMinusDestinationColor: output = GX2_BLEND_MODE_INV_DST_COLOR; return true;
    case BlendFactor::SourceAlpha: output = GX2_BLEND_MODE_SRC_ALPHA; return true;
    case BlendFactor::OneMinusSourceAlpha: output = GX2_BLEND_MODE_INV_SRC_ALPHA; return true;
    case BlendFactor::DestinationAlpha: output = GX2_BLEND_MODE_DST_ALPHA; return true;
    case BlendFactor::OneMinusDestinationAlpha: output = GX2_BLEND_MODE_INV_DST_ALPHA; return true;
    case BlendFactor::ConstantColor: output = GX2_BLEND_MODE_BLEND_FACTOR; return true;
    case BlendFactor::OneMinusConstantColor: output = GX2_BLEND_MODE_INV_BLEND_FACTOR; return true;
    case BlendFactor::ConstantAlpha: output = GX2_BLEND_MODE_CONSTANT_ALPHA; return true;
    case BlendFactor::OneMinusConstantAlpha: output = GX2_BLEND_MODE_INV_CONSTANT_ALPHA; return true;
    case BlendFactor::SourceAlphaSaturate: output = GX2_BLEND_MODE_SRC_ALPHA_SAT; return true;
    }
    return false;
}

bool translateBlendEquation(BlendEquation equation, GX2BlendCombineMode& output)
{
    switch (equation) {
    case BlendEquation::Add: output = GX2_BLEND_COMBINE_MODE_ADD; return true;
    case BlendEquation::Subtract: output = GX2_BLEND_COMBINE_MODE_SUB; return true;
    case BlendEquation::ReverseSubtract: output = GX2_BLEND_COMBINE_MODE_REV_SUB; return true;
    case BlendEquation::Minimum: output = GX2_BLEND_COMBINE_MODE_MIN; return true;
    case BlendEquation::Maximum: output = GX2_BLEND_COMBINE_MODE_MAX; return true;
    }
    return false;
}

bool applyBlendState(WUTBackendContext& state)
{
    GX2BlendMode srcRGB, dstRGB, srcAlpha, dstAlpha;
    GX2BlendCombineMode equationRGB, equationAlpha;
    if (!translateBlendFactor(state.sourceRGB, srcRGB)
        || !translateBlendFactor(state.destinationRGB, dstRGB)
        || !translateBlendFactor(state.sourceAlpha, srcAlpha)
        || !translateBlendFactor(state.destinationAlpha, dstAlpha)
        || !translateBlendEquation(state.blendEquationRGB, equationRGB)
        || !translateBlendEquation(state.blendEquationAlpha, equationAlpha))
        return false;

    GX2SetBlendControl(GX2_RENDER_TARGET_0, srcRGB, dstRGB, equationRGB,
        TRUE, srcAlpha, dstAlpha, equationAlpha);
    GX2SetColorControl(GX2_LOGIC_OP_COPY, state.blendEnabled ? 1 : 0, FALSE, TRUE);
    return true;
}

GX2ChannelMask translateColorMask(const ColorMask& mask)
{
    uint32_t bits = 0;
    if (mask.red) bits |= GX2_CHANNEL_MASK_R;
    if (mask.green) bits |= GX2_CHANNEL_MASK_G;
    if (mask.blue) bits |= GX2_CHANNEL_MASK_B;
    if (mask.alpha) bits |= GX2_CHANNEL_MASK_A;
    return static_cast<GX2ChannelMask>(bits);
}

bool setColorMask(void* context, const ColorMask& mask)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.colorMask = mask;
    const auto nativeMask = translateColorMask(mask);
    GX2SetTargetChannelMasks(nativeMask, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA,
        GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA,
        GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA);
    return true;
}

bool setBlendFunction(void* context, BlendFactor sourceRGB, BlendFactor destinationRGB,
    BlendFactor sourceAlpha, BlendFactor destinationAlpha)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.sourceRGB = sourceRGB;
    state.destinationRGB = destinationRGB;
    state.sourceAlpha = sourceAlpha;
    state.destinationAlpha = destinationAlpha;
    return applyBlendState(state);
}

bool setBlendEquation(void* context, BlendEquation rgb, BlendEquation alpha)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.blendEquationRGB = rgb;
    state.blendEquationAlpha = alpha;
    return applyBlendState(state);
}

bool setBlendColor(void*, const Color& color)
{
    GX2SetBlendConstantColor(color.red, color.green, color.blue, color.alpha);
    return true;
}

bool translateStencilOperation(StencilOperation operation, GX2StencilFunction& output)
{
    switch (operation) {
    case StencilOperation::Keep: output = GX2_STENCIL_FUNCTION_KEEP; return true;
    case StencilOperation::Zero: output = GX2_STENCIL_FUNCTION_ZERO; return true;
    case StencilOperation::Replace: output = GX2_STENCIL_FUNCTION_REPLACE; return true;
    case StencilOperation::Increment: output = GX2_STENCIL_FUNCTION_INCR_CLAMP; return true;
    case StencilOperation::Decrement: output = GX2_STENCIL_FUNCTION_DECR_CLAMP; return true;
    case StencilOperation::Invert: output = GX2_STENCIL_FUNCTION_INV; return true;
    case StencilOperation::IncrementWrap: output = GX2_STENCIL_FUNCTION_INCR_WRAP; return true;
    case StencilOperation::DecrementWrap: output = GX2_STENCIL_FUNCTION_DECR_WRAP; return true;
    }
    return false;
}

bool translateCompareFunction(CompareFunction function, GX2CompareFunction& output)
{
    switch (function) {
    case CompareFunction::Never: output = GX2_COMPARE_FUNC_NEVER; return true;
    case CompareFunction::Less: output = GX2_COMPARE_FUNC_LESS; return true;
    case CompareFunction::Equal: output = GX2_COMPARE_FUNC_EQUAL; return true;
    case CompareFunction::LessEqual: output = GX2_COMPARE_FUNC_LEQUAL; return true;
    case CompareFunction::Greater: output = GX2_COMPARE_FUNC_GREATER; return true;
    case CompareFunction::NotEqual: output = GX2_COMPARE_FUNC_NOT_EQUAL; return true;
    case CompareFunction::GreaterEqual: output = GX2_COMPARE_FUNC_GEQUAL; return true;
    case CompareFunction::Always: output = GX2_COMPARE_FUNC_ALWAYS; return true;
    }
    return false;
}

bool applyStencilState(WUTBackendContext& state)
{
    GX2CompareFunction depthCompare, frontCompare, backCompare;
    GX2StencilFunction frontFail, frontZFail, frontZPass, backFail, backZFail, backZPass;
    if (!translateCompareFunction(state.depthFunction, depthCompare)
        || !translateCompareFunction(state.frontStencilFunction, frontCompare)
        || !translateCompareFunction(state.backStencilFunction, backCompare)
        || !translateStencilOperation(state.frontStencilFail, frontFail)
        || !translateStencilOperation(state.frontDepthFail, frontZFail)
        || !translateStencilOperation(state.frontDepthPass, frontZPass)
        || !translateStencilOperation(state.backStencilFail, backFail)
        || !translateStencilOperation(state.backDepthFail, backZFail)
        || !translateStencilOperation(state.backDepthPass, backZPass))
        return false;

    GX2SetDepthStencilControl(state.depthTestEnabled ? TRUE : FALSE,
        state.depthWriteEnabled ? TRUE : FALSE,
        depthCompare,
        state.stencilEnabled ? TRUE : FALSE, TRUE,
        frontCompare, frontZPass, frontZFail, frontFail,
        backCompare, backZPass, backZFail, backFail);
    GX2SetStencilMask(
        static_cast<uint8_t>(state.frontStencilCompareMask & 0xff),
        static_cast<uint8_t>(state.frontStencilWriteMask & 0xff),
        static_cast<uint8_t>(state.frontStencilReference & 0xff),
        static_cast<uint8_t>(state.backStencilCompareMask & 0xff),
        static_cast<uint8_t>(state.backStencilWriteMask & 0xff),
        static_cast<uint8_t>(state.backStencilReference & 0xff));
    return true;
}

bool setStencilFunction(void* context, Face face, CompareFunction function,
    int32_t reference, uint32_t mask)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    if (face == Face::Front || face == Face::FrontAndBack) {
        state.frontStencilFunction = function;
        state.frontStencilReference = reference;
        state.frontStencilCompareMask = mask;
    }
    if (face == Face::Back || face == Face::FrontAndBack) {
        state.backStencilFunction = function;
        state.backStencilReference = reference;
        state.backStencilCompareMask = mask;
    }
    return applyStencilState(state);
}

bool setStencilOperation(void* context, Face face, StencilOperation stencilFail,
    StencilOperation depthFail, StencilOperation depthPass)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    if (face == Face::Front || face == Face::FrontAndBack) {
        state.frontStencilFail = stencilFail;
        state.frontDepthFail = depthFail;
        state.frontDepthPass = depthPass;
    }
    if (face == Face::Back || face == Face::FrontAndBack) {
        state.backStencilFail = stencilFail;
        state.backDepthFail = depthFail;
        state.backDepthPass = depthPass;
    }
    return applyStencilState(state);
}

bool setStencilWriteMask(void* context, Face face, uint32_t mask)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    if (face == Face::Front || face == Face::FrontAndBack)
        state.frontStencilWriteMask = mask;
    if (face == Face::Back || face == Face::FrontAndBack)
        state.backStencilWriteMask = mask;
    return applyStencilState(state);
}

bool setCapability(void* context, Capability capability, bool enabled)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    switch (capability) {
    case Capability::Blend:
        state.blendEnabled = enabled;
        return applyBlendState(state);
    case Capability::DepthTest:
        state.depthTestEnabled = enabled;
        // Depth and stencil share GX2's depth-stencil control register. Updating
        // only the depth register can overwrite/disable the active stencil state.
        return applyStencilState(state);
    case Capability::CullFace:
        state.cullEnabled = enabled;
        return applyCullState(state);
    case Capability::PolygonOffsetFill:
        state.polygonOffsetEnabled = enabled;
        return applyPolygonOffsetState(state);
    case Capability::StencilTest:
        state.stencilEnabled = enabled;
        return applyStencilState(state);
    case Capability::ScissorTest:
        state.scissorEnabled = enabled;
        return applyScissorState(state);
    default:
        // Do not report capabilities as implemented until their state is mapped.
        return false;
    }
}

bool setScissor(void* context, const Scissor& scissor)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.scissor = scissor;
    return applyScissorState(state);
}

bool draw(void* context, NativePrimitive primitive, uint32_t first, uint32_t count, uint32_t instances)
{
    Primitive translatedPrimitive;
    switch (primitive) {
    case NativePrimitive::Points: translatedPrimitive = Primitive::Points; break;
    case NativePrimitive::Lines: translatedPrimitive = Primitive::Lines; break;
    case NativePrimitive::LineStrip: translatedPrimitive = Primitive::LineStrip; break;
    case NativePrimitive::Triangles: translatedPrimitive = Primitive::Triangles; break;
    case NativePrimitive::TriangleStrip: translatedPrimitive = Primitive::TriangleStrip; break;
    case NativePrimitive::TriangleFan: translatedPrimitive = Primitive::TriangleFan; break;
    default: return false;
    }

    auto& state = *static_cast<WUTBackendContext*>(context);
    if (!state.colorBuffer)
        return false;
    bindWUTRenderTarget(state);
    return drawVertexArrayArrays(&state.bufferResources, translatedPrimitive, first, count, instances);
}

bool drawIndexed(void* context, NativePrimitive primitive, IndexType type, uint32_t count,
    uint64_t byteOffset, uint32_t instances)
{
    // WebGL permits zero-count and zero-instance draws without issuing GPU work.
    if (!count || !instances)
        return true;
    if (byteOffset > UINT32_MAX)
        return false;

    Primitive translatedPrimitive;
    switch (primitive) {
    case NativePrimitive::Points: translatedPrimitive = Primitive::Points; break;
    case NativePrimitive::Lines: translatedPrimitive = Primitive::Lines; break;
    case NativePrimitive::LineStrip: translatedPrimitive = Primitive::LineStrip; break;
    case NativePrimitive::Triangles: translatedPrimitive = Primitive::Triangles; break;
    case NativePrimitive::TriangleStrip: translatedPrimitive = Primitive::TriangleStrip; break;
    case NativePrimitive::TriangleFan: translatedPrimitive = Primitive::TriangleFan; break;
    default: return false;
    }

    uint8_t indexBytes;
    switch (type) {
    case IndexType::UnsignedByte: indexBytes = 1; break;
    case IndexType::UnsignedShort: indexBytes = 2; break;
    case IndexType::UnsignedInt: indexBytes = 4; break;
    default: return false;
    }

    auto& state = *static_cast<WUTBackendContext*>(context);
    if (!state.colorBuffer)
        return false;
    bindWUTRenderTarget(state);
    const auto binding = state.bufferResources.bindings.find(
        static_cast<uint8_t>(BufferTarget::ElementArray));
    if (binding == state.bufferResources.bindings.end() || !binding->second)
        return false;

    // GX2 has no native U8 index type, and indexed draws must use the current
    // vertex-fetch descriptors as well as the element-array binding. Route all
    // index widths through the shared VAO draw path: it validates source ranges,
    // widens U8/U16 indices into a GX2R U32 stream, applies the linked shader's
    // actual input locations, stages GX2-incompatible vec3 formats, and waits
    // for GX2 before releasing temporary buffers.
    IndexedDraw draw;
    draw.primitive = translatedPrimitive;
    draw.count = count;
    draw.instances = instances;
    draw.indexBytes = indexBytes;
    draw.indexOffset = static_cast<size_t>(byteOffset);
    draw.baseVertex = 0;
    return drawVertexArrayIndexed(&state.bufferResources, draw, binding->second);
}

bool setDepthFunction(void* context, CompareFunction function)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.depthFunction = function;
    return applyStencilState(state);
}

bool setDepthWrite(void* context, bool enabled)
{
    auto& state = *static_cast<WUTBackendContext*>(context);
    state.depthWriteEnabled = enabled;
    return applyStencilState(state);
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
    // GX2's polygon-control register also stores the front-face convention;
    // reapply it so changing glFrontFace does not leave polygon-offset state
    // configured against the previous winding.
    return applyCullState(state) && applyPolygonOffsetState(state);
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
    if ((mask & ClearColorBuffer) && !state.colorBuffer)
        return false;
    if (state.colorBuffer)
        bindWUTRenderTarget(state);
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

WUTBackendContext::~WUTBackendContext()
{
    destroyOwnedRenderTarget(*this);
}

bool resizeWUTRenderTarget(WUTBackendContext& state, uint32_t width, uint32_t height)
{
    // GX2 surface dimensions and all downstream pitches/sizes are 32-bit.
    // Keep WebGL zero-sized drawing buffers valid without allocating a surface.
    if (width > 16384 || height > 16384)
        return false;
    if (!state.ownsRenderTarget && (state.colorBuffer || state.depthBuffer))
        return false;

    destroyOwnedRenderTarget(state);
    state.ownsRenderTarget = true;
    if (!width || !height) {
        state.renderTargetWidth = width;
        state.renderTargetHeight = height;
        return true;
    }

    auto& color = state.ownedColorBuffer;
    color = {};
    color.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    color.surface.width = width;
    color.surface.height = height;
    color.surface.depth = 1;
    color.surface.mipLevels = 1;
    color.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    color.surface.aa = GX2_AA_MODE1X;
    color.surface.use = GX2_SURFACE_USE_COLOR_BUFFER;
    color.surface.tileMode = GX2_TILE_MODE_DEFAULT;
    GX2CalcSurfaceSizeAndAlignment(&color.surface);
    if (!color.surface.imageSize || !color.surface.alignment
        || !GX2RCreateSurface(&color.surface, colorSurfaceFlags)) {
        destroyOwnedRenderTarget(state);
        return false;
    }
    state.hasColorSurface = true;
    color.viewMip = 0;
    color.viewFirstSlice = 0;
    color.viewNumSlices = 1;
    GX2InitColorBufferRegs(&color);

    auto& depth = state.ownedDepthBuffer;
    depth = {};
    depth.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    depth.surface.width = width;
    depth.surface.height = height;
    depth.surface.depth = 1;
    depth.surface.mipLevels = 1;
    depth.surface.format = GX2_SURFACE_FORMAT_FLOAT_D24_S8;
    depth.surface.aa = GX2_AA_MODE1X;
    depth.surface.use = GX2_SURFACE_USE_DEPTH_BUFFER;
    depth.surface.tileMode = GX2_TILE_MODE_DEFAULT;
    GX2CalcSurfaceSizeAndAlignment(&depth.surface);
    if (!depth.surface.imageSize || !depth.surface.alignment
        || !GX2RCreateSurface(&depth.surface, depthSurfaceFlags)) {
        destroyOwnedRenderTarget(state);
        return false;
    }
    state.hasDepthSurface = true;
    depth.viewMip = 0;
    depth.viewFirstSlice = 0;
    depth.viewNumSlices = 1;
    depth.depthClear = 1.0f;
    depth.stencilClear = 0;

    uint32_t hiZAlignment = 0;
    GX2CalcDepthBufferHiZInfo(&depth, &state.depthHiZSize, &hiZAlignment);
    if (state.depthHiZSize && hiZAlignment)
        state.depthHiZMemory = MEMAllocFromDefaultHeapEx(state.depthHiZSize, hiZAlignment);
    if (state.depthHiZMemory) {
        depth.hiZPtr = state.depthHiZMemory;
        depth.hiZSize = state.depthHiZSize;
        GX2InitDepthBufferHiZEnable(&depth, TRUE);
    } else {
        depth.hiZPtr = nullptr;
        depth.hiZSize = 0;
        state.depthHiZSize = 0;
        GX2InitDepthBufferHiZEnable(&depth, FALSE);
    }
    GX2InitDepthBufferRegs(&depth);

    state.colorBuffer = &color;
    state.depthBuffer = &depth;
    state.renderTargetWidth = width;
    state.renderTargetHeight = height;
    bindWUTRenderTarget(state);
    GX2SetViewport(0.0f, 0.0f, static_cast<float>(width),
        static_cast<float>(height), 0.0f, 1.0f);
    applyScissorState(state);
    return true;
}

void bindWUTRenderTarget(WUTBackendContext& state)
{
    if (state.colorBuffer)
        GX2SetColorBuffer(state.colorBuffer, GX2_RENDER_TARGET_0);
    if (state.depthBuffer)
        GX2SetDepthBuffer(state.depthBuffer);
}

BufferBackend makeWUTBufferBackend(WUTBackendContext& context)
{
    return makeWUTBufferBackend(context.bufferResources);
}

VertexArrayBackend makeWUTVertexArrayBackend(WUTBackendContext& context)
{
    return makeWUTVertexArrayBackend(context.bufferResources);
}

Backend makeWUTBackend(WUTBackendContext& context)
{
    Backend backend;
    backend.context = &context;
    backend.setViewport = setViewport;
    backend.setScissor = setScissor;
    backend.setCapability = setCapability;
    backend.setDepthRange = setDepthRange;
    backend.setPolygonOffset = setPolygonOffset;
    backend.setBlendFunction = setBlendFunction;
    backend.setBlendEquation = setBlendEquation;
    backend.setBlendColor = setBlendColor;
    backend.setColorMask = setColorMask;
    backend.setStencilFunction = setStencilFunction;
    backend.setStencilOperation = setStencilOperation;
    backend.setStencilWriteMask = setStencilWriteMask;
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
