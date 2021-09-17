/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Assertions.h>
#include <wtf/RetainPtr.h>

namespace WTF {

// Use checked_ns_cast<> instead of dynamic_ns_cast<> when a specific NS type is required.

template<typename T> T* checked_ns_cast(id object)
{
    using ValueType = std::remove_pointer_t<T>;
    using PtrType = ValueType*;

    if (!object)
        return nullptr;

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION([object isKindOfClass:[ValueType class]]);

    return reinterpret_cast<PtrType>(object);
}

// Use dynamic_ns_cast<> instead of checked_ns_cast<> when actively checking NS types,
// similar to dynamic_cast<> in C++. Be sure to include a nil check.

template<typename T> T* dynamic_ns_cast(id object)
{
    using ValueType = std::remove_pointer_t<T>;
    using PtrType = ValueType*;

    if (![object isKindOfClass:[ValueType class]])
        return nullptr;

    return reinterpret_cast<PtrType>(object);
}

template<typename T, typename U> RetainPtr<T> dynamic_ns_cast(RetainPtr<U>&& object)
{
    if (![object isKindOfClass:[T class]])
        return nullptr;

    return WTFMove(object);
}

} // namespace WTF

using WTF::checked_ns_cast;
using WTF::dynamic_ns_cast;
