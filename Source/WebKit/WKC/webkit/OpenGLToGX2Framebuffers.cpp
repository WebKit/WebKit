#include "OpenGLToGX2Framebuffers.h"

#include <limits>
#include <new>

namespace WKC {
namespace GLGX2 {

Framebuffers::Framebuffers(const FramebufferBackend& backend)
    : m_backend(backend)
{
}

FramebufferName Framebuffers::nextFramebufferName()
{
    for (uint64_t attempts = 0; attempts < std::numeric_limits<FramebufferName>::max(); ++attempts) {
        FramebufferName candidate = m_nextFramebuffer++;
        if (!m_nextFramebuffer)
            m_nextFramebuffer = 1;
        if (candidate && !m_framebuffers.count(candidate))
            return candidate;
    }
    return 0;
}

RenderbufferName Framebuffers::nextRenderbufferName()
{
    for (uint64_t attempts = 0; attempts < std::numeric_limits<RenderbufferName>::max(); ++attempts) {
        RenderbufferName candidate = m_nextRenderbuffer++;
        if (!m_nextRenderbuffer)
            m_nextRenderbuffer = 1;
        if (candidate && !m_renderbuffers.count(candidate))
            return candidate;
    }
    return 0;
}

FramebufferResult Framebuffers::createFramebuffer(FramebufferName& output)
{
    output = 0;
    if (!m_backend.createFramebuffer || !m_backend.destroyFramebuffer)
        return FramebufferResult::MissingBackend;

    const FramebufferName name = nextFramebufferName();
    if (!name)
        return FramebufferResult::OutOfMemory;

    Framebuffer object;
    if (!m_backend.createFramebuffer(m_backend.context, object.native) || !object.native)
        return FramebufferResult::BackendFailure;

    try {
        m_framebuffers.emplace(name, object);
    } catch (const std::bad_alloc&) {
        m_backend.destroyFramebuffer(m_backend.context, object.native);
        return FramebufferResult::OutOfMemory;
    }
    output = name;
    return FramebufferResult::Ok;
}

FramebufferResult Framebuffers::destroyFramebuffer(FramebufferName name)
{
    auto it = m_framebuffers.find(name);
    if (it == m_framebuffers.end())
        return FramebufferResult::InvalidName;
    if (!m_backend.destroyFramebuffer)
        return FramebufferResult::MissingBackend;
    if (m_bound == name) {
        if (!m_backend.bind)
            return FramebufferResult::MissingBackend;
        if (!m_backend.bind(m_backend.context, 0))
            return FramebufferResult::BackendFailure;
        m_bound = 0;
    }
    m_backend.destroyFramebuffer(m_backend.context, it->second.native);
    m_framebuffers.erase(it);
    return FramebufferResult::Ok;
}

FramebufferResult Framebuffers::createRenderbuffer(RenderbufferName& output)
{
    output = 0;
    if (!m_backend.createRenderbuffer || !m_backend.destroyRenderbuffer)
        return FramebufferResult::MissingBackend;

    const RenderbufferName name = nextRenderbufferName();
    if (!name)
        return FramebufferResult::OutOfMemory;

    Renderbuffer object;
    if (!m_backend.createRenderbuffer(m_backend.context, object.native) || !object.native)
        return FramebufferResult::BackendFailure;
    try {
        m_renderbuffers.emplace(name, object);
    } catch (const std::bad_alloc&) {
        m_backend.destroyRenderbuffer(m_backend.context, object.native);
        return FramebufferResult::OutOfMemory;
    }
    output = name;
    return FramebufferResult::Ok;
}

FramebufferResult Framebuffers::destroyRenderbuffer(RenderbufferName name)
{
    auto it = m_renderbuffers.find(name);
    if (it == m_renderbuffers.end())
        return FramebufferResult::InvalidName;
    if (!m_backend.destroyRenderbuffer)
        return FramebufferResult::MissingBackend;

    // No detach callback exists yet. Refuse to destroy an attached object rather
    // than leave native framebuffer state pointing at a released GX2 resource.
    for (const auto& framebuffer : m_framebuffers) {
        for (const auto& attachment : framebuffer.second.attachments) {
            const auto& binding = attachment.second;
            if (binding.attached && !binding.texture && binding.objectName == name)
                return FramebufferResult::InvalidArgument;
        }
    }

    m_backend.destroyRenderbuffer(m_backend.context, it->second.native);
    m_renderbuffers.erase(it);
    return FramebufferResult::Ok;
}

FramebufferResult Framebuffers::allocateRenderbuffer(RenderbufferName name,
    const RenderbufferDescriptor& descriptor)
{
    auto it = m_renderbuffers.find(name);
    if (it == m_renderbuffers.end())
        return FramebufferResult::InvalidName;
    if (!descriptor.width || !descriptor.height || !descriptor.samples)
        return FramebufferResult::InvalidArgument;
    if (!m_backend.allocateRenderbuffer)
        return FramebufferResult::MissingBackend;
    if (!m_backend.allocateRenderbuffer(m_backend.context, it->second.native, descriptor))
        return FramebufferResult::Unsupported;
    it->second.descriptor = descriptor;
    it->second.allocated = true;
    return FramebufferResult::Ok;
}

FramebufferResult Framebuffers::attachTexture(FramebufferName framebufferName,
    Attachment attachment, TextureObjectName textureName, uint64_t nativeTexture,
    uint32_t mipLevel, uint32_t layer)
{
    auto it = m_framebuffers.find(framebufferName);
    if (it == m_framebuffers.end())
        return FramebufferResult::InvalidName;
    if (!textureName || !nativeTexture)
        return FramebufferResult::InvalidArgument;
    if (!m_backend.attachTexture)
        return FramebufferResult::MissingBackend;
    // Reserve bookkeeping before mutating native GX2 state. If allocation
    // fails after the native attach succeeds, the attachment would otherwise
    // exist in GX2 but be invisible to completeness and lifetime checks.
    auto existing = it->second.attachments.find(attachment);
    bool inserted = false;
    if (existing == it->second.attachments.end()) {
        try {
            existing = it->second.attachments.emplace(attachment, AttachmentBinding {}).first;
            inserted = true;
        } catch (const std::bad_alloc&) {
            return FramebufferResult::OutOfMemory;
        }
    }
    if (!m_backend.attachTexture(m_backend.context, it->second.native, attachment,
            nativeTexture, mipLevel, layer)) {
        if (inserted)
            it->second.attachments.erase(existing);
        return FramebufferResult::Unsupported;
    }
    existing->second = AttachmentBinding { true, true, textureName };
    return FramebufferResult::Ok;
}

FramebufferResult Framebuffers::attachRenderbuffer(FramebufferName framebufferName,
    Attachment attachment, RenderbufferName renderbufferName)
{
    auto framebuffer = m_framebuffers.find(framebufferName);
    if (framebuffer == m_framebuffers.end())
        return FramebufferResult::InvalidName;
    auto renderbuffer = m_renderbuffers.find(renderbufferName);
    if (renderbuffer == m_renderbuffers.end())
        return FramebufferResult::InvalidName;
    if (!renderbuffer->second.allocated)
        return FramebufferResult::InvalidArgument;
    if (!m_backend.attachRenderbuffer)
        return FramebufferResult::MissingBackend;
    // Keep the GL-side attachment record allocation-safe before changing
    // the native framebuffer, matching attachTexture's failure guarantees.
    auto existing = framebuffer->second.attachments.find(attachment);
    bool inserted = false;
    if (existing == framebuffer->second.attachments.end()) {
        try {
            existing = framebuffer->second.attachments.emplace(attachment, AttachmentBinding {}).first;
            inserted = true;
        } catch (const std::bad_alloc&) {
            return FramebufferResult::OutOfMemory;
        }
    }
    if (!m_backend.attachRenderbuffer(m_backend.context, framebuffer->second.native,
            attachment, renderbuffer->second.native)) {
        if (inserted)
            framebuffer->second.attachments.erase(existing);
        return FramebufferResult::Unsupported;
    }
    existing->second = AttachmentBinding { true, false, renderbufferName };
    return FramebufferResult::Ok;
}

FramebufferResult Framebuffers::bind(FramebufferName name)
{
    uint64_t native = 0;
    if (name) {
        auto it = m_framebuffers.find(name);
        if (it == m_framebuffers.end())
            return FramebufferResult::InvalidName;
        native = it->second.native;
    }
    if (!m_backend.bind)
        return FramebufferResult::MissingBackend;
    if (!m_backend.bind(m_backend.context, native))
        return FramebufferResult::BackendFailure;
    m_bound = name;
    return FramebufferResult::Ok;
}

FramebufferStatus Framebuffers::check(FramebufferName name) const
{
    if (!name)
        return FramebufferStatus::Complete; // default framebuffer is embedder-owned
    auto it = m_framebuffers.find(name);
    if (it == m_framebuffers.end())
        return FramebufferStatus::BackendFailure;
    if (it->second.attachments.empty())
        return FramebufferStatus::MissingAttachment;
    if (!m_backend.check)
        return FramebufferStatus::Unsupported;
    return m_backend.check(m_backend.context, it->second.native);
}

bool Framebuffers::info(FramebufferName name, FramebufferInfo& output) const
{
    if (!name) {
        output.hasAttachment = true;
        output.status = FramebufferStatus::Complete;
        return true;
    }
    auto it = m_framebuffers.find(name);
    if (it == m_framebuffers.end())
        return false;
    output.hasAttachment = !it->second.attachments.empty();
    output.status = check(name);
    return true;
}

} // namespace GLGX2
} // namespace WKC
