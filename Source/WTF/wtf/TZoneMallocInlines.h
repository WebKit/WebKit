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

#include <wtf/Platform.h>

#if USE(SYSTEM_MALLOC) || !USE(TZONE_MALLOC)

#include <wtf/FastMalloc.h>

#define WTF_MAKE_TZONE_ALLOCATED_INLINE(typeName) WTF_MAKE_FAST_ALLOCATED
#define WTF_MAKE_TZONE_ALLOCATED_IMPL(typeName) struct WTFIsoMallocSemicolonifier##typeName { }
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(typeName, type) struct WTFIsoMallocSemicolonifier##typeName { }
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_TEMPLATE(typeName) struct WTFIsoMallocSemicolonifier##typeName { }
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED_TEMPLATE(typeName, type) struct WTFIsoMallocSemicolonifier##typeName { }

#else

#include <bmalloc/TZoneHeapInlines.h>

#if !BUSE(TZONE)
#error "TZones enabled in WTF, but not enabled in bmalloc"
#endif

#define WTF_MAKE_TZONE_ALLOCATED_INLINE(typeName) MAKE_BTZONE_MALLOCED_INLINE(typeName)
#define WTF_MAKE_TZONE_ALLOCATED_IMPL(type) MAKE_BTZONE_MALLOCED_IMPL(type)
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(typeName, type) MAKE_BTZONE_MALLOCED_IMPL_NESTED(typeName, type)
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_TEMPLATE(typeName) MAKE_BTZONE_MALLOCED_IMPL_TEMPLATE(typeName)
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED_TEMPLATE(typeName, type) MAKE_BTZONE_MALLOCED_IMPL_NESTED_TEMPLATE(typeName, type)

#endif

#if !USE(WK_TZONE_MALLOC)

#define WTF_MAKE_WK_TZONE_ALLOCATED_INLINE(typeName)WTF_MAKE_FAST_ALLOCATED
#define WTF_MAKE_WK_TZONE_ALLOCATED_IMPL(typeName) struct WTFIsoMallocSemicolonifier##typeName { }
#define WTF_MAKE_WK_TZONE_ALLOCATED_IMPL_NESTED(typeName, type) truct WTFIsoMallocSemicolonifier##typeName { }
#define WTF_MAKE_WK_TZONE_ALLOCATED_IMPL_TEMPLATE(typeName) struct WTFIsoMallocSemicolonifier##typeName { }
#define WTF_MAKE_WK_TZONE_ALLOCATED_IMPL_NESTED_TEMPLATE(typeName, type) struct WTFIsoMallocSemicolonifier##typeName { }

#else

#define WTF_MAKE_WK_TZONE_ALLOCATED_INLINE(typeName) WTF_MAKE_TZONE_ALLOCATED_INLINE(typeName)
#define WTF_MAKE_WK_TZONE_ALLOCATED_IMPL(typeName) WTF_MAKE_TZONE_ALLOCATED_IMPL(typeName)
#define WTF_MAKE_WK_TZONE_ALLOCATED_IMPL_NESTED(typeName, type) WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(typeName, type)
#define WTF_MAKE_WK_TZONE_ALLOCATED_IMPL_TEMPLATE(typeName) WTF_MAKE_TZONE_ALLOCATED_IMPL_TEMPLATE(typeName)
#define WTF_MAKE_WK_TZONE_ALLOCATED_IMPL_NESTED_TEMPLATE(typeName, type) WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED_TEMPLATE(typeName, type)

#endif
