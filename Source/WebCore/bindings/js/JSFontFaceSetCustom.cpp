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
#include "JSFontFaceSet.h"

#include "FontFace.h"
#include "JSFontFace.h"
#include "JSKeyValueIterator.h"

namespace WebCore {

using FontFaceSetIterator = JSKeyValueIterator<JSFontFaceSet>;
using FontFaceSetIteratorPrototype = JSKeyValueIteratorPrototype<JSFontFaceSet>;

template<>
const JSC::ClassInfo FontFaceSetIterator::s_info = { "Font Face Set Iterator", &Base::s_info, 0, CREATE_METHOD_TABLE(FontFaceSetIterator) };

template<>
const JSC::ClassInfo FontFaceSetIteratorPrototype::s_info = { "Font Face Set Iterator Prototype", &Base::s_info, 0, CREATE_METHOD_TABLE(FontFaceSetIteratorPrototype) };

JSC::JSValue JSFontFaceSet::ready(JSC::ExecState& execState) const
{
    auto& promise = wrapped().promise(execState);
    return promise.deferred().promise();
}

JSC::JSValue JSFontFaceSet::entries(JSC::ExecState&)
{
    return createIterator<JSFontFaceSet>(*globalObject(), *this, IterationKind::KeyValue);
}

JSC::JSValue JSFontFaceSet::keys(JSC::ExecState&)
{
    return createIterator<JSFontFaceSet>(*globalObject(), *this, IterationKind::Key);
}

JSC::JSValue JSFontFaceSet::values(JSC::ExecState&)
{
    return createIterator<JSFontFaceSet>(*globalObject(), *this, IterationKind::Value);
}

}
