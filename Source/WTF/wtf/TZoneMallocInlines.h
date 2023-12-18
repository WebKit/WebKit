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

#define WTF_MAKE_TZONE_ALLOCATED_INLINE(name) WTF_MAKE_FAST_ALLOCATED
#define WTF_MAKE_TZONE_ALLOCATED_IMPL(name) struct WTFIsoMallocSemicolonifier##name { }
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(name, type) struct WTFIsoMallocSemicolonifier##name { }
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_TEMPLATE(name) struct WTFIsoMallocSemicolonifier##name { }
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED_TEMPLATE(name, type) struct WTFIsoMallocSemicolonifier##name { }

#else

#include <bmalloc/TZoneHeapInlines.h>

#define WTF_MAKE_TZONE_ALLOCATED_INLINE(name) MAKE_BTZONE_MALLOCED_INLINE(name)
#define WTF_MAKE_TZONE_ALLOCATED_IMPL(name) MAKE_BTZONE_MALLOCED_IMPL(name)
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(name, type) MAKE_BTZONE_MALLOCED_IMPL_NESTED(name, type)
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_TEMPLATE(name) MAKE_BTZONE_MALLOCED_IMPL_TEMPLATE(name)
#define WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED_TEMPLATE(name, type) MAKE_BTZONE_MALLOCED_IMPL_NESTED_TEMPLATE(name, type)

#endif


