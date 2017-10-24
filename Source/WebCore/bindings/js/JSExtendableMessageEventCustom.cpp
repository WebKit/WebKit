/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(SERVICE_WORKER)
#include "JSExtendableMessageEvent.h"

namespace WebCore {

using namespace JSC;

JSValue JSExtendableMessageEvent::data(ExecState& state) const
{
    if (JSValue cachedValue = m_data.get()) {
        // We cannot use a cached object if we are in a different world than the one it was created in.
        if (!cachedValue.isObject() || &worldForDOMObject(cachedValue.getObject()) == &currentWorld(&state))
            return cachedValue;
        ASSERT_NOT_REACHED();
    }

    auto& event = wrapped();
    JSValue result;
    if (auto* serializedValue = event.data())
        result = serializedValue->deserialize(state, globalObject(), event.ports(), SerializationErrorMode::NonThrowing);
    else
        result = jsNull();

    // Save the result so we don't have to deserialize the value again.
    m_data.set(state.vm(), this, result);
    return result;
}

}

#endif // ENABLE(SERVICE_WORKER)
