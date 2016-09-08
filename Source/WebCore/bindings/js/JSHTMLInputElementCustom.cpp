/*
 * Copyright (C) 2008, 2016 Apple Inc. All rights reserved.
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
 
#include "config.h"
#include "JSHTMLInputElement.h"

#include "HTMLInputElement.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

JSValue JSHTMLInputElement::selectionStart(ExecState& state) const
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    HTMLInputElement& input = wrapped();
    if (!input.canHaveSelection())
        return throwTypeError(&state, scope);

    return jsNumber(input.selectionStart());
}

void JSHTMLInputElement::setSelectionStart(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    HTMLInputElement& input = wrapped();
    if (!input.canHaveSelection())
        throwTypeError(&state, scope);

    input.setSelectionStart(value.toInt32(&state));
}

JSValue JSHTMLInputElement::selectionEnd(ExecState& state) const
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    HTMLInputElement& input = wrapped();
    if (!input.canHaveSelection())
        return throwTypeError(&state, scope);

    return jsNumber(input.selectionEnd());
}

void JSHTMLInputElement::setSelectionEnd(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    HTMLInputElement& input = wrapped();
    if (!input.canHaveSelection())
        throwTypeError(&state, scope);

    input.setSelectionEnd(value.toInt32(&state));
}

JSValue JSHTMLInputElement::selectionDirection(ExecState& state) const
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    HTMLInputElement& input = wrapped();
    if (!input.canHaveSelection())
        return throwTypeError(&state, scope);

    return jsStringWithCache(&state, input.selectionDirection());
}

void JSHTMLInputElement::setSelectionDirection(ExecState& state, JSValue value)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    HTMLInputElement& input = wrapped();
    if (!input.canHaveSelection()) {
        throwTypeError(&state, scope);
        return;
    }

    input.setSelectionDirection(value.toString(&state)->value(&state));
}

JSValue JSHTMLInputElement::setSelectionRange(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 2))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    HTMLInputElement& input = wrapped();
    if (!input.canHaveSelection())
        return throwTypeError(&state, scope);

    int start = state.uncheckedArgument(0).toInt32(&state);
    int end = state.uncheckedArgument(1).toInt32(&state);
    String direction = state.argument(2).toWTFString(&state);

    input.setSelectionRange(start, end, direction);
    return jsUndefined();
}

} // namespace WebCore
