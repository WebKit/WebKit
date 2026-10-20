/*
 * OpenGL-to-GX2 compatibility layer for the Wii U WKC port.
 *
 * This is a backend-neutral command/state translator. The GX2 binding supplied
 * by the embedder owns native GX2 objects and shader binaries; this layer
 * validates GL-style state and translates supported draw/state operations.
 *
 * It intentionally reports unsupported operations instead of silently
 * pretending that desktop OpenGL 4.6 functionality is available.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace WKC {
namespace GLGX2 {

enum class Result : uint8_t {
    Ok,
    InvalidArgument,
    MissingBackend,
    Unsupported,
};

enum class Primitive : uint8_t {
    Points,
    Lines,
    LineStrip,
    Triangles,
    TriangleStrip,
    TriangleFan,
};

enum class NativePrimitive : uint8_t {
    Points,
    Lines,
    LineStrip,
    Triangles,
    TriangleStrip,
    TriangleFan,
};

enum class Capability : uint8_t {
    Blend,
    CullFace,
    DepthTest,
    ScissorTest,
    StencilTest,
    PolygonOffsetFill,
};

struct Viewport {
    int32_t x { 0 };
    int32_t y { 0 };
    int32_t width { 0 };
    int32_t height { 0 };
    float nearDepth { 0.0f };
    float farDepth { 1.0f };
};

struct Scissor {
    int32_t x { 0 };
    int32_t y { 0 };
    int32_t width { 0 };
    int32_t height { 0 };
};

/*
 * The platform binding maps these callbacks to the matching GX2/WUT calls.
 * Keeping GX2 SDK types out of this public header allows WebCore and host-side
 * unit tests to compile without including console-only SDK headers.
 */
struct Backend {
    void* context { nullptr };
    bool (*setViewport)(void*, const Viewport&) { nullptr };
    bool (*setScissor)(void*, const Scissor&) { nullptr };
    bool (*setCapability)(void*, Capability, bool) { nullptr };
    bool (*draw)(void*, NativePrimitive, uint32_t first, uint32_t count, uint32_t instances) { nullptr };
    void (*reportUnsupported)(void*, const char* operation) { nullptr };
};

class Driver final {
public:
    explicit Driver(const Backend& backend);

    Result setViewport(const Viewport&);
    Result setScissor(const Scissor&);
    Result enable(Capability);
    Result disable(Capability);
    Result drawArrays(Primitive, int32_t first, int32_t count);
    Result drawArraysInstanced(Primitive, int32_t first, int32_t count, int32_t instances);

    // Use for GL operations for which no semantically equivalent GX2 path has
    // been implemented. This must not be treated as successful emulation.
    Result unsupported(const char* operation);

private:
    Result setCapability(Capability, bool);
    static bool validPrimitive(Primitive);
    static NativePrimitive translatePrimitive(Primitive);

    Backend m_backend;
    Viewport m_viewport;
    Scissor m_scissor;
};

} // namespace GLGX2
} // namespace WKC
