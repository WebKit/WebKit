/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "config.h"

#include <wtf/Bag.h>
#include <wtf/Platform.h>
#include <wtf/RefCountedArray.h>
#include <wtf/RefPtr.h>

#if OS(DARWIN)
#include <mach/vm_param.h>
#include <mach/vm_types.h>
#endif

namespace WTF {

namespace {
struct DummyStruct { };
}

static_assert(sizeof(Bag<DummyStruct>) == sizeof(void*), "");

static_assert(sizeof(Ref<DummyStruct>) == sizeof(DummyStruct*), "");

static_assert(sizeof(RefPtr<DummyStruct>) == sizeof(DummyStruct*), "");

static_assert(sizeof(RefCountedArray<DummyStruct>) == sizeof(void*), "");

#if OS(DARWIN) && CPU(ADDRESS64)
static_assert(MACH_VM_MAX_ADDRESS <= ((1ULL << OS_CONSTANT(EFFECTIVE_ADDRESS_WIDTH)) - 1));
#endif

} // namespace WTF

