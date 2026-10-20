/*
 * GL-style texture object state for the Wii U GX2 backend.
 *
 * GPU allocation, upload and binding are supplied by the platform backend.
 * No operation reports success unless the corresponding callback succeeds.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace WKC {
namespace GLGX2 {

using TextureName = uint32_t;

enum class TextureTarget : uint8_t { Texture2D, CubeMap, Texture3D, Texture2DArray };
enum class TextureFormat : uint8_t {
    R8, RG8, RGB8, RGBA8, SRGB8Alpha8, R16F, RG16F, RGBA16F,
    R32F, RG32F, RGBA32F, Depth16, Depth24, Depth24Stencil8
};
enum class TextureResult : uint8_t {
    Ok, InvalidArgument, InvalidName, MissingBackend, Unsupported, OutOfMemory, BackendFailure
};

struct TextureDescriptor {
    TextureTarget target { TextureTarget::Texture2D };
    TextureFormat format { TextureFormat::RGBA8 };
    uint32_t width { 0 };
    uint32_t height { 0 };
    uint32_t depth { 1 };
    uint32_t levels { 1 };
    bool immutableStorage { false };
};

struct TextureBackend {
    void* context { nullptr };
    bool (*create)(void*, TextureTarget, uint64_t& nativeTexture) { nullptr };
    void (*destroy)(void*, uint64_t nativeTexture) { nullptr };
    bool (*allocate)(void*, uint64_t nativeTexture, const TextureDescriptor&) { nullptr };
    bool (*upload)(void*, uint64_t nativeTexture, uint32_t level, uint32_t layer,
        TextureFormat, uint32_t width, uint32_t height, const void*, size_t) { nullptr };
    bool (*bind)(void*, uint32_t unit, TextureTarget, uint64_t nativeTexture) { nullptr };
    bool (*setSampler)(void*, uint64_t nativeTexture, uint32_t minFilter,
        uint32_t magFilter, uint32_t wrapS, uint32_t wrapT, uint32_t wrapR) { nullptr };
};

struct TextureInfo {
    TextureTarget target { TextureTarget::Texture2D };
    TextureDescriptor storage;
    bool allocated { false };
};

class Textures final {
public:
    explicit Textures(const TextureBackend&);

    TextureResult create(TextureTarget, TextureName&);
    TextureResult destroy(TextureName);
    TextureResult allocate(TextureName, const TextureDescriptor&);
    TextureResult upload(TextureName, uint32_t level, uint32_t layer,
        uint32_t width, uint32_t height, const void*, size_t);
    TextureResult bind(uint32_t unit, TextureName);
    TextureResult setSampler(TextureName, uint32_t minFilter, uint32_t magFilter,
        uint32_t wrapS, uint32_t wrapT, uint32_t wrapR);
    bool info(TextureName, TextureInfo&) const;

private:
    struct Object {
        TextureTarget target { TextureTarget::Texture2D };
        TextureDescriptor storage;
        uint64_t native { 0 };
        bool allocated { false };
    };

    TextureName nextName();
    static bool validTarget(TextureTarget);
    static bool validDescriptor(const TextureDescriptor&);
    static bool levelDimensions(const TextureDescriptor&, uint32_t level,
        uint32_t& width, uint32_t& height, uint32_t& depth);

    TextureBackend m_backend;
    TextureName m_nextName { 1 };
    std::unordered_map<TextureName, Object> m_objects;
};

} // namespace GLGX2
} // namespace WKC
