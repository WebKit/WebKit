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
#include <wtf/MallocSpan.h>
#include <wtf/StdLibExtras.h>
#include <wtf/glib/WTFGType.h>

typedef struct {
    // Instance of the base GstMemory class (we're effectively subclassing it, not unlike GstMemorySystem).
    GstMemory base;
    // Span to the tail of an GstMemoryFastMalloc allocation block where the user memory contents will be stored.
    // gstMemoryFastMallocNew() will allocate enough space to fit such tail after GstMemoryFastMalloc.
    // gstAllocatorFastMallocMemShare() only allocates enough space for the GstMemoryFastMalloc struct and
    // instead shares the `data` span with the original (parent) GstMemoryFastMalloc.
    std::span<uint8_t> data;
} GstMemoryFastMalloc;
//
// Note: when allocating GstMemoryFastMalloc we also allocate a long enough tail to hold the contents
// the user will store. That tail is what will be returned by our map() function.

typedef struct {
} GstAllocatorFastMallocPrivate;

typedef struct {
    GstAllocator parent;
    GstAllocatorFastMallocPrivate* priv;
} GstAllocatorFastMalloc;

typedef struct {
    GstAllocatorClass parent;
} GstAllocatorFastMallocClass;

WEBKIT_DEFINE_TYPE(GstAllocatorFastMalloc, gst_allocator_fast_malloc, GST_TYPE_ALLOCATOR)

#define WEBKIT_GST_ALLOCATOR_FAST_MALLOC_CAST(p) reinterpret_cast<GstAllocatorFastMalloc*>(p)
#define GST_MEMORY_FAST_MALLOC_CAST(p) reinterpret_cast<GstMemoryFastMalloc*>(p)

static GstMemoryFastMalloc* gstMemoryFastMallocNew(GstAllocator* allocator, gsize size, gsize alignment, gsize offset, gsize padding, GstMemoryFlags flags)
{
    ASSERT(G_TYPE_CHECK_INSTANCE_TYPE(allocator, gst_allocator_fast_malloc_get_type()));

    // Alignment should be a (power-of-two - 1). That is GStreamer's convention.
    alignment |= gst_memory_alignment;
    ASSERT(!((alignment + 1) & alignment));

    // GStreamer's allocator requires heap allocations.
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;

    // The block we get from FastMalloc consists of:
    // (1) *Header*: GstMemoryFastMalloc struct, plus any extra space needed to satisfy alignment.
    gsize headerSize = (sizeof(GstMemoryFastMalloc) + alignment) & ~alignment; // Rounds up to the nearest alignment unit.
    // (2) *Tail*: the memory that is accessible to the user via GstMemory.map().
    gsize tailSize = offset + size + padding;

    auto totalSize = headerSize + tailSize;
    auto storage = MallocSpan<uint8_t, FastAlignedMalloc>::tryAlignedMalloc(alignment + 1 /* Power of 2 */, totalSize);
    if (!storage)
        return nullptr;
    auto wholeAllocation = storage.leakSpan();

    auto& header = WTF::reinterpretCastSpanStartTo<GstMemoryFastMalloc>(wholeAllocation);
    header.data = wholeAllocation.subspan(headerSize);

    if (offset && (flags & GST_MEMORY_FLAG_ZERO_PREFIXED))
        memsetSpan(header.data.subspan(0, offset), 0);
    if (padding && (flags & GST_MEMORY_FLAG_ZERO_PADDED))
        memsetSpan(header.data.subspan(offset + size, padding), 0);

    gst_memory_init(&header.base, flags, allocator, nullptr, tailSize, alignment, offset, size);
    return &header;
}

static GstMemory* gstAllocatorFastMallocAlloc(GstAllocator* allocator, gsize size, GstAllocationParams* params)
{
    return GST_MEMORY_CAST(gstMemoryFastMallocNew(allocator, size, params->align, params->prefix, params->padding, params->flags));
}

static void gstAllocatorFastMallocFree(GstAllocator* allocator, GstMemory* memory)
{
#if ASSERT_ENABLED
    ASSERT(G_TYPE_CHECK_INSTANCE_TYPE(allocator, gst_allocator_fast_malloc_get_type()));
#else
    UNUSED_PARAM(allocator);
#endif

    fastFree(memory);
}

static gpointer gstAllocatorFastMallocMemMap(GstMemory* baseMemory, gsize, GstMapFlags)
{
    auto memory = GST_MEMORY_FAST_MALLOC_CAST(baseMemory);
    return memory->data.data();
}

static void gstAllocatorFastMallocMemUnmap(GstMemory*)
{
}

static GstMemory* gstAllocatorFastMallocMemCopy(GstMemory* baseMemory, gssize offset, gssize size)
{
    auto memory = GST_MEMORY_FAST_MALLOC_CAST(baseMemory);
    if (size == static_cast<gssize>(-1))
        size = memory->base.size > static_cast<gsize>(offset) ? memory->base.size - offset : 0;

    auto* copy = gstMemoryFastMallocNew(memory->base.allocator, size, memory->base.align, 0, 0, static_cast<GstMemoryFlags>(0));
    if (!copy)
        return nullptr;

    auto destinationSpan = copy->data.subspan(0, size);
    memcpySpan(destinationSpan, memory->data.subspan(memory->base.offset + offset, size));
    return GST_MEMORY_CAST(copy);
}

static GstMemory* gstAllocatorFastMallocMemShare(GstMemory* baseMemory, gssize offset, gssize size)
{
    auto memory = GST_MEMORY_FAST_MALLOC_CAST(baseMemory);
    GstMemoryFastMalloc* sharedMemory;
    if (!tryFastMalloc(sizeof(GstMemoryFastMalloc)).getValue(sharedMemory))
        return nullptr;

    sharedMemory->data = memory->data;

    if (size == static_cast<gssize>(-1))
        size = memory->base.size - offset;

    auto* parent = memory->base.parent ? memory->base.parent : GST_MEMORY_CAST(memory);
    gst_memory_init(GST_MEMORY_CAST(sharedMemory),
        static_cast<GstMemoryFlags>(GST_MINI_OBJECT_FLAGS(parent) | GST_MINI_OBJECT_FLAG_LOCK_READONLY),
        memory->base.allocator, parent, memory->base.maxsize, memory->base.align,
        memory->base.offset + offset, size);

    return GST_MEMORY_CAST(sharedMemory);
}

// Will be called by gst_memory_is_span() after checking that both GstMemories have
// the same allocators and they both share the same non-null parent.
//
// Returns whether the visible end of the first memory coincides with the visible
// start of the second memory and fills `offset` with the offset of the first memory
// within the visible span of the parent memory.
//
// This is used to avoid expensive concatenation: If the function returns true, it
// means you can use gst_memory_share() on the parent memory with the filled offset
// and the combined size of both memories to get a GstMemory that spans both previous
// memories without doing any copies. See _sysmem_is_span() in GStreamer for reference.
static gboolean gstAllocatorFastMallocMemIsSpan(GstMemory* _memory1, GstMemory* _memory2, gsize* offset)
{
    auto memory1 = GST_MEMORY_FAST_MALLOC_CAST(_memory1);
    auto memory2 = GST_MEMORY_FAST_MALLOC_CAST(_memory2);
    if (offset) {
        auto parent = GST_MEMORY_FAST_MALLOC_CAST(memory1->base.parent);
        ASSERT(parent);
        *offset = memory1->base.offset - parent->base.offset;
    }

    auto visibleSpan1 = memory1->data.subspan(memory1->base.offset, memory1->base.size);
    auto visibleSpan2 = memory2->data.subspan(memory2->base.offset, memory2->base.size);
    return visibleSpan1.end() == visibleSpan2.begin();
}

static void gstAllocatorFastMallocConstructed(GObject* object)
{
    G_OBJECT_CLASS(gst_allocator_fast_malloc_parent_class)->constructed(object);
IGNORE_WARNINGS_BEGIN("cast-align")
    GST_OBJECT_FLAG_SET(GST_OBJECT_CAST(object), GST_OBJECT_FLAG_MAY_BE_LEAKED);
    auto allocator = GST_ALLOCATOR_CAST(object);
IGNORE_WARNINGS_END

    allocator->mem_type = "FastMalloc";
    allocator->mem_map = gstAllocatorFastMallocMemMap;
    allocator->mem_unmap = gstAllocatorFastMallocMemUnmap;
    allocator->mem_copy = gstAllocatorFastMallocMemCopy;
    allocator->mem_share = gstAllocatorFastMallocMemShare;
    allocator->mem_is_span = gstAllocatorFastMallocMemIsSpan;
}

static void gst_allocator_fast_malloc_class_init(GstAllocatorFastMallocClass* klass)
{
    auto gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = gstAllocatorFastMallocConstructed;

    auto gstAllocatorClass = GST_ALLOCATOR_CLASS(klass);
    gstAllocatorClass->alloc = gstAllocatorFastMallocAlloc;
    gstAllocatorClass->free = gstAllocatorFastMallocFree;
}
