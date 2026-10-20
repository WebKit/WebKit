#include "OpenGLToGX2Driver.h"

namespace WKC {
namespace GLGX2 {

Driver::Driver(const Backend& backend)
    : m_backend(backend)
{
}

Result Driver::setViewport(const Viewport& viewport)
{
    if (viewport.width < 0 || viewport.height < 0
        || viewport.nearDepth < 0.0f || viewport.nearDepth > 1.0f
        || viewport.farDepth < 0.0f || viewport.farDepth > 1.0f)
        return Result::InvalidArgument;

    if (!m_backend.setViewport)
        return Result::MissingBackend;
    if (!m_backend.setViewport(m_backend.context, viewport))
        return Result::Unsupported;

    m_viewport = viewport;
    return Result::Ok;
}

Result Driver::setScissor(const Scissor& scissor)
{
    if (scissor.width < 0 || scissor.height < 0)
        return Result::InvalidArgument;

    if (!m_backend.setScissor)
        return Result::MissingBackend;
    if (!m_backend.setScissor(m_backend.context, scissor))
        return Result::Unsupported;

    m_scissor = scissor;
    return Result::Ok;
}

Result Driver::enable(Capability capability)
{
    switch (capability) {
    case Capability::Blend:
    case Capability::CullFace:
    case Capability::DepthTest:
    case Capability::ScissorTest:
    case Capability::StencilTest:
    case Capability::PolygonOffsetFill:
        return setCapability(capability, true);
    }
    return Result::InvalidArgument;
}

Result Driver::disable(Capability capability)
{
    switch (capability) {
    case Capability::Blend:
    case Capability::CullFace:
    case Capability::DepthTest:
    case Capability::ScissorTest:
    case Capability::StencilTest:
    case Capability::PolygonOffsetFill:
        return setCapability(capability, false);
    }
    return Result::InvalidArgument;
}

Result Driver::setCapability(Capability capability, bool enabled)
{
    if (!m_backend.setCapability)
        return Result::MissingBackend;
    return m_backend.setCapability(m_backend.context, capability, enabled)
        ? Result::Ok : Result::Unsupported;
}

bool Driver::validBlendFactor(BlendFactor factor)
{
    switch (factor) {
    case BlendFactor::Zero:
    case BlendFactor::One:
    case BlendFactor::SourceColor:
    case BlendFactor::OneMinusSourceColor:
    case BlendFactor::DestinationColor:
    case BlendFactor::OneMinusDestinationColor:
    case BlendFactor::SourceAlpha:
    case BlendFactor::OneMinusSourceAlpha:
    case BlendFactor::DestinationAlpha:
    case BlendFactor::OneMinusDestinationAlpha:
    case BlendFactor::ConstantColor:
    case BlendFactor::OneMinusConstantColor:
    case BlendFactor::ConstantAlpha:
    case BlendFactor::OneMinusConstantAlpha:
    case BlendFactor::SourceAlphaSaturate:
        return true;
    }
    return false;
}

bool Driver::validBlendEquation(BlendEquation equation)
{
    switch (equation) {
    case BlendEquation::Add:
    case BlendEquation::Subtract:
    case BlendEquation::ReverseSubtract:
    case BlendEquation::Minimum:
    case BlendEquation::Maximum:
        return true;
    }
    return false;
}

bool Driver::validCompareFunction(CompareFunction function)
{
    switch (function) {
    case CompareFunction::Never:
    case CompareFunction::Less:
    case CompareFunction::Equal:
    case CompareFunction::LessEqual:
    case CompareFunction::Greater:
    case CompareFunction::NotEqual:
    case CompareFunction::GreaterEqual:
    case CompareFunction::Always:
        return true;
    }
    return false;
}

Result Driver::setBlendFunction(BlendFactor sourceRGB, BlendFactor destinationRGB,
    BlendFactor sourceAlpha, BlendFactor destinationAlpha)
{
    if (!validBlendFactor(sourceRGB) || !validBlendFactor(destinationRGB)
        || !validBlendFactor(sourceAlpha) || !validBlendFactor(destinationAlpha))
        return Result::InvalidArgument;
    if (!m_backend.setBlendFunction)
        return Result::MissingBackend;
    return m_backend.setBlendFunction(m_backend.context, sourceRGB, destinationRGB,
        sourceAlpha, destinationAlpha) ? Result::Ok : Result::Unsupported;
}

Result Driver::setBlendEquation(BlendEquation rgb, BlendEquation alpha)
{
    if (!validBlendEquation(rgb) || !validBlendEquation(alpha))
        return Result::InvalidArgument;
    if (!m_backend.setBlendEquation)
        return Result::MissingBackend;
    return m_backend.setBlendEquation(m_backend.context, rgb, alpha)
        ? Result::Ok : Result::Unsupported;
}

Result Driver::setDepthFunction(CompareFunction function)
{
    if (!validCompareFunction(function))
        return Result::InvalidArgument;
    if (!m_backend.setDepthFunction)
        return Result::MissingBackend;
    return m_backend.setDepthFunction(m_backend.context, function)
        ? Result::Ok : Result::Unsupported;
}

Result Driver::setColorMask(const ColorMask& mask)
{
    if (!m_backend.setColorMask)
        return Result::MissingBackend;
    return m_backend.setColorMask(m_backend.context, mask)
        ? Result::Ok : Result::Unsupported;
}

Result Driver::setClearColor(const Color& color)
{
    if (!(color.red >= 0.0f && color.red <= 1.0f)
        || !(color.green >= 0.0f && color.green <= 1.0f)
        || !(color.blue >= 0.0f && color.blue <= 1.0f)
        || !(color.alpha >= 0.0f && color.alpha <= 1.0f))
        return Result::InvalidArgument;
    if (!m_backend.setClearColor)
        return Result::MissingBackend;
    return m_backend.setClearColor(m_backend.context, color)
        ? Result::Ok : Result::Unsupported;
}

Result Driver::clear(uint8_t bufferMask)
{
    constexpr uint8_t validMask = ClearColorBuffer | ClearDepthBuffer | ClearStencilBuffer;
    if (!bufferMask || (bufferMask & ~validMask))
        return Result::InvalidArgument;
    if (!m_backend.clear)
        return Result::MissingBackend;
    return m_backend.clear(m_backend.context, bufferMask)
        ? Result::Ok : Result::Unsupported;
}

bool Driver::validPrimitive(Primitive primitive)
{
    switch (primitive) {
    case Primitive::Points:
    case Primitive::Lines:
    case Primitive::LineStrip:
    case Primitive::Triangles:
    case Primitive::TriangleStrip:
    case Primitive::TriangleFan:
        return true;
    }
    return false;
}

NativePrimitive Driver::translatePrimitive(Primitive primitive)
{
    switch (primitive) {
    case Primitive::Points:
        return NativePrimitive::Points;
    case Primitive::Lines:
        return NativePrimitive::Lines;
    case Primitive::LineStrip:
        return NativePrimitive::LineStrip;
    case Primitive::Triangles:
        return NativePrimitive::Triangles;
    case Primitive::TriangleStrip:
        return NativePrimitive::TriangleStrip;
    case Primitive::TriangleFan:
        return NativePrimitive::TriangleFan;
    }
    // Callers validate first; this is a defensive fallback, not a translation.
    return NativePrimitive::Triangles;
}

Result Driver::drawArrays(Primitive primitive, int32_t first, int32_t count)
{
    return drawArraysInstanced(primitive, first, count, 1);
}

Result Driver::drawArraysInstanced(Primitive primitive, int32_t first, int32_t count, int32_t instances)
{
    if (!validPrimitive(primitive) || first < 0 || count < 0 || instances < 0)
        return Result::InvalidArgument;

    // OpenGL specifies zero-count and zero-instance draws as no-ops.
    if (!count || !instances)
        return Result::Ok;

    if (!m_backend.draw)
        return Result::MissingBackend;

    if (!m_backend.draw(m_backend.context, translatePrimitive(primitive),
            static_cast<uint32_t>(first), static_cast<uint32_t>(count),
            static_cast<uint32_t>(instances)))
        return Result::Unsupported;

    return Result::Ok;
}

Result Driver::unsupported(const char* operation)
{
    if (m_backend.reportUnsupported)
        m_backend.reportUnsupported(m_backend.context,
            operation ? operation : "(unnamed OpenGL operation)");
    return Result::Unsupported;
}

} // namespace GLGX2
} // namespace WKC
