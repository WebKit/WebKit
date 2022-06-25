/*
 *  Copyright (C) 2018 Igalia S.L
 *  Copyright (C) 2018 Metrological Group B.V.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GstAllocatorFastMalloc.h"

#include <gst/gst.h>
#include <wtf/FastMalloc.h>

typedef struct {
    GstMemory base;

    uint8_t* data;
} GstMemoryFastMalloc;

typedef struct {
    GstAllocator parent;
} GstAllocatorFastMalloc;

typedef struct {
    GstAllocatorClass parent;
} GstAllocatorFastMallocClass;

G_DEFINE_TYPE(GstAllocatorFastMalloc, gst_allocator_fast_malloc, GST_TYPE_ALLOCATOR)

static GstMemoryFastMalloc* gstMemoryFastMallocNew(GstAllocator* allocator, gsize size, gsize alignment, gsize offset, gsize padding, GstMemoryFlags flags)
{
    // alignment should be a (power-of-two - 1).
    alignment |= gst_memory_alignment;
    ASSERT(!((alignment + 1) & alignment));

    // GStreamer's allocator requires heap allocations.
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    gsize headerSize = (sizeof(GstMemoryFastMalloc) + alignment) & ~alignment;
    gsize allocationSize = offset + size + padding;
    auto* memory = static_cast<GstMemoryFastMalloc*>(tryFastAlignedMalloc(alignment + 1, headerSize + allocationSize));
    if (!memory)
        return nullptr;

    memory->data = reinterpret_cast<uint8_t*>(memory) + headerSize;

    if (offset && (flags & GST_MEMORY_FLAG_ZERO_PREFIXED))
        std::memset(memory->data, 0, offset);
    if (padding && (flags & GST_MEMORY_FLAG_ZERO_PADDED))
        std::memset(memory->data + offset + size, 0, padding);

    gst_memory_init(GST_MEMORY_CAST(memory), flags, allocator, nullptr, allocationSize, alignment, offset, size);

    return memory;
}

static GstMemory* gstAllocatorFastMallocAlloc(GstAllocator* allocator, gsize size, GstAllocationParams* params)
{
    ASSERT(G_TYPE_CHECK_INSTANCE_TYPE(allocator, gst_allocator_fast_malloc_get_type()));

    return GST_MEMORY_CAST(gstMemoryFastMallocNew(allocator, size, params->align, params->prefix, params->padding, params->flags));
}

static void gstAllocatorFastMallocFree(GstAllocator* allocator, GstMemory* memory)
{
#if ASSERT_ENABLED
    ASSERT(G_TYPE_CHECK_INSTANCE_TYPE(allocator, gst_allocator_fast_malloc_get_type()));
#else
    UNUSED_PARAM(allocator);
#endif

    fastAlignedFree(memory);
}

static gpointer gstAllocatorFastMallocMemMap(GstMemoryFastMalloc* memory, gsize, GstMapFlags)
{
    return memory->data;
}

static void gstAllocatorFastMallocMemUnmap(GstMemoryFastMalloc*)
{
}

static GstMemoryFastMalloc* gstAllocatorFastMallocMemCopy(GstMemoryFastMalloc* memory, gssize offset, gsize size)
{
    if (size == static_cast<gsize>(-1))
        size = memory->base.size > static_cast<gsize>(offset) ? memory->base.size - offset : 0;

    auto* copy = gstMemoryFastMallocNew(memory->base.allocator, size, memory->base.align, 0, 0, static_cast<GstMemoryFlags>(0));
    if (!copy)
        return nullptr;

    std::memcpy(copy->data, memory->data + memory->base.offset + offset, size);
    return copy;
}

static GstMemoryFastMalloc* gstAllocatorFastMallocMemShare(GstMemoryFastMalloc* memory, gssize offset, gsize size)
{
    GstMemoryFastMalloc* sharedMemory;
    if (!tryFastMalloc(sizeof(GstMemoryFastMalloc)).getValue(sharedMemory))
        return nullptr;

    sharedMemory->data = memory->data;

    if (size == static_cast<gsize>(-1))
        size = memory->base.size - offset;

    auto* parent = memory->base.parent ? memory->base.parent : GST_MEMORY_CAST(memory);
    gst_memory_init(GST_MEMORY_CAST(sharedMemory),
        static_cast<GstMemoryFlags>(GST_MINI_OBJECT_FLAGS(parent) | GST_MINI_OBJECT_FLAG_LOCK_READONLY),
        memory->base.allocator, parent, memory->base.maxsize, memory->base.align,
        memory->base.offset + offset, size);

    return sharedMemory;
}

static gboolean gstAllocatorFastMallocMemIsSpan(GstMemoryFastMalloc* memory, GstMemoryFastMalloc* other, gsize* offset)
{
    if (offset) {
        auto* parent = reinterpret_cast<GstMemoryFastMalloc*>(memory->base.parent);
        ASSERT(parent);
        *offset = memory->base.offset - parent->base.offset;
    }

    return memory->data + memory->base.offset + memory->base.size == other->data + other->base.offset;
}

static void gst_allocator_fast_malloc_class_init(GstAllocatorFastMallocClass* klass)
{
    auto* gstAllocatorClass = GST_ALLOCATOR_CLASS(klass);
    gstAllocatorClass->alloc = gstAllocatorFastMallocAlloc;
    gstAllocatorClass->free = gstAllocatorFastMallocFree;
}

static void gst_allocator_fast_malloc_init(GstAllocatorFastMalloc* allocator)
{
    auto* baseAllocator = GST_ALLOCATOR_CAST(allocator);

    baseAllocator->mem_type = "FastMalloc";
    baseAllocator->mem_map = reinterpret_cast<GstMemoryMapFunction>(gstAllocatorFastMallocMemMap);
    baseAllocator->mem_unmap = reinterpret_cast<GstMemoryUnmapFunction>(gstAllocatorFastMallocMemUnmap);
    baseAllocator->mem_copy = reinterpret_cast<GstMemoryCopyFunction>(gstAllocatorFastMallocMemCopy);
    baseAllocator->mem_share = reinterpret_cast<GstMemoryShareFunction>(gstAllocatorFastMallocMemShare);
    baseAllocator->mem_is_span = reinterpret_cast<GstMemoryIsSpanFunction>(gstAllocatorFastMallocMemIsSpan);
}
