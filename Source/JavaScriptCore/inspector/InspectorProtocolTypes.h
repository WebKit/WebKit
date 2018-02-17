/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 The Chromium Authors. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/JSONValues.h>

namespace Inspector {

namespace Protocol {

template<typename> struct BindingTraits;

template<JSON::Value::Type type> struct PrimitiveBindingTraits {
    static void assertValueHasExpectedType(JSON::Value* value)
    {
        ASSERT_UNUSED(value, value);
        ASSERT_UNUSED(value, value->type() == type);
    }
};

template<typename T> struct BindingTraits<JSON::ArrayOf<T>> {
    static RefPtr<JSON::ArrayOf<T>> runtimeCast(RefPtr<JSON::Value>&& value)
    {
        ASSERT_ARG(value, value);
        RefPtr<JSON::Array> array;
        bool castSucceeded = value->asArray(array);
        ASSERT_UNUSED(castSucceeded, castSucceeded);
        assertValueHasExpectedType(array.get());
        COMPILE_ASSERT(sizeof(JSON::ArrayOf<T>) == sizeof(JSON::Array), type_cast_problem);
        return static_cast<JSON::ArrayOf<T>*>(static_cast<JSON::ArrayBase*>(array.get()));
    }

    static void assertValueHasExpectedType(JSON::Value* value)
    {
#if ASSERT_DISABLED
        UNUSED_PARAM(value);
#else
        ASSERT_ARG(value, value);
        RefPtr<JSON::Array> array;
        bool castSucceeded = value->asArray(array);
        ASSERT_UNUSED(castSucceeded, castSucceeded);
        for (unsigned i = 0; i < array->length(); i++)
            BindingTraits<T>::assertValueHasExpectedType(array->get(i).get());
#endif // !ASSERT_DISABLED
    }
};

template<> struct BindingTraits<JSON::Value> {
    static void assertValueHasExpectedType(JSON::Value*)
    {
    }
};

template<> struct BindingTraits<JSON::Array> : PrimitiveBindingTraits<JSON::Value::Type::Array> { };
template<> struct BindingTraits<JSON::Object> : PrimitiveBindingTraits<JSON::Value::Type::Object> { };
template<> struct BindingTraits<String> : PrimitiveBindingTraits<JSON::Value::Type::String> { };
template<> struct BindingTraits<bool> : PrimitiveBindingTraits<JSON::Value::Type::Boolean> { };
template<> struct BindingTraits<double> : PrimitiveBindingTraits<JSON::Value::Type::Double> { };
template<> struct BindingTraits<int> : PrimitiveBindingTraits<JSON::Value::Type::Integer> { };

}

using Protocol::BindingTraits;

} // namespace Inspector
