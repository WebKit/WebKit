/*

Copyright (C) 2016 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#pragma once

#include "JSDOMBinding.h"

namespace WebCore {

template<typename T> T build(JSC::ExecState&, JSC::JSValue);
template<typename T, unsigned characterCount> T build(JSC::ExecState&, JSC::JSValue, const char (&propertyName)[characterCount]);
enum class ShouldAllowNonFinite { No, Yes };
template<typename T> T build(JSC::ExecState&, JSC::JSValue, ShouldAllowNonFinite);

template<typename T, unsigned characterCount> inline T build(JSC::ExecState& state, JSC::JSValue value, const char (&propertyName)[characterCount])
{
    return build<T>(state, value.get(&state, JSC::Identifier::fromString(&state, propertyName)));
}

template<typename T> inline T build(JSC::ExecState& state, JSC::JSValue value, ShouldAllowNonFinite allowNonFinite)
{
    static_assert(std::is_same<T, float>::value || std::is_same<T, double>::value, "Should be called with converting to float or double");
    double number = value.toNumber(&state);
    if (allowNonFinite == ShouldAllowNonFinite::No && UNLIKELY(!std::isfinite(number)))
        throwNonFiniteTypeError(state);
    return static_cast<T>(number);
}

}
