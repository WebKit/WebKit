/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/ForbidHeapAllocation.h>
#include <wtf/Platform.h>

#define WTF_NOEXPORT

#if USE(SYSTEM_MALLOC) || !USE(TZONE_MALLOC)

#include <wtf/FastMalloc.h>

// class allocators with FastMalloc fallback if TZoneHeap is disabled.
#define WTF_MAKE_TZONE_ALLOCATED(name) WTF_MAKE_FAST_ALLOCATED
#define WTF_MAKE_TZONE_ALLOCATED_EXPORT(name, exportMacro) WTF_MAKE_FAST_ALLOCATED

// struct allocators with FastMalloc fallback if TZoneHeap is disabled.
#define WTF_MAKE_STRUCT_TZONE_ALLOCATED(name) WTF_MAKE_STRUCT_FAST_ALLOCATED

// template allocators with FastMalloc fallback if TZoneHeap is disabled.
#define WTF_MAKE_TZONE_ALLOCATED_TEMPLATE(name) WTF_MAKE_FAST_ALLOCATED
#define WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_EXPORT(name, exportMacro) WTF_MAKE_FAST_ALLOCATED

// special class (e.g. those used with CompactPtr) allocators with FastMalloc fallback if TZoneHeap is disabled.
#define WTF_MAKE_COMPACT_TZONE_ALLOCATED(name) WTF_MAKE_FAST_COMPACT_ALLOCATED
#define WTF_MAKE_COMPACT_TZONE_ALLOCATED_EXPORT(name, exportMacro) WTF_MAKE_FAST_COMPACT_ALLOCATED

#if USE(SYSTEM_MALLOC) || !USE(ISO_MALLOC)

// class allocators with IsoHeap fallback if TZoneHeap and IsoHeap are disabled.
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED(name) WTF_MAKE_FAST_ALLOCATED
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_EXPORT(name, exportMacro) WTF_MAKE_FAST_ALLOCATED

// template allocators with IsoHeap fallback if TZoneHeap and IsoHeap are disabled.
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_TEMPLATE(name) WTF_MAKE_FAST_ALLOCATED

// class allocators with IsoHeap fallback if TZoneHeap and IsoHeap are disabled.
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED(name) WTF_MAKE_FAST_COMPACT_ALLOCATED
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED_EXPORT(name, exportMacro) WTF_MAKE_FAST_COMPACT_ALLOCATED

// template implementation to go with WTF_MAKE_TZONE_OR_ISO_ALLOCATED_TEMPLATE
// if TZoneHeap and IsoHeap are disabled. This should be added immediately after the
// template definition.
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_TEMPLATE_IMPL(_templateParameters, _type) \
    using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#else // !USE(SYSTEM_MALLOC) && USE(ISO_MALLOC) && !USE(TZONE_MALLOC)

#include <bmalloc/IsoHeap.h>

// class allocators with IsoHeap fallback if TZoneHeap is disabled, but IsoHeap is enabled.
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED(name) MAKE_BISO_MALLOCED(name, IsoHeap, WTF_NOEXPORT)
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_EXPORT(name, exportMacro) MAKE_BISO_MALLOCED(name, IsoHeap, exportMacro)

// template allocators with IsoHeap fallback if TZoneHeap is disabled, but IsoHeap is enabled.
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_TEMPLATE(name) MAKE_BISO_MALLOCED(name, IsoHeap, WTF_NOEXPORT)

// special class (e.g. those used with CompactPtr) allocators with IsoHeap fallback if TZoneHeap is disabled, but IsoHeap is enabled.
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED(name) \
    WTF_ALLOW_COMPACT_POINTERS; \
    MAKE_BISO_MALLOCED(name, CompactIsoHeap, WTF_NOEXPORT)
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED_EXPORT(name, exportMacro) \
    WTF_ALLOW_COMPACT_POINTERS; \
    MAKE_BISO_MALLOCED(name, CompactIsoHeap, exportMacro)

// template implementation to go with WTF_MAKE_TZONE_OR_ISO_ALLOCATED_TEMPLATE
// if TZoneHeap is disabled, but IsoHeap is enabled. This should be added immediately
// after the template definition.
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_TEMPLATE_IMPL(_templateParameters, _type) \
    MAKE_BISO_MALLOCED_TEMPLATE_IMPL(_templateParameters, _type)

#endif // USE(SYSTEM_MALLOC) || !USE(ISO_MALLOC)

// template implementation to go with WTF_MAKE_TZONE_ALLOCATED_TEMPLATE and
// WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_EXPORT if TZoneHeap is disabled. This
// should be added immediately after the template definition.
#define WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_IMPL(_templateParameters, _type) \
    using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#define WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_IMPL_WITH_MULTIPLE_OR_SPECIALIZED_PARAMETERS() \
    using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#else // !USE(SYSTEM_MALLOC) && USE(TZONE_MALLOC)

#include <bmalloc/TZoneHeap.h>

#if !BUSE(TZONE)
#error "TZones enabled in WTF, but not enabled in bmalloc"
#endif

// FastMalloc fallback allocators

// class allocators with FastMalloc fallback if TZoneHeap is enabled.
#define WTF_MAKE_TZONE_ALLOCATED(name) MAKE_BTZONE_MALLOCED(name, NonCompact, WTF_NOEXPORT)
#define WTF_MAKE_TZONE_ALLOCATED_EXPORT(name, exportMacro) MAKE_BTZONE_MALLOCED(name, NonCompact, exportMacro)

// struct allocators with FastMalloc fallback if TZoneHeap is enabled.
#define WTF_MAKE_STRUCT_TZONE_ALLOCATED(name) MAKE_STRUCT_BTZONE_MALLOCED(name, NonCompact, WTF_NOEXPORT)

// template allocators with FastMalloc fallback if TZoneHeap is enabled.
#define WTF_MAKE_TZONE_ALLOCATED_TEMPLATE(name) MAKE_BTZONE_MALLOCED_TEMPLATE(name, NonCompact, WTF_NOEXPORT)
#define WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_EXPORT(name, exportMacro) MAKE_BTZONE_MALLOCED_TEMPLATE(name, NonCompact, exportMacro)

// special class (e.g. those used with CompactPtr) allocators with FastMalloc fallback if TZoneHeap is enabled.
#define WTF_MAKE_COMPACT_TZONE_ALLOCATED(name) MAKE_BTZONE_MALLOCED(name, Compact, WTF_NOEXPORT)
#define WTF_MAKE_COMPACT_TZONE_ALLOCATED_EXPORT(name, exportMacro) MAKE_BTZONE_MALLOCED(name, Compact, exportMacro)

// IsoHeap fallback allocators

// class allocators with IsoHeap fallback if TZoneHeap is enabled.
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED(name) MAKE_BTZONE_MALLOCED(name, NonCompact, WTF_NOEXPORT)
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_EXPORT(name, exportMacro) MAKE_BTZONE_MALLOCED(name, NonCompact, exportMacro)

// template allocators with IsoHeap fallback if TZoneHeap is enabled.
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_TEMPLATE(name) MAKE_BTZONE_MALLOCED_TEMPLATE(name, NonCompact, WTF_NOEXPORT)

// special class (e.g. those used with CompactPtr) allocators with IsoHeap fallback if TZoneHeap is enabled.
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED(name) \
    WTF_ALLOW_COMPACT_POINTERS; \
    MAKE_BTZONE_MALLOCED(name, Compact, WTF_NOEXPORT)
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED_EXPORT(name, exportMacro) \
    WTF_ALLOW_COMPACT_POINTERS; \
    MAKE_BTZONE_MALLOCED(name, Compact, exportMacro)

// Template implementations for instantiating allocator template static / methods

// template implementation to go with WTF_MAKE_TZONE_ALLOCATED_TEMPLATE and
// WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_EXPORT if TZoneHeap is enabled. This
// should be added immediately after the template definition.
#define WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_IMPL(_templateParameters, _type) MAKE_BTZONE_MALLOCED_TEMPLATE_IMPL(_templateParameters, _type)

// template implementation to go with WTF_MAKE_TZONE_OR_ISO_ALLOCATED_TEMPLATE
// if TZoneHeap is enabled. This should be added immediately after the template definition.
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_TEMPLATE_IMPL(_templateParameters, _type) MAKE_BTZONE_MALLOCED_TEMPLATE_IMPL(_templateParameters, _type)

// template implementation for to go with WTF_MAKE_TZONE_ALLOCATED_TEMPLATE and
// WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_EXPORT if TZoneHeap is ensabled. This
// should be added immediately after the template definition. This version is
// needed in order to support templates with multiple parameters (which
// WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_IMPL cannot support).
//
// Requires the client to define these 3 macros:
//     TZONE_TEMPLATE_PARAMS, TZONE_TYPE
#define WTF_MAKE_TZONE_ALLOCATED_TEMPLATE_IMPL_WITH_MULTIPLE_OR_SPECIALIZED_PARAMETERS() \
    MAKE_BTZONE_MALLOCED_TEMPLATE_IMPL_WITH_MULTIPLE_PARAMETERS()

#endif

// Annotation to forbid use with dynamic allocation

// class / struct which should not use dynamic allocation.
#define WTF_MAKE_TZONE_NON_HEAP_ALLOCATABLE(name) WTF_FORBID_HEAP_ALLOCATION

// class / struct which should not use dynamic allocation. These used to be ISO_ALLOCATED.
// FIXME: we should remove this and use WTF_MAKE_TZONE_NON_HEAP_ALLOCATABLE instead.
#define WTF_MAKE_TZONE_OR_ISO_NON_HEAP_ALLOCATABLE(name) WTF_FORBID_HEAP_ALLOCATION
