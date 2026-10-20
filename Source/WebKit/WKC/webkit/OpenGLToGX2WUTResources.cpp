#include "OpenGLToGX2WUTResources.h"

#if defined(__WUT__)
#include <gx2r/resource.h>

#include <cstring>
#include <limits>
#include <new>

namespace WKC {
namespace GLGX2 {
namespace {

bool createBuffer(void* context, BufferName name, size_t size, BufferUsage)
{
    if (!name || size > std::numeric_limits<uint32_t>::max())
        return false;

    auto& resources = *static_cast<WUTBufferResources*>(context);
    auto existing = resources.buffers.find(name);
    if (existing != resources.buffers.end())
        return false;

    auto* buffer = new (std::nothrow) GX2RBuffer {};
    if (!buffer)
        return false;

    // The allocation is writable by the CPU and readable by the GPU. Declare
    // all common GL buffer roles up front because glBufferData can precede the
    // first target-specific bind in the GL object model.
    buffer->flags = static_cast<GX2RResourceFlags>(
        GX2R_RESOURCE_BIND_VERTEX_BUFFER
        | GX2R_RESOURCE_BIND_INDEX_BUFFER
        | GX2R_RESOURCE_BIND_UNIFORM_BLOCK
        | GX2R_RESOURCE_USAGE_CPU_WRITE
        | GX2R_RESOURCE_USAGE_GPU_READ);
    buffer->elemSize = 1;
    buffer->elemCount = static_cast<uint32_t>(size);

    if (!GX2RCreateBuffer(buffer)) {
        delete buffer;
        return false;
    }

    try {
        resources.buffers.emplace(name, buffer);
    } catch (const std::bad_alloc&) {
        GX2RDestroyBufferEx(buffer, buffer->flags);
        delete buffer;
        return false;
    }
    return true;
}

bool uploadBuffer(void* context, BufferName name, size_t offset, const void* data, size_t size)
{
    auto& resources = *static_cast<WUTBufferResources*>(context);
    auto it = resources.buffers.find(name);
    if (it == resources.buffers.end() || (size && !data))
        return false;

    GX2RBuffer* buffer = it->second;
    const size_t capacity = static_cast<size_t>(buffer->elemSize) * buffer->elemCount;
    if (offset > capacity || size > capacity - offset)
        return false;
    if (!size)
        return true;

    void* memory = GX2RLockBufferEx(buffer,
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));
    if (!memory)
        return false;
    std::memcpy(static_cast<uint8_t*>(memory) + offset, data, size);
    GX2RUnlockBufferEx(buffer,
        static_cast<GX2RResourceFlags>(GX2R_RESOURCE_USAGE_CPU_WRITE));
    return true;
}

bool destroyBuffer(void* context, BufferName name)
{
    auto& resources = *static_cast<WUTBufferResources*>(context);
    auto it = resources.buffers.find(name);
    if (it == resources.buffers.end())
        return false;

    GX2RBuffer* buffer = it->second;
    GX2RDestroyBufferEx(buffer, buffer->flags);
    delete buffer;
    resources.buffers.erase(it);
    for (auto& binding : resources.bindings) {
        if (binding.second == name)
            binding.second = 0;
    }
    return true;
}

bool bindBuffer(void* context, BufferTarget target, BufferName name)
{
    auto& resources = *static_cast<WUTBufferResources*>(context);
    if (name && resources.buffers.find(name) == resources.buffers.end())
        return false;
    resources.bindings[static_cast<uint8_t>(target)] = name;
    return true;
}

} // namespace

BufferBackend makeWUTBufferBackend(WUTBufferResources& resources)
{
    BufferBackend backend;
    backend.context = &resources;
    backend.create = createBuffer;
    backend.upload = uploadBuffer;
    backend.destroy = destroyBuffer;
    backend.bind = bindBuffer;
    return backend;
}

} // namespace GLGX2
} // namespace WKC
#endif
