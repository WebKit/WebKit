/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

#include <wtf/HashTraits.h>
#include <wtf/OptionSet.h>

namespace WTF {

template<typename T, ConcurrencyTag concurrency>
struct DefaultHash<OptionSet<T, concurrency>> {
    static unsigned hash(OptionSet<T, concurrency> key)
    {
        return IntHash<typename OptionSet<T, concurrency>::StorageType>::hash(key.toRaw());
    }

    static bool equal(OptionSet<T, concurrency> a, OptionSet<T, concurrency> b)
    {
        return a == b;
    }

    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<typename T, ConcurrencyTag concurrency>
struct HashTraits<OptionSet<T, concurrency>> : GenericHashTraits<OptionSet<T, concurrency>> {
    using StorageTraits = UnsignedWithZeroKeyHashTraits<typename OptionSet<T, concurrency>::StorageType>;

    static OptionSet<T, concurrency> emptyValue()
    {
        return OptionSet<T, concurrency>::fromRaw(StorageTraits::emptyValue());
    }

    static void constructDeletedValue(OptionSet<T, concurrency>& slot)
    {
        typename OptionSet<T, concurrency>::StorageType storage;
        StorageTraits::constructDeletedValue(storage);
        slot = OptionSet<T, concurrency>::fromRaw(storage);
    }

    static bool isDeletedValue(OptionSet<T, concurrency> value)
    {
        return StorageTraits::isDeletedValue(value.toRaw());
    }
};

} // namespace WTF
