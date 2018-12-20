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

template<typename IDLType>
struct VariadicConverter {
    using Item = typename IDLType::ImplementationType;

    static Optional<Item> convert(JSC::ExecState& state, JSC::JSValue value)
    {
        auto& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto result = Converter<IDLType>::convert(state, value);
        RETURN_IF_EXCEPTION(scope, WTF::nullopt);

        return WTFMove(result);
    }
};

template<typename IDLType> Vector<typename VariadicConverter<IDLType>::Item> convertVariadicArguments(JSC::ExecState& state, size_t startIndex)
{
    size_t length = state.argumentCount();
    if (startIndex >= length)
        return { };

    Vector<typename VariadicConverter<IDLType>::Item> result;
    result.reserveInitialCapacity(length - startIndex);

    for (size_t i = startIndex; i < length; ++i) {
        auto value = VariadicConverter<IDLType>::convert(state, state.uncheckedArgument(i));
        if (!value)
            return { };
        result.uncheckedAppend(WTFMove(*value));
    }

    return WTFMove(result);
}

} // namespace WebCore
