/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY CANON INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CANON INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ExceptionOr.h"
#include "JSDOMBinding.h"
#include "JSDOMConvert.h"
#include "JSDOMGuardedObject.h"
#include "JSWritableStream.h"

namespace WebCore {

class WritableStreamSink;

class WritableStream final : public DOMGuarded<JSWritableStream> {
public:
    static Ref<WritableStream> create(JSDOMGlobalObject& globalObject, JSWritableStream& jsStream) { return adoptRef(*new WritableStream(globalObject, jsStream)); }

    static ExceptionOr<Ref<WritableStream>> create(JSC::JSGlobalObject&, RefPtr<WritableStreamSink>&&);
    JSWritableStream* writableStream() const { return guarded(); }

    void lock();
    bool isLocked() const;

private:
    WritableStream(JSDOMGlobalObject&, JSWritableStream&);
};

struct JSWritableStreamWrapperConverter {
    static RefPtr<WritableStream> toWrapped(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        JSC::VM& vm = JSC::getVM(&lexicalGlobalObject);
        auto* globalObject = JSC::jsDynamicCast<JSDOMGlobalObject*>(vm, &lexicalGlobalObject);
        if (!globalObject)
            return nullptr;

        auto* WritableStream = JSC::jsDynamicCast<JSWritableStream*>(vm, value);
        if (!WritableStream)
            return nullptr;

        return WritableStream::create(*globalObject, *WritableStream);
    }
};

template<> struct JSDOMWrapperConverterTraits<WritableStream> {
    using WrapperClass = JSWritableStreamWrapperConverter;
    using ToWrappedReturnType = RefPtr<WritableStream>;
    static constexpr bool needsState = true;
};

inline WritableStream::WritableStream(JSDOMGlobalObject& globalObject, JSWritableStream& WritableStream)
    : DOMGuarded<JSWritableStream>(globalObject, WritableStream)
{
}

inline JSC::JSValue toJS(JSC::JSGlobalObject*, JSC::JSGlobalObject*, WritableStream* stream)
{
    return stream ? stream->writableStream() : JSC::jsUndefined();
}

inline JSC::JSValue toJSNewlyCreated(JSC::JSGlobalObject*, JSDOMGlobalObject*, Ref<WritableStream>&& stream)
{
    return stream->writableStream();
}

}
