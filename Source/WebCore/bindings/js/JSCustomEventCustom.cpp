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
#include "JSCustomEvent.h"

#include "CustomEvent.h"
#include "DOMWrapperWorld.h"
#include <runtime/JSCInlines.h>
#include <runtime/JSCJSValue.h>
#include <runtime/Structure.h>


namespace WebCore {
using namespace JSC;
    
JSValue JSCustomEvent::detail(ExecState& state) const
{
    auto& event = wrapped();

    auto detail = event.detail();
    if (!detail)
        return jsNull();

    if (detail.isObject() && &worldForDOMObject(detail.getObject()) != &currentWorld(&state)) {
        // We need to make sure CustomEvents do not leak their detail property across isolated DOM worlds.
        // Ideally, we would check that the worlds have different privileges but that's not possible yet.
        auto serializedDetail = event.trySerializeDetail(state);
        if (!serializedDetail)
            return jsNull();
        return serializedDetail->deserialize(state, globalObject());
    }

    return detail;
}

} // namespace WebCore

