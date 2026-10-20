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

enum class IndexType : uint8_t { UnsignedByte, UnsignedShort, UnsignedInt };

enum class Capability : uint8_t {
    Blend,
    CullFace,
    DepthTest,
    ScissorTest,
    StencilTest,
    PolygonOffsetFill,
};

enum class BlendFactor : uint8_t {
    Zero, One, SourceColor, OneMinusSourceColor, DestinationColor,
    OneMinusDestinationColor, SourceAlpha, OneMinusSourceAlpha,
    DestinationAlpha, OneMinusDestinationAlpha, ConstantColor,
    OneMinusConstantColor, ConstantAlpha, OneMinusConstantAlpha,
    SourceAlphaSaturate,
};

enum class BlendEquation : uint8_t { Add, Subtract, ReverseSubtract, Minimum, Maximum };
enum class Face : uint8_t { Front, Back, FrontAndBack };
enum class Winding : uint8_t { Clockwise, CounterClockwise };
enum class StencilOperation : uint8_t { Keep, Zero, Replace, Increment, Decrement, Invert, IncrementWrap, DecrementWrap };
enum class CompareFunction : uint8_t {
    Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always
};

struct Color {
    float red { 0.0f };
    float green { 0.0f };
    float blue { 0.0f };
    float alpha { 0.0f };
};

struct ColorMask {
    bool red { true };
    bool green { true };
    bool blue { true };
    bool alpha { true };
};

enum ClearBuffer : uint8_t {
    ClearColorBuffer = 1 << 0,
    ClearDepthBuffer = 1 << 1,
    ClearStencilBuffer = 1 << 2,
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
    bool (*setBlendFunction)(void*, BlendFactor sourceRGB, BlendFactor destinationRGB,
        BlendFactor sourceAlpha, BlendFactor destinationAlpha) { nullptr };
    bool (*setBlendEquation)(void*, BlendEquation rgb, BlendEquation alpha) { nullptr };
    bool (*setDepthFunction)(void*, CompareFunction) { nullptr };
    bool (*setDepthWrite)(void*, bool) { nullptr };
    bool (*setDepthRange)(void*, float nearDepth, float farDepth) { nullptr };
    bool (*setCullFace)(void*, Face) { nullptr };
    bool (*setFrontFace)(void*, Winding) { nullptr };
    bool (*setPolygonOffset)(void*, float factor, float units) { nullptr };
    bool (*setStencilFunction)(void*, Face, CompareFunction, int32_t reference, uint32_t mask) { nullptr };
    bool (*setStencilOperation)(void*, Face, StencilOperation stencilFail,
        StencilOperation depthFail, StencilOperation depthPass) { nullptr };
    bool (*setStencilWriteMask)(void*, Face, uint32_t mask) { nullptr };
    bool (*setBlendColor)(void*, const Color&) { nullptr };
    bool (*setColorMask)(void*, const ColorMask&) { nullptr };
    bool (*setClearColor)(void*, const Color&) { nullptr };
    bool (*setClearDepth)(void*, float depth) { nullptr };
    bool (*setClearStencil)(void*, int32_t value) { nullptr };
    bool (*clear)(void*, uint8_t bufferMask) { nullptr };
    bool (*draw)(void*, NativePrimitive, uint32_t first, uint32_t count, uint32_t instances) { nullptr };
    bool (*drawIndexed)(void*, NativePrimitive, IndexType, uint32_t count,
        uint64_t byteOffset, uint32_t instances) { nullptr };
    void (*reportUnsupported)(void*, const char* operation) { nullptr };
};

class Driver final {
public:
    explicit Driver(const Backend& backend);

    Result setViewport(const Viewport&);
    Result setScissor(const Scissor&);
    Result enable(Capability);
    Result disable(Capability);
    Result setBlendFunction(BlendFactor sourceRGB, BlendFactor destinationRGB,
        BlendFactor sourceAlpha, BlendFactor destinationAlpha);
    Result setBlendEquation(BlendEquation rgb, BlendEquation alpha);
    Result setDepthFunction(CompareFunction);
    Result setDepthWrite(bool);
    Result setDepthRange(float nearDepth, float farDepth);
    Result setCullFace(Face);
    Result setFrontFace(Winding);
    Result setPolygonOffset(float factor, float units);
    Result setStencilFunction(Face, CompareFunction, int32_t reference, uint32_t mask);
    Result setStencilOperation(Face, StencilOperation stencilFail,
        StencilOperation depthFail, StencilOperation depthPass);
    Result setStencilWriteMask(Face, uint32_t mask);
    Result setBlendColor(const Color&);
    Result setColorMask(const ColorMask&);
    Result setClearColor(const Color&);
    Result setClearDepth(float depth);
    Result setClearStencil(int32_t value);
    Result clear(uint8_t bufferMask);
    Result drawArrays(Primitive, int32_t first, int32_t count);
    Result drawArraysInstanced(Primitive, int32_t first, int32_t count, int32_t instances);
    Result drawElements(Primitive, int32_t count, IndexType, uint64_t byteOffset);
    Result drawElementsInstanced(Primitive, int32_t count, IndexType, uint64_t byteOffset,
        int32_t instances);

    // Use for GL operations for which no semantically equivalent GX2 path has
    // been implemented. This must not be treated as successful emulation.
    Result unsupported(const char* operation);

private:
    Result setCapability(Capability, bool);
    static bool validPrimitive(Primitive);
    static bool validBlendFactor(BlendFactor);
    static bool validBlendEquation(BlendEquation);
    static bool validCompareFunction(CompareFunction);
    static NativePrimitive translatePrimitive(Primitive);

    Backend m_backend;
    Viewport m_viewport;
    Scissor m_scissor;
};

} // namespace GLGX2
} // namespace WKC
