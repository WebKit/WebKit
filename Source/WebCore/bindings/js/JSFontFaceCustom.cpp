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

#include "config.h"
#include "JSFontFace.h"

#include "CSSFontSelector.h"
#include "CSSValue.h"
#include "CSSValueList.h"
#include "Dictionary.h"
#include "ExceptionCode.h"
#include "JSDOMConstructor.h"

namespace WebCore {

JSC::JSValue JSFontFace::loaded(JSC::ExecState& state) const
{
    if (!m_loaded) {
        if (!wrapped().promise()) {
            DeferredWrapper promise(&state, globalObject(), JSC::JSPromiseDeferred::create(&state, globalObject()));
            m_loaded.set(state.vm(), this, promise.promise());
            wrapped().registerLoaded(WTFMove(promise));
        } else
            m_loaded.set(state.vm(), this, wrapped().promise().value().deferredWrapper().promise());
    }
    return m_loaded.get();
}

JSC::JSValue JSFontFace::load(JSC::ExecState& state)
{
    wrapped().load();
    return loaded(state);
}

}
