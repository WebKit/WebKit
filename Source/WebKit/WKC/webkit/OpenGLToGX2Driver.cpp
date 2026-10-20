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
    return setCapability(capability, true);
}

Result Driver::disable(Capability capability)
{
    return setCapability(capability, false);
}

Result Driver::setCapability(Capability capability, bool enabled)
{
    if (!m_backend.setCapability)
        return Result::MissingBackend;
    return m_backend.setCapability(m_backend.context, capability, enabled)
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
