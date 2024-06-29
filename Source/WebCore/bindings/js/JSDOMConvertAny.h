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
#include "JSValueInWrappedObject.h"

namespace WebCore {

template<> struct Converter<IDLAny> : DefaultConverter<IDLAny> {
    static constexpr bool conversionHasSideEffects = false;

    static ConversionResult<IDLAny> convert(JSC::JSGlobalObject&, JSC::JSValue value)
    {
        return value;
    }

    static ConversionResult<IDLAny> convert(const JSC::Strong<JSC::Unknown>& value)
    {
        return value.get();
    }
};

template<> struct JSConverter<IDLAny> {
    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(const JSC::JSValue& value)
    {
        return value;
    }

    static JSC::JSValue convert(const JSC::Strong<JSC::Unknown>& value)
    {
        return value.get();
    }

    static JSC::JSValue convert(const JSValueInWrappedObject& value)
    {
        return value.getValue();
    }
};

template<> struct VariadicConverter<IDLAny> {
    using Item = JSC::Strong<JSC::Unknown>;

    static std::optional<Item> convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return Item { JSC::getVM(&lexicalGlobalObject), value };
    }
};

} // namespace WebCore
