#include "OpenGLToGX2Buffers.h"

#include <limits>
#include <new>
#include <vector>

namespace WKC {
namespace GLGX2 {

BufferObjects::BufferObjects(const BufferBackend& backend)
    : m_backend(backend)
{
}

BufferName BufferObjects::nextName()
{
    // Name zero is reserved by the GL object model. Skip live names on wrap.
    for (uint64_t attempts = 0; attempts < std::numeric_limits<uint32_t>::max() - 1ULL; ++attempts) {
        BufferName candidate = m_nextName++;
        if (!m_nextName)
            m_nextName = 1;
        if (candidate && m_buffers.find(candidate) == m_buffers.end())
            return candidate;
    }
    return 0;
}

BufferResult BufferObjects::create(BufferName& output)
{
    output = nextName();
    if (!output)
        return BufferResult::OutOfMemory;

    try {
        const auto insertion = m_buffers.emplace(output, BufferInfo {});
        if (!insertion.second) {
            output = 0;
            return BufferResult::OutOfMemory;
        }
    } catch (const std::bad_alloc&) {
        output = 0;
        return BufferResult::OutOfMemory;
    }
    return BufferResult::Ok;
}

const BufferInfo* BufferObjects::find(BufferName name) const
{
    auto it = m_buffers.find(name);
    return it == m_buffers.end() ? nullptr : &it->second;
}

BufferInfo* BufferObjects::find(BufferName name)
{
    auto it = m_buffers.find(name);
    return it == m_buffers.end() ? nullptr : &it->second;
}

BufferResult BufferObjects::destroy(BufferName name)
{
    if (!name)
        return BufferResult::InvalidName;
    auto it = m_buffers.find(name);
    if (it == m_buffers.end())
        return BufferResult::InvalidName;

    if (it->second.allocated && !m_backend.destroy)
        return BufferResult::MissingBackend;

    // Unbind first so a backend failure cannot leave a deleted GPU allocation
    // represented as a live, allocated GL object. Keep the logical bindings
    // unchanged until every backend operation succeeds, and roll back any
    // successful native unbinds if a later unbind or destruction fails.
    std::vector<uint8_t> unboundTargets;
    try {
        unboundTargets.reserve(m_bindings.size());
    } catch (const std::bad_alloc&) {
        return BufferResult::OutOfMemory;
    }

    auto restoreBindings = [&] {
        if (!m_backend.bind)
            return;
        for (uint8_t target : unboundTargets)
            m_backend.bind(m_backend.context, static_cast<BufferTarget>(target), name);
    };

    if (m_backend.bind) {
        for (const auto& binding : m_bindings) {
            if (binding.second != name)
                continue;
            if (!m_backend.bind(m_backend.context,
                    static_cast<BufferTarget>(binding.first), 0)) {
                restoreBindings();
                return BufferResult::BackendFailure;
            }
            unboundTargets.push_back(binding.first);
        }
    }

    if (it->second.allocated && !m_backend.destroy(m_backend.context, name)) {
        restoreBindings();
        return BufferResult::BackendFailure;
    }

    for (auto& binding : m_bindings) {
        if (binding.second == name)
            binding.second = 0;
    }
    m_buffers.erase(it);
    return BufferResult::Ok;
}

BufferResult BufferObjects::bind(BufferTarget target, BufferName name)
{
    if (name && !find(name))
        return BufferResult::InvalidName;
    if (m_backend.bind && !m_backend.bind(m_backend.context, target, name))
        return BufferResult::BackendFailure;
    if (name && !m_backend.bind)
        return BufferResult::MissingBackend;
    m_bindings[static_cast<uint8_t>(target)] = name;
    return BufferResult::Ok;
}

BufferResult BufferObjects::allocate(BufferName name, size_t size, BufferUsage usage, const void* initialData)
{
    BufferInfo* buffer = find(name);
    if (!buffer)
        return BufferResult::InvalidName;
    if (!m_backend.create || !m_backend.upload)
        return BufferResult::MissingBackend;

    // Create may be called again for an existing buffer to replace its store.
    // Destroy the old GPU allocation first, and preserve the object if it fails.
    if (buffer->allocated) {
        if (!m_backend.destroy)
            return BufferResult::MissingBackend;
        if (!m_backend.destroy(m_backend.context, name))
            return BufferResult::BackendFailure;
        buffer->allocated = false;
        buffer->size = 0;
    }

    if (!m_backend.create(m_backend.context, name, size, usage))
        return BufferResult::BackendFailure;

    if (size && initialData && !m_backend.upload(m_backend.context, name, 0, initialData, size)) {
        if (m_backend.destroy)
            m_backend.destroy(m_backend.context, name);
        buffer->size = 0;
        buffer->allocated = false;
        return BufferResult::BackendFailure;
    }

    buffer->size = size;
    buffer->usage = usage;
    buffer->allocated = true;
    return BufferResult::Ok;
}

BufferResult BufferObjects::update(BufferName name, size_t offset, size_t size, const void* data)
{
    BufferInfo* buffer = find(name);
    if (!buffer)
        return BufferResult::InvalidName;
    if (!buffer->allocated)
        return BufferResult::InvalidArgument;
    if (offset > buffer->size || size > buffer->size - offset)
        return BufferResult::InvalidArgument;
    if (size && !data)
        return BufferResult::InvalidArgument;
    if (!size)
        return BufferResult::Ok;
    if (!m_backend.upload)
        return BufferResult::MissingBackend;
    return m_backend.upload(m_backend.context, name, offset, data, size)
        ? BufferResult::Ok : BufferResult::BackendFailure;
}

BufferName BufferObjects::bound(BufferTarget target) const
{
    auto it = m_bindings.find(static_cast<uint8_t>(target));
    return it == m_bindings.end() ? 0 : it->second;
}

bool BufferObjects::info(BufferName name, BufferInfo& output) const
{
    const BufferInfo* buffer = find(name);
    if (!buffer)
        return false;
    output = *buffer;
    return true;
}

} // namespace GLGX2
} // namespace WKC
