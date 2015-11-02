/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSDOMTokenList.h"

#include "JSDOMBinding.h"
#include <runtime/Error.h>
#include <wtf/Optional.h>

using namespace JSC;

namespace WebCore {

JSValue JSDOMTokenList::toggle(ExecState& state)
{
    if (UNLIKELY(state.argumentCount() < 1))
        return state.vm().throwException(&state, createNotEnoughArgumentsError(&state));

    ExceptionCode ec = 0;
    String token = state.argument(0).toString(&state)->value(&state);
    if (UNLIKELY(state.hadException()))
        return jsUndefined();

    // toggle() needs to be able to distinguish undefined/missing from the false value for the 'force' parameter.
    Optional<bool> force = state.argument(1).isUndefined() ? Nullopt : Optional<bool>(state.uncheckedArgument(1).toBoolean(&state));
    if (UNLIKELY(state.hadException()))
        return jsUndefined();
    JSValue result = jsBoolean(wrapped().toggle(token, force, ec));

    setDOMException(&state, ec);
    return result;
}


}
