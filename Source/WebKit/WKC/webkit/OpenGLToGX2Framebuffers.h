/*
 * GL framebuffer/renderbuffer object state for the GX2 compatibility backend.
 * Native GX2 render-target allocation and attachment operations are delegated
 * to callbacks; completeness is never assumed without a backend check.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace WKC {
namespace GLGX2 {

using FramebufferName = uint32_t;
using RenderbufferName = uint32_t;
using TextureObjectName = uint32_t;

enum class Attachment : uint8_t { Color0, Color1, Color2, Color3, Depth, Stencil, DepthStencil };
enum class FramebufferStatus : uint8_t {
    Complete, MissingAttachment, IncompleteAttachment, Unsupported, BackendFailure
};
enum class FramebufferResult : uint8_t {
    Ok, InvalidArgument, InvalidName, MissingBackend, Unsupported, OutOfMemory, BackendFailure
};

struct RenderbufferDescriptor {
    uint32_t width { 0 };
    uint32_t height { 0 };
    uint32_t samples { 1 };
    uint32_t internalFormat { 0 };
};

struct FramebufferBackend {
    void* context { nullptr };
    bool (*createFramebuffer)(void*, uint64_t& nativeObject) { nullptr };
    void (*destroyFramebuffer)(void*, uint64_t nativeObject) { nullptr };
    bool (*createRenderbuffer)(void*, uint64_t& nativeObject) { nullptr };
    void (*destroyRenderbuffer)(void*, uint64_t nativeObject) { nullptr };
    bool (*allocateRenderbuffer)(void*, uint64_t nativeObject, const RenderbufferDescriptor&) { nullptr };
    bool (*attachTexture)(void*, uint64_t nativeFramebuffer, Attachment,
        uint64_t nativeTexture, uint32_t mipLevel, uint32_t layer) { nullptr };
    bool (*attachRenderbuffer)(void*, uint64_t nativeFramebuffer, Attachment,
        uint64_t nativeRenderbuffer) { nullptr };
    FramebufferStatus (*check)(void*, uint64_t nativeFramebuffer) { nullptr };
    bool (*bind)(void*, uint64_t nativeFramebuffer) { nullptr };
};

struct FramebufferInfo {
    bool hasAttachment { false };
    FramebufferStatus status { FramebufferStatus::MissingAttachment };
};

class Framebuffers final {
public:
    explicit Framebuffers(const FramebufferBackend&);

    FramebufferResult createFramebuffer(FramebufferName&);
    FramebufferResult destroyFramebuffer(FramebufferName);
    FramebufferResult createRenderbuffer(RenderbufferName&);
    FramebufferResult destroyRenderbuffer(RenderbufferName);
    FramebufferResult allocateRenderbuffer(RenderbufferName, const RenderbufferDescriptor&);
    FramebufferResult attachTexture(FramebufferName, Attachment, TextureObjectName,
        uint64_t nativeTexture, uint32_t mipLevel = 0, uint32_t layer = 0);
    FramebufferResult attachRenderbuffer(FramebufferName, Attachment, RenderbufferName);
    FramebufferResult bind(FramebufferName);
    FramebufferStatus check(FramebufferName) const;
    bool info(FramebufferName, FramebufferInfo&) const;

private:
    struct Renderbuffer {
        uint64_t native { 0 };
        RenderbufferDescriptor descriptor;
        bool allocated { false };
    };
    struct AttachmentBinding {
        bool attached { false };
        bool texture { false };
        uint32_t objectName { 0 };
    };
    struct Framebuffer {
        uint64_t native { 0 };
        std::unordered_map<Attachment, AttachmentBinding> attachments;
    };

    FramebufferName nextFramebufferName();
    RenderbufferName nextRenderbufferName();

    FramebufferBackend m_backend;
    FramebufferName m_nextFramebuffer { 1 };
    RenderbufferName m_nextRenderbuffer { 1 };
    FramebufferName m_bound { 0 };
    std::unordered_map<FramebufferName, Framebuffer> m_framebuffers;
    std::unordered_map<RenderbufferName, Renderbuffer> m_renderbuffers;
};

} // namespace GLGX2
} // namespace WKC
