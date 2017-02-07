/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "IDLTypes.h"
#include "JSDOMConvertBase.h"

namespace WebCore {

namespace Detail {

template<typename IDLType>
struct VariadicConverterBase;

template<typename IDLType> 
struct VariadicConverterBase {
    using Item = typename IDLType::ImplementationType;

    static std::optional<Item> convert(JSC::ExecState& state, JSC::JSValue value)
    {
        auto& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto result = Converter<IDLType>::convert(state, value);
        RETURN_IF_EXCEPTION(scope, std::nullopt);

        return WTFMove(result);
    }
};

template<typename T>
struct VariadicConverterBase<IDLInterface<T>> {
    using Item = std::reference_wrapper<T>;

    static std::optional<Item> convert(JSC::ExecState& state, JSC::JSValue value)
    {
        auto* result = Converter<IDLInterface<T>>::convert(state, value);
        if (!result)
            return std::nullopt;
        return std::optional<Item>(*result);
    }
};

template<typename IDLType>
struct VariadicConverter : VariadicConverterBase<IDLType> {
    using Item = typename VariadicConverterBase<IDLType>::Item;
    using Container = Vector<Item>;

    struct Result {
        size_t argumentIndex;
        std::optional<Container> arguments;
    };
};

}

template<typename IDLType> typename Detail::VariadicConverter<IDLType>::Result convertVariadicArguments(JSC::ExecState& state, size_t startIndex)
{
    size_t length = state.argumentCount();
    if (startIndex > length)
        return { 0, std::nullopt };

    typename Detail::VariadicConverter<IDLType>::Container result;
    result.reserveInitialCapacity(length - startIndex);

    for (size_t i = startIndex; i < length; ++i) {
        auto value = Detail::VariadicConverter<IDLType>::convert(state, state.uncheckedArgument(i));
        if (!value)
            return { i, std::nullopt };
        result.uncheckedAppend(WTFMove(*value));
    }

    return { length, WTFMove(result) };
}

} // namespace WebCore
