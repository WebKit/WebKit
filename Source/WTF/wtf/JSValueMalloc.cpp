/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include <wtf/JSValueMalloc.h>

#include <wtf/DataLog.h>
#include <wtf/RawPointer.h>

#if !(defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC)
#include <bmalloc/bmalloc.h>
#endif

namespace WTF {

#if defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC
void* tryJSValueMalloc(size_t size)
{
    return FastMalloc::tryMalloc(size);
}

void* jsValueMalloc(size_t size)
{
    return fastMalloc(size);
}

void* jsValueRealloc(void* p, size_t size)
{
    return fastRealloc(p, size);
}

void jsValueFree(void* p)
{
    return fastFree(p);
}
#else
void* tryJSValueMalloc(size_t size)
{
    return bmalloc::api::tryMalloc(size, bmalloc::HeapKind::JSValueGigacage);
}

void* jsValueMalloc(size_t size)
{
    return bmalloc::api::malloc(size, bmalloc::HeapKind::JSValueGigacage);
}

void* jsValueRealloc(void* p, size_t size)
{
    return bmalloc::api::realloc(p, size, bmalloc::HeapKind::JSValueGigacage);
}

void jsValueFree(void* p)
{
    if (!p)
        return;
    if (UNLIKELY(!Gigacage::isCaged(Gigacage::JSValue, p))) {
        dataLog("Trying to free JSValue that is not caged: ", RawPointer(p), "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    bmalloc::api::free(p, bmalloc::HeapKind::JSValueGigacage);
}
#endif

} // namespace WTF
