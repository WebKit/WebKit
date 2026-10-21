#include "OpenGLToGX2WUTDriver.h"

#if defined(__WUT__)
#include <gx2/clear.h>
#include <gx2/draw.h>
#include <gx2/registers.h>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace WKC {
namespace GLGX2 {
namespace {

GX2CompareFunction toGX2Compare(CompareFunction value, bool& valid)
{
    valid = true;
    switch (value) {
    case CompareFunction::Never: return GX2_COMPARE_FUNC_NEVER;
    case CompareFunction::Less: return GX2_COMPARE_FUNC_LESS;
    case CompareFunction::Equal: return GX2_COMPARE_FUNC_EQUAL;
    case CompareFunction::LessEqual: return GX2_COMPARE_FUNC_LEQUAL;
    case CompareFunction::Greater: return GX2_COMPARE_FUNC_GREATER;
    case CompareFunction::NotEqual: return GX2_COMPARE_FUNC_NOT_EQUAL;
    case CompareFunction::GreaterEqual: return GX2_COMPARE_FUNC_GEQUAL;
    case CompareFunction::Always: return GX2_COMPARE_FUNC_ALWAYS;
    }
    valid = false;
    return GX2_COMPARE_FUNC_ALWAYS;
}

GX2StencilFunction toGX2StencilOperation(StencilOperation value, bool& valid)
{
    valid = true;
    switch (value) {
    case StencilOperation::Keep: return GX2_STENCIL_FUNCTION_KEEP;
    case StencilOperation::Zero: return GX2_STENCIL_FUNCTION_ZERO;
    case StencilOperation::Replace: return GX2_STENCIL_FUNCTION_REPLACE;
    case StencilOperation::Increment: return GX2_STENCIL_FUNCTION_INCR_CLAMP;
    case StencilOperation::Decrement: return GX2_STENCIL_FUNCTION_DECR_CLAMP;
    case StencilOperation::Invert: return GX2_STENCIL_FUNCTION_INV;
    case StencilOperation::IncrementWrap: return GX2_STENCIL_FUNCTION_INCR_WRAP;
    case StencilOperation::DecrementWrap: return GX2_STENCIL_FUNCTION_DECR_WRAP;
    }
    valid = false;
    return GX2_STENCIL_FUNCTION_KEEP;
}

GX2BlendMode toGX2BlendFactor(BlendFactor value, bool& valid)
{
    valid = true;
    switch (value) {
    case BlendFactor::Zero: return GX2_BLEND_MODE_ZERO;
    case BlendFactor::One: return GX2_BLEND_MODE_ONE;
    case BlendFactor::SourceColor: return GX2_BLEND_MODE_SRC_COLOR;
    case BlendFactor::OneMinusSourceColor: return GX2_BLEND_MODE_INV_SRC_COLOR;
    case BlendFactor::DestinationColor: return GX2_BLEND_MODE_DST_COLOR;
    case BlendFactor::OneMinusDestinationColor: return GX2_BLEND_MODE_INV_DST_COLOR;
    case BlendFactor::SourceAlpha: return GX2_BLEND_MODE_SRC_ALPHA;
    case BlendFactor::OneMinusSourceAlpha: return GX2_BLEND_MODE_INV_SRC_ALPHA;
    case BlendFactor::DestinationAlpha: return GX2_BLEND_MODE_DST_ALPHA;
    case BlendFactor::OneMinusDestinationAlpha: return GX2_BLEND_MODE_INV_DST_ALPHA;
    case BlendFactor::ConstantColor: return GX2_BLEND_MODE_BLEND_FACTOR;
    case BlendFactor::OneMinusConstantColor: return GX2_BLEND_MODE_INV_BLEND_FACTOR;
    case BlendFactor::ConstantAlpha: return GX2_BLEND_MODE_CONSTANT_ALPHA;
    case BlendFactor::OneMinusConstantAlpha: return GX2_BLEND_MODE_INV_CONSTANT_ALPHA;
    case BlendFactor::SourceAlphaSaturate: return GX2_BLEND_MODE_SRC_ALPHA_SAT;
    }
    valid = false;
    return GX2_BLEND_MODE_ONE;
}

GX2BlendCombineMode toGX2BlendEquation(BlendEquation value, bool& valid)
{
    valid = true;
    switch (value) {
    case BlendEquation::Add: return GX2_BLEND_COMBINE_MODE_ADD;
    case BlendEquation::Subtract: return GX2_BLEND_COMBINE_MODE_SUB;
    case BlendEquation::ReverseSubtract: return GX2_BLEND_COMBINE_MODE_REV_SUB;
    case BlendEquation::Minimum: return GX2_BLEND_COMBINE_MODE_MIN;
    case BlendEquation::Maximum: return GX2_BLEND_COMBINE_MODE_MAX;
    }
    valid = false;
    return GX2_BLEND_COMBINE_MODE_ADD;
}

GX2FrontFace toGX2FrontFace(Winding value)
{
    return value == Winding::Clockwise ? GX2_FRONT_FACE_CW : GX2_FRONT_FACE_CCW;
}

bool applyDepthStencil(WUTDriverContext& context)
{
    bool validDepth = false;
    bool validFront = false;
    bool validBack = false;
    bool validFrontFail = false;
    bool validFrontDepthFail = false;
    bool validFrontDepthPass = false;
    bool validBackFail = false;
    bool validBackDepthFail = false;
    bool validBackDepthPass = false;

    GX2CompareFunction depthFunction = toGX2Compare(context.depthFunction, validDepth);
    GX2CompareFunction frontFunction = toGX2Compare(context.stencilFrontFunction, validFront);
    GX2CompareFunction backFunction = toGX2Compare(context.stencilBackFunction, validBack);
    GX2StencilFunction frontFail = toGX2StencilOperation(context.stencilFrontFail, validFrontFail);
    GX2StencilFunction frontDepthFail = toGX2StencilOperation(context.stencilFrontDepthFail, validFrontDepthFail);
    GX2StencilFunction frontDepthPass = toGX2StencilOperation(context.stencilFrontDepthPass, validFrontDepthPass);
    GX2StencilFunction backFail = toGX2StencilOperation(context.stencilBackFail, validBackFail);
    GX2StencilFunction backDepthFail = toGX2StencilOperation(context.stencilBackDepthFail, validBackDepthFail);
    GX2StencilFunction backDepthPass = toGX2StencilOperation(context.stencilBackDepthPass, validBackDepthPass);
    if (!validDepth || !validFront || !validBack || !validFrontFail || !validFrontDepthFail
        || !validFrontDepthPass || !validBackFail || !validBackDepthFail || !validBackDepthPass)
        return false;

    GX2SetDepthStencilControl(
        context.depthTestEnabled ? GX2_TRUE : GX2_FALSE,
        context.depthWriteEnabled ? GX2_TRUE : GX2_FALSE,
        depthFunction,
        context.stencilEnabled ? GX2_TRUE : GX2_FALSE,
        GX2_TRUE,
        frontFunction, frontDepthPass, frontDepthFail, frontFail,
        backFunction, backDepthPass, backDepthFail, backFail);

    GX2SetStencilMask(
        context.stencilFrontValueMask, context.stencilFrontWriteMask,
        static_cast<uint8_t>(context.stencilFrontReference),
        context.stencilBackValueMask, context.stencilBackWriteMask,
        static_cast<uint8_t>(context.stencilBackReference));
    return true;
}

bool applyCullAndPolygon(WUTDriverContext& context)
{
    bool cullFront = false;
    bool cullBack = false;
    if (context.cullEnabled) {
        cullFront = context.cullFace == Face::Front || context.cullFace == Face::FrontAndBack;
        cullBack = context.cullFace == Face::Back || context.cullFace == Face::FrontAndBack;
    }
    GX2FrontFace winding = toGX2FrontFace(context.frontFace);
    GX2SetCullOnlyControl(winding, cullFront ? GX2_TRUE : GX2_FALSE, cullBack ? GX2_TRUE : GX2_FALSE);
    GX2SetPolygonControl(
        winding, cullFront ? GX2_TRUE : GX2_FALSE, cullBack ? GX2_TRUE : GX2_FALSE,
        GX2_FALSE, GX2_POLYGON_MODE_TRIANGLE, GX2_POLYGON_MODE_TRIANGLE,
        context.polygonOffsetEnabled ? GX2_TRUE : GX2_FALSE,
        context.polygonOffsetEnabled ? GX2_TRUE : GX2_FALSE, GX2_FALSE);
    GX2SetPolygonOffset(
        context.polygonOffsetUnits, context.polygonOffsetFactor,
        context.polygonOffsetUnits, context.polygonOffsetFactor, 0.0f);
    return true;
}

bool applyBlend(WUTDriverContext& context)
{
    bool validSourceRGB = false;
    bool validDestinationRGB = false;
    bool validSourceAlpha = false;
    bool validDestinationAlpha = false;
    bool validEquationRGB = false;
    bool validEquationAlpha = false;
    GX2BlendMode sourceRGB = toGX2BlendFactor(context.sourceRGB, validSourceRGB);
    GX2BlendMode destinationRGB = toGX2BlendFactor(context.destinationRGB, validDestinationRGB);
    GX2BlendMode sourceAlpha = toGX2BlendFactor(context.sourceAlpha, validSourceAlpha);
    GX2BlendMode destinationAlpha = toGX2BlendFactor(context.destinationAlpha, validDestinationAlpha);
    GX2BlendCombineMode equationRGB = toGX2BlendEquation(context.equationRGB, validEquationRGB);
    GX2BlendCombineMode equationAlpha = toGX2BlendEquation(context.equationAlpha, validEquationAlpha);
    if (!validSourceRGB || !validDestinationRGB || !validSourceAlpha || !validDestinationAlpha
        || !validEquationRGB || !validEquationAlpha)
        return false;

    GX2SetBlendControl(GX2_RENDER_TARGET_0, sourceRGB, destinationRGB, equationRGB,
        GX2_TRUE, sourceAlpha, destinationAlpha, equationAlpha);
    GX2SetColorControl(GX2_LOGIC_OP_COPY,
        context.blendEnabled ? 1 : 0, GX2_FALSE, GX2_TRUE);
    return true;
}

GX2ChannelMask toGX2ChannelMask(const ColorMask& value)
{
    uint32_t mask = (value.red ? GX2_CHANNEL_MASK_R : 0)
        | (value.green ? GX2_CHANNEL_MASK_G : 0)
        | (value.blue ? GX2_CHANNEL_MASK_B : 0)
        | (value.alpha ? GX2_CHANNEL_MASK_A : 0);
    return static_cast<GX2ChannelMask>(mask);
}

bool applyScissor(WUTDriverContext& context)
{
    if (!context.colorBuffer)
        return true; // Render target may be attached after GL state is established.

    const int64_t targetWidth = context.colorBuffer->surface.width;
    const int64_t targetHeight = context.colorBuffer->surface.height;
    int64_t left = 0;
    int64_t top = 0;
    int64_t right = targetWidth;
    int64_t bottom = targetHeight;

    if (context.scissorEnabled) {
        left = context.scissor.x;
        top = context.scissor.y;
        right = left + static_cast<int64_t>(context.scissor.width);
        bottom = top + static_cast<int64_t>(context.scissor.height);
    }

    left = std::clamp<int64_t>(left, 0, targetWidth);
    top = std::clamp<int64_t>(top, 0, targetHeight);
    right = std::clamp<int64_t>(right, left, targetWidth);
    bottom = std::clamp<int64_t>(bottom, top, targetHeight);

    GX2SetScissor(static_cast<uint32_t>(left), static_cast<uint32_t>(top),
        static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top));
    return true;
}

bool setViewport(void* opaque, const Viewport& viewport)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    context.viewport = viewport;
    GX2SetViewport(static_cast<float>(viewport.x), static_cast<float>(viewport.y),
        static_cast<float>(viewport.width), static_cast<float>(viewport.height),
        viewport.nearDepth, viewport.farDepth);
    return applyScissor(context);
}

bool setScissor(void* opaque, const Scissor& scissor)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    context.scissor = scissor;
    return applyScissor(context);
}

bool setCapability(void* opaque, Capability capability, bool enabled)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    switch (capability) {
    case Capability::Blend:
        context.blendEnabled = enabled;
        return applyBlend(context);
    case Capability::CullFace:
        context.cullEnabled = enabled;
        return applyCullAndPolygon(context);
    case Capability::DepthTest:
        context.depthTestEnabled = enabled;
        return applyDepthStencil(context);
    case Capability::ScissorTest:
        context.scissorEnabled = enabled;
        return applyScissor(context);
    case Capability::StencilTest:
        context.stencilEnabled = enabled;
        return applyDepthStencil(context);
    case Capability::PolygonOffsetFill:
        context.polygonOffsetEnabled = enabled;
        return applyCullAndPolygon(context);
    }
    return false;
}

bool setBlendFunction(void* opaque, BlendFactor sourceRGB, BlendFactor destinationRGB,
    BlendFactor sourceAlpha, BlendFactor destinationAlpha)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    context.sourceRGB = sourceRGB;
    context.destinationRGB = destinationRGB;
    context.sourceAlpha = sourceAlpha;
    context.destinationAlpha = destinationAlpha;
    return applyBlend(context);
}

bool setBlendEquation(void* opaque, BlendEquation rgb, BlendEquation alpha)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    context.equationRGB = rgb;
    context.equationAlpha = alpha;
    return applyBlend(context);
}

bool setDepthFunction(void* opaque, CompareFunction function)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    context.depthFunction = function;
    return applyDepthStencil(context);
}

bool setDepthWrite(void* opaque, bool enabled)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    context.depthWriteEnabled = enabled;
    return applyDepthStencil(context);
}

bool setDepthRange(void* opaque, float nearDepth, float farDepth)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    context.viewport.nearDepth = nearDepth;
    context.viewport.farDepth = farDepth;
    GX2SetViewport(static_cast<float>(context.viewport.x), static_cast<float>(context.viewport.y),
        static_cast<float>(context.viewport.width), static_cast<float>(context.viewport.height),
        nearDepth, farDepth);
    return true;
}

bool setCullFace(void* opaque, Face face)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    context.cullFace = face;
    return applyCullAndPolygon(context);
}

bool setFrontFace(void* opaque, Winding winding)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    context.frontFace = winding;
    return applyCullAndPolygon(context);
}

bool setPolygonOffset(void* opaque, float factor, float units)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    context.polygonOffsetFactor = factor;
    context.polygonOffsetUnits = units;
    return applyCullAndPolygon(context);
}

bool setStencilFunction(void* opaque, Face face, CompareFunction function, int32_t reference, uint32_t mask)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    if (face == Face::Front || face == Face::FrontAndBack) {
        context.stencilFrontFunction = function;
        context.stencilFrontReference = reference;
        context.stencilFrontValueMask = static_cast<uint8_t>(mask);
    }
    if (face == Face::Back || face == Face::FrontAndBack) {
        context.stencilBackFunction = function;
        context.stencilBackReference = reference;
        context.stencilBackValueMask = static_cast<uint8_t>(mask);
    }
    return applyDepthStencil(context);
}

bool setStencilOperation(void* opaque, Face face, StencilOperation fail,
    StencilOperation depthFail, StencilOperation depthPass)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    if (face == Face::Front || face == Face::FrontAndBack) {
        context.stencilFrontFail = fail;
        context.stencilFrontDepthFail = depthFail;
        context.stencilFrontDepthPass = depthPass;
    }
    if (face == Face::Back || face == Face::FrontAndBack) {
        context.stencilBackFail = fail;
        context.stencilBackDepthFail = depthFail;
        context.stencilBackDepthPass = depthPass;
    }
    return applyDepthStencil(context);
}

bool setStencilWriteMask(void* opaque, Face face, uint32_t mask)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    if (face == Face::Front || face == Face::FrontAndBack)
        context.stencilFrontWriteMask = static_cast<uint8_t>(mask);
    if (face == Face::Back || face == Face::FrontAndBack)
        context.stencilBackWriteMask = static_cast<uint8_t>(mask);
    return applyDepthStencil(context);
}

bool setBlendColor(void* opaque, const Color& color)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    GX2SetBlendConstantColor(color.red, color.green, color.blue, color.alpha);
    return true;
}

bool setColorMask(void* opaque, const ColorMask& mask)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    context.colorMask = mask;
    GX2ChannelMask nativeMask = toGX2ChannelMask(mask);
    GX2SetTargetChannelMasks(nativeMask, nativeMask, nativeMask, nativeMask,
        nativeMask, nativeMask, nativeMask, nativeMask);
    GX2SetColorControl(GX2_LOGIC_OP_COPY, context.blendEnabled ? 1 : 0,
        GX2_FALSE, (mask.red || mask.green || mask.blue || mask.alpha) ? GX2_TRUE : GX2_FALSE);
    return true;
}

bool setClearColor(void* opaque, const Color& color)
{
    static_cast<WUTDriverContext*>(opaque)->clearColor = color;
    return true;
}

bool setClearDepth(void* opaque, float depth)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    context.clearDepth = depth;
    if (context.depthBuffer)
        GX2SetClearDepth(context.depthBuffer, depth);
    return true;
}

bool setClearStencil(void* opaque, int32_t value)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    context.clearStencil = value;
    if (context.depthBuffer)
        GX2SetClearStencil(context.depthBuffer, static_cast<uint8_t>(value));
    return true;
}

bool clear(void* opaque, uint8_t mask)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    if (mask & ClearColorBuffer) {
        if (!context.colorBuffer)
            return false;
        GX2ClearColor(context.colorBuffer, context.clearColor.red, context.clearColor.green,
            context.clearColor.blue, context.clearColor.alpha);
    }

    GX2ClearFlags flags = static_cast<GX2ClearFlags>(0);
    if (mask & ClearDepthBuffer)
        flags = static_cast<GX2ClearFlags>(static_cast<uint32_t>(flags) | GX2_CLEAR_FLAGS_DEPTH);
    if (mask & ClearStencilBuffer)
        flags = static_cast<GX2ClearFlags>(static_cast<uint32_t>(flags) | GX2_CLEAR_FLAGS_STENCIL);
    if (flags) {
        if (!context.depthBuffer)
            return false;
        GX2ClearDepthStencilEx(context.depthBuffer, context.clearDepth,
            static_cast<uint8_t>(context.clearStencil), flags);
    }
    return true;
}

bool drawArrays(void* opaque, NativePrimitive primitive, uint32_t first, uint32_t count, uint32_t instances)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    Primitive glPrimitive;
    switch (primitive) {
    case NativePrimitive::Points: glPrimitive = Primitive::Points; break;
    case NativePrimitive::Lines: glPrimitive = Primitive::Lines; break;
    case NativePrimitive::LineStrip: glPrimitive = Primitive::LineStrip; break;
    case NativePrimitive::Triangles: glPrimitive = Primitive::Triangles; break;
    case NativePrimitive::TriangleStrip: glPrimitive = Primitive::TriangleStrip; break;
    case NativePrimitive::TriangleFan: glPrimitive = Primitive::TriangleFan; break;
    default: return false;
    }
    return drawVertexArrayArrays(&context.resources, glPrimitive, first, count, instances);
}

bool drawIndexed(void* opaque, NativePrimitive primitive, IndexType indexType, uint32_t count,
    uint64_t byteOffset, uint32_t instances)
{
    auto& context = *static_cast<WUTDriverContext*>(opaque);
    if (byteOffset > std::numeric_limits<size_t>::max())
        return false;

    auto elementBinding = context.resources.bindings.find(static_cast<uint8_t>(BufferTarget::ElementArray));
    if (elementBinding == context.resources.bindings.end() || !elementBinding->second)
        return false;

    Primitive glPrimitive;
    switch (primitive) {
    case NativePrimitive::Points: glPrimitive = Primitive::Points; break;
    case NativePrimitive::Lines: glPrimitive = Primitive::Lines; break;
    case NativePrimitive::LineStrip: glPrimitive = Primitive::LineStrip; break;
    case NativePrimitive::Triangles: glPrimitive = Primitive::Triangles; break;
    case NativePrimitive::TriangleStrip: glPrimitive = Primitive::TriangleStrip; break;
    case NativePrimitive::TriangleFan: glPrimitive = Primitive::TriangleFan; break;
    default: return false;
    }

    uint8_t indexBytes = 0;
    switch (indexType) {
    case IndexType::UnsignedByte: indexBytes = 1; break;
    case IndexType::UnsignedShort: indexBytes = 2; break;
    case IndexType::UnsignedInt: indexBytes = 4; break;
    default: return false;
    }

    IndexedDraw draw;
    draw.primitive = glPrimitive;
    draw.count = count;
    draw.instances = instances;
    draw.indexBytes = indexBytes;
    draw.indexOffset = static_cast<size_t>(byteOffset);
    draw.baseVertex = 0;
    return drawVertexArrayIndexed(&context.resources, draw, elementBinding->second);
}

void reportUnsupported(void* opaque, const char* operation)
{
    static_cast<WUTDriverContext*>(opaque)->lastUnsupportedOperation = operation;
}

} // namespace

Backend makeWUTDriverBackend(WUTDriverContext& context)
{
    Backend backend;
    backend.context = &context;
    backend.setViewport = setViewport;
    backend.setScissor = setScissor;
    backend.setCapability = setCapability;
    backend.setBlendFunction = setBlendFunction;
    backend.setBlendEquation = setBlendEquation;
    backend.setDepthFunction = setDepthFunction;
    backend.setDepthWrite = setDepthWrite;
    backend.setDepthRange = setDepthRange;
    backend.setCullFace = setCullFace;
    backend.setFrontFace = setFrontFace;
    backend.setPolygonOffset = setPolygonOffset;
    backend.setStencilFunction = setStencilFunction;
    backend.setStencilOperation = setStencilOperation;
    backend.setStencilWriteMask = setStencilWriteMask;
    backend.setBlendColor = setBlendColor;
    backend.setColorMask = setColorMask;
    backend.setClearColor = setClearColor;
    backend.setClearDepth = setClearDepth;
    backend.setClearStencil = setClearStencil;
    backend.clear = clear;
    backend.draw = drawArrays;
    backend.drawIndexed = drawIndexed;
    backend.reportUnsupported = reportUnsupported;
    return backend;
}

void setWUTRenderTargets(WUTDriverContext& context, GX2ColorBuffer* color, GX2DepthBuffer* depth)
{
    context.colorBuffer = color;
    context.depthBuffer = depth;
    if (color)
        GX2SetColorBuffer(color, GX2_RENDER_TARGET_0);
    if (depth)
        GX2SetDepthBuffer(depth);
    applyScissor(context);
    applyDepthStencil(context);
    applyBlend(context);
    applyCullAndPolygon(context);
}

} // namespace GLGX2
} // namespace WKC
#endif
