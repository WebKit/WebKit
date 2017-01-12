/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
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
#include "JSDOMStringMap.h"

#include "CustomElementReactionQueue.h"
#include "DOMStringMap.h"
#include "JSDOMConvert.h"
#include "JSNode.h"
#include <runtime/IdentifierInlines.h>
#include <wtf/text/AtomicString.h>

using namespace JSC;

namespace WebCore {

bool JSDOMStringMap::deleteProperty(JSCell* cell, ExecState* state, PropertyName propertyName)
{
    CustomElementReactionStack customElementReactionStack;
    if (propertyName.isSymbol())
        return Base::deleteProperty(cell, state, propertyName);
    return jsCast<JSDOMStringMap*>(cell)->wrapped().deleteItem(propertyNameToString(propertyName));
}

bool JSDOMStringMap::deletePropertyByIndex(JSCell* cell, ExecState* state, unsigned index)
{
    return deleteProperty(cell, state, Identifier::from(state, index));
}

bool JSDOMStringMap::putDelegate(ExecState* state, PropertyName propertyName, JSValue value, PutPropertySlot&, bool& putResult)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (propertyName.isSymbol())
        return false;

    CustomElementReactionStack customElementReactionStack;

    String stringValue = value.toWTFString(state);
    RETURN_IF_EXCEPTION(scope, true);

    auto result = wrapped().setItem(propertyNameToString(propertyName), WTFMove(stringValue));
    if (result.hasException()) {
        propagateException(*state, scope, result.releaseException());
        return true;
    }

    putResult = true;
    return true;
}

} // namespace WebCore
