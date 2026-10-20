/*
 * OpenGL buffer-object compatibility layer for the Wii U GX2 backend.
 *
 * The backend callbacks must allocate GX2-compatible GPU memory and perform
 * the required cache flush/invalidation. CPU-side storage is deliberately not
 * presented as GPU storage when those callbacks are absent.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace WKC {
namespace GLGX2 {

using BufferName = uint32_t;

enum class BufferTarget : uint8_t {
    Array,
    ElementArray,
    Uniform,
    PixelPack,
    PixelUnpack,
    CopyRead,
    CopyWrite,
};

enum class BufferUsage : uint8_t {
    StaticDraw,
    DynamicDraw,
    StreamDraw,
};

enum class BufferResult : uint8_t {
    Ok,
    InvalidArgument,
    InvalidName,
    OutOfMemory,
    MissingBackend,
    BackendFailure,
};

struct BufferBackend {
    void* context { nullptr };
    bool (*create)(void*, BufferName, size_t, BufferUsage) { nullptr };
    bool (*upload)(void*, BufferName, size_t, const void*, size_t) { nullptr };
    bool (*destroy)(void*, BufferName) { nullptr };
    bool (*bind)(void*, BufferTarget, BufferName) { nullptr };
};

struct BufferInfo {
    size_t size { 0 };
    BufferUsage usage { BufferUsage::StaticDraw };
    bool allocated { false };
};

class BufferObjects final {
public:
    explicit BufferObjects(const BufferBackend&);

    BufferResult create(BufferName&);
    BufferResult destroy(BufferName);
    BufferResult bind(BufferTarget, BufferName);
    BufferResult allocate(BufferName, size_t, BufferUsage, const void* initialData = nullptr);
    BufferResult update(BufferName, size_t offset, size_t size, const void*);
    BufferName bound(BufferTarget) const;
    bool info(BufferName, BufferInfo&) const;

private:
    BufferName nextName();
    const BufferInfo* find(BufferName) const;
    BufferInfo* find(BufferName);

    BufferBackend m_backend;
    BufferName m_nextName { 1 };
    std::unordered_map<BufferName, BufferInfo> m_buffers;
    std::unordered_map<uint8_t, BufferName> m_bindings;
};

} // namespace GLGX2
} // namespace WKC
