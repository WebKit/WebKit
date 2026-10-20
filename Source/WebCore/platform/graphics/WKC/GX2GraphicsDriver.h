/*
 * Wii U GX2 graphics backend interface for WebKit's WKC port.
 *
 * GX2 is not OpenGL 4.6. This interface deliberately exposes the portable
 * subset that a WebGL/OpenGL compatibility layer can implement on GX2.
 * Shader translation and WebGL entry-point dispatch must be wired by the
 * GraphicsContext3D backend before this is a complete driver.
 */
#pragma once

#if defined(__WIIU__) || defined(WIIU) || defined(WKC_USE_GX2)

#include <cstddef>
#include <cstdint>

namespace WebCore {
namespace GX2Driver {

enum class Result : uint8_t {
    Success,
    NotInitialized,
    InvalidArgument,
    Unsupported,
    OutOfMemory,
    DriverError
};

enum class Primitive : uint8_t {
    Points,
    Lines,
    LineStrip,
    Triangles,
    TriangleStrip
};

enum class BufferTarget : uint8_t {
    Vertex,
    Index,
    Uniform
};

enum class CompareFunc : uint8_t {
    Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always
};

struct ContextAttributes {
    bool alpha;
    bool depth;
    bool stencil;
    bool antialias;
    uint32_t width;
    uint32_t height;
};

struct BufferHandle {
    uint32_t id;
    size_t size;
};

struct TextureHandle {
    uint32_t id;
    uint32_t width;
    uint32_t height;
    uint32_t format;
};

struct Viewport {
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    float nearDepth;
    float farDepth;
};

/*
 * This is the low-level driver contract, not a claim that all OpenGL 4.6
 * features are available on GX2. Implementations must return Unsupported
 * for features that cannot be faithfully emulated.
 */
class Driver {
public:
    Driver();
    ~Driver();

    Driver(const Driver&) = delete;
    Driver& operator=(const Driver&) = delete;

    Result initialize(const ContextAttributes&);
    void shutdown();
    bool isInitialized() const;

    Result resize(uint32_t width, uint32_t height);
    Result setViewport(const Viewport&);
    Result clearColor(float red, float green, float blue, float alpha);
    Result clear(bool color, bool depth, bool stencil);

    Result createBuffer(BufferTarget, size_t size, const void* initialData,
                        BufferHandle& output);
    Result updateBuffer(const BufferHandle&, size_t offset, size_t size,
                        const void* data);
    Result deleteBuffer(BufferHandle&);
    Result bindBuffer(BufferTarget, const BufferHandle*);

    Result createTexture2D(uint32_t width, uint32_t height, uint32_t format,
                           const void* pixels, size_t byteCount,
                           TextureHandle& output);
    Result deleteTexture(TextureHandle&);

    Result setDepthTest(bool enabled, CompareFunc function, bool writeEnabled);
    Result setBlend(bool enabled);
    Result setCullFace(bool enabled, bool frontFaces);
    Result draw(Primitive, uint32_t first, uint32_t count);
    Result drawIndexed(Primitive, uint32_t count, uint32_t indexType,
                       size_t byteOffset);

    /*
     * WebGL requires GLES2-class semantics. OpenGL 4.6 features such as
     * compute shaders, tessellation, geometry shaders, SSBOs, and many
     * extension paths require separate translation/emulation and must not
     * be advertised until implemented and tested.
     */
    bool supportsWebGL1() const;
    bool supportsWebGL2() const;
    bool supportsOpenGL46() const;

private:
    struct State;
    State* m_state;
};

} // namespace GX2Driver
} // namespace WebCore

#endif // Wii U
