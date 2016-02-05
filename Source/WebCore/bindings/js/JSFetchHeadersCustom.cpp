/*
 * Copyright (C) 2016 Canon Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSFetchHeaders.h"

#if ENABLE(FETCH_API)

#include "JSKeyValueIterator.h"

namespace WebCore {

// FIXME: Move this code to JSFetchHeaders.
using FetchHeadersIterator = JSKeyValueIterator<JSFetchHeaders>;
using FetchHeadersIteratorPrototype = JSKeyValueIteratorPrototype<JSFetchHeaders>;

template<>
const JSC::ClassInfo FetchHeadersIterator::s_info = { "Headers Iterator", &Base::s_info, 0, CREATE_METHOD_TABLE(FetchHeadersIterator) };

template<>
const JSC::ClassInfo FetchHeadersIteratorPrototype::s_info = { "Headers Iterator", &Base::s_info, 0, CREATE_METHOD_TABLE(FetchHeadersIteratorPrototype) };

JSC::JSValue JSFetchHeaders::entries(JSC::ExecState&)
{
    return createIterator<JSFetchHeaders>(*globalObject(), *this, IterationKind::KeyValue);
}

JSC::JSValue JSFetchHeaders::keys(JSC::ExecState&)
{
    return createIterator<JSFetchHeaders>(*globalObject(), *this, IterationKind::Key);
}

JSC::JSValue JSFetchHeaders::values(JSC::ExecState&)
{
    return createIterator<JSFetchHeaders>(*globalObject(), *this, IterationKind::Value);
}

}

#endif
