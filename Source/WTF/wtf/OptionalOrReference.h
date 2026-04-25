/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <wtf/StdLibExtras.h>

namespace WTF {

// OptionalOrReference<T> is just like const T&, except that it can also optionally hold T.
//
// When T has an efficient default constructor, use ValueOrReference<T>; otherwise use OptionalOrReference<T>.
//
// OptionalOrReference<T> is an optimization when you need to return a value that is
// usually an existing reference, but sometimes a temporary, e.g.:
//
// OptionalOrReference<CSSParserContext> context()
// {
//     if (canUseCachedContext()) [[likely]]
//         return cachedContext(); // existing reference -- OptionalOrReference<T> avoids a copy
//     return makeContext(); // temporary -- OptionalOrReference<T> holds T
// }

template<typename T> class OptionalOrReference {
public:
    OptionalOrReference(OptionalOrReference&& other)
        : m_value(WTF::move(other.m_value))
        , m_reference(m_value.has_value() ? m_value.value() : other.m_reference)
    {
    }

    OptionalOrReference(const T& reference LIFETIME_BOUND)
        : m_reference(reference)
    {
    }

    OptionalOrReference(T&& temporary)
        : m_value(WTF::move(temporary))
        , m_reference(m_value.value())
    {
    }

    operator const T&() const LIFETIME_BOUND { return m_reference; }
    const T& get() const LIFETIME_BOUND { return m_reference; }
    const T* operator->() const LIFETIME_BOUND { return &m_reference; }

private:
    std::optional<T> m_value;
    const T& m_reference;
};

} // namespace WTF

using WTF::OptionalOrReference;
