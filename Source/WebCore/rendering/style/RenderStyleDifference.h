/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#if !LOG_DISABLED

#include <wtf/PointerComparison.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

#define LOG_IF_DIFFERENT(name) \
    do { logIfDifferent(ts, ASCIILiteral::fromLiteralUnsafe(#name), name, other.name); } while (0)

#define LOG_IF_DIFFERENT_WITH_CAST(type, name) \
    do { logIfDifferent(ts, ASCIILiteral::fromLiteralUnsafe(#name), static_cast<type>(name), static_cast<type>(other.name)); } while (0)

#define LOG_RAW_OPTIONSET_IF_DIFFERENT(type, name) \
    do { logIfDifferent(ts, ASCIILiteral::fromLiteralUnsafe(#name), OptionSet<type>::fromRaw(name), OptionSet<type>::fromRaw(other.name)); } while (0)


template<class T>
struct is_pointer_wrapper : std::false_type { };

template<class T>
struct is_pointer_wrapper<RefPtr<T>> : std::true_type { };

template<class T>
struct is_pointer_wrapper<Ref<T>> : std::true_type { };

template<class T>
struct is_pointer_wrapper<std::unique_ptr<T>> : std::true_type { };

template<typename T>
struct ValueOrUnstreamableMessage {
    explicit ValueOrUnstreamableMessage(const T& value)
        : value(value)
    { }
    const T& value;
};

template<typename T>
TextStream& operator<<(TextStream& ts, ValueOrUnstreamableMessage<T> item)
{
    if constexpr (WTF::supports_text_stream_insertion<T>::value) {
        if constexpr (is_pointer_wrapper<T>::value)
            ts << ValueOrNull(item.value.get());
        else
            ts << item.value;
    } else
        ts << "(unstreamable)";
    return ts;
}


template<typename T>
void logIfDifferent(TextStream& ts, ASCIILiteral name, const T& item1, const T& item2)
{
    bool differ = false;
    if constexpr (is_pointer_wrapper<T>::value)
        differ = !arePointingToEqualData(item1, item2);
    else
        differ = item1 != item2;

    if (differ)
        ts << name << " differs: " << ValueOrUnstreamableMessage(item1) << ", " << ValueOrUnstreamableMessage(item2) << '\n';
}

} // namespace WebCore

#endif // !LOG_DISABLED
