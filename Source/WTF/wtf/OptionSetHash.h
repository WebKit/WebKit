/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

template<typename T> struct DefaultHash<OptionSet<T>> {
    static unsigned hash(OptionSet<T> key)
    {
        return IntHash<typename OptionSet<T>::StorageType>::hash(key.toRaw());
    }

    static bool equal(OptionSet<T> a, OptionSet<T> b)
    {
        return a == b;
    }

    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<typename T> struct HashTraits<OptionSet<T>> : GenericHashTraits<OptionSet<T>> {
    using StorageTraits = UnsignedWithZeroKeyHashTraits<typename OptionSet<T>::StorageType>;

    static OptionSet<T> emptyValue()
    {
        return OptionSet<T>::fromRaw(StorageTraits::emptyValue());
    }

    static void constructDeletedValue(OptionSet<T>& slot)
    {
        typename OptionSet<T>::StorageType storage;
        StorageTraits::constructDeletedValue(storage);
        slot = OptionSet<T>::fromRaw(storage);
    }

    static bool isDeletedValue(OptionSet<T> value)
    {
        return StorageTraits::isDeletedValue(value.toRaw());
    }
};

} // namespace WTF
