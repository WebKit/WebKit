/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if USE(SYSTEM_MALLOC) || !USE(TZONE_MALLOC)

#include <wtf/FastMalloc.h>

#define WTF_MAKE_TZONE_ALLOCATED_INLINE(typeName) WTF_MAKE_FAST_ALLOCATED
#define WTF_MAKE_TZONE_ALLOCATED_IMPL(typeName) struct WTFTzoneMallocSemicolonifier##typeName { }
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(typeName, type) struct WTFTzoneMallocSemicolonifier##typeName { }
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_TEMPLATE(typeName) struct WTFTzoneMallocSemicolonifier##typeName { }
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED_TEMPLATE(typeName, type) struct WTFTzoneMallocSemicolonifier##typeName { }

#define WTF_MAKE_COMPACT_TZONE_ALLOCATED_INLINE(typeName) WTF_MAKE_FAST_COMPACT_ALLOCATED
#define WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(typeName) struct WTFTzoneMallocSemicolonifier##typeName { }
#define WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL_NESTED(typeName, type) struct WTFTzoneMallocSemicolonifier##typeName { }
#define WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL_TEMPLATE(typeName) struct WTFTzoneMallocSemicolonifier##typeName { }
#define WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL_NESTED_TEMPLATE(typeName, type) struct WTFTzoneMallocSemicolonifier##typeName { }

#if USE(SYSTEM_MALLOC) || !USE(ISO_MALLOC)

#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_INLINE(name) WTF_MAKE_FAST_ALLOCATED
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(name) struct WTFIsoMallocSemicolonifier##name { }
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL_TEMPLATE(name) struct WTFIsoMallocSemicolonifier##name { }
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED_INLINE(name) WTF_MAKE_FAST_COMPACT_ALLOCATED
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED_IMPL(name) struct WTFIsoMallocSemicolonifier##name { }
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED_IMPL_TEMPLATE(name) struct WTFIsoMallocSemicolonifier##name { }

#else // !USE(SYSTEM_MALLOC) && USE(ISO_MALLOC) && !USE(TZONE_MALLOC)

#include <bmalloc/IsoHeapInlines.h>

#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_INLINE(name) MAKE_BISO_MALLOCED_INLINE(name, IsoHeap)
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(name) MAKE_BISO_MALLOCED_IMPL(name, IsoHeap)
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL_TEMPLATE(name) MAKE_BISO_MALLOCED_IMPL_TEMPLATE(name, IsoHeap)
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED_INLINE(name) MAKE_BISO_MALLOCED_INLINE(name, CompactIsoHeap)
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED_IMPL(name) MAKE_BISO_MALLOCED_IMPL(name, CompactIsoHeap)
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED_IMPL_TEMPLATE(name) MAKE_BISO_MALLOCED_IMPL_TEMPLATE(name, IsoHeap)

#endif

#else // !USE(SYSTEM_MALLOC) && USE(TZONE_MALLOC)

#include <bmalloc/TZoneHeapInlines.h>

#if !BUSE(TZONE)
#error "TZones enabled in WTF, but not enabled in bmalloc"
#endif

#define WTF_MAKE_TZONE_ALLOCATED_INLINE(typeName) MAKE_BTZONE_MALLOCED_INLINE(typeName, TZoneHeap)
#define WTF_MAKE_TZONE_ALLOCATED_IMPL(type) MAKE_BTZONE_MALLOCED_IMPL(type, TZoneHeap)
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(typeName, type) MAKE_BTZONE_MALLOCED_IMPL_NESTED(typeName, type, TZoneHeap)
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_TEMPLATE(typeName) MAKE_BTZONE_MALLOCED_IMPL_TEMPLATE(typeName, TZoneHeap)
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED_TEMPLATE(typeName, type) MAKE_BTZONE_MALLOCED_IMPL_NESTED_TEMPLATE(typeName, type, TZoneHeap)

#define WTF_MAKE_COMPACT_TZONE_ALLOCATED_INLINE(typeName) MAKE_BTZONE_MALLOCED_INLINE(typeName, CompactTZoneHeap)
#define WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(type) MAKE_BTZONE_MALLOCED_IMPL(type, CompactTZoneHeap)
#define WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL_NESTED(typeName, type) MAKE_BTZONE_MALLOCED_IMPL_NESTED(typeName, type, CompactTZoneHeap)
#define WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL_TEMPLATE(typeName) MAKE_BTZONE_MALLOCED_IMPL_TEMPLATE(typeName, CompactTZoneHeap)
#define WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL_NESTED_TEMPLATE(typeName, type) MAKE_BTZONE_MALLOCED_IMPL_NESTED_TEMPLATE(typeName, type, CompactTZoneHeap)

#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_INLINE(name) MAKE_BTZONE_MALLOCED_INLINE(name, TZoneHeap)
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(name) MAKE_BTZONE_MALLOCED_IMPL(name, TZoneHeap)
#define WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL_TEMPLATE(name) MAKE_BTZONE_MALLOCED_IMPL_TEMPLATE(name, TZoneHeap)
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED_INLINE(name) MAKE_BTZONE_MALLOCED_INLINE(name, CompactTZoneHeap)
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED_IMPL(name) MAKE_BTZONE_MALLOCED_IMPL(name, CompactTZoneHeap)
#define WTF_MAKE_COMPACT_TZONE_OR_ISO_ALLOCATED_IMPL_TEMPLATE(name) MAKE_BTZONE_MALLOCED_IMPL_NESTED_TEMPLATE(name, CompactTZoneHeap)

#endif
