/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "ReadableStreamBYOBRequest.h"

#include "JSReadableStreamBYOBRequest.h"
#include "ReadableByteStreamController.h"
#include "ReadableStream.h"
#include <JavaScriptCore/ArrayBufferView.h>

namespace WebCore {

Ref<ReadableStreamBYOBRequest> ReadableStreamBYOBRequest::create()
{
    return adoptRef(*new ReadableStreamBYOBRequest);
}

ReadableStreamBYOBRequest::ReadableStreamBYOBRequest() = default;

JSC::ArrayBufferView* ReadableStreamBYOBRequest::view() const
{
    return m_view.get();
}

// https://streams.spec.whatwg.org/#rs-byob-request-respond
ExceptionOr<void> ReadableStreamBYOBRequest::respond(JSDOMGlobalObject& globalObject, size_t bytesWritten)
{
    RefPtr controller = m_controller.get();
    if (!controller)
        return Exception { ExceptionCode::TypeError, "controller is undefined"_s };

    RefPtr view = m_view;
    if (!view || view->isDetached())
        return Exception { ExceptionCode::TypeError, "buffer is detached"_s };

    ASSERT(view->byteLength() > 0);
    ASSERT(view->possiblySharedBuffer()->byteLength() > 0);
    return controller->respond(globalObject, bytesWritten);
}

// https://streams.spec.whatwg.org/#rs-byob-request-respond-with-new-view
ExceptionOr<void> ReadableStreamBYOBRequest::respondWithNewView(JSDOMGlobalObject& globalObject, JSC::ArrayBufferView& view)
{
    RefPtr controller = m_controller.get();
    if (!controller)
        return Exception { ExceptionCode::TypeError, "controller is undefined"_s };
    if (view.isDetached())
        return Exception { ExceptionCode::TypeError, "buffer is detached"_s };
    return controller->respondWithNewView(globalObject, view);
}

void ReadableStreamBYOBRequest::setController(ReadableByteStreamController* controller)
{
    m_controller = controller;
}

void ReadableStreamBYOBRequest::setView(JSC::ArrayBufferView* view)
{
    m_view = view;
}

template<typename Visitor>
void ReadableStreamBYOBRequest::visitAdditionalChildren(Visitor& visitor)
{
    if (m_controller)
        SUPPRESS_UNCOUNTED_ARG m_controller->stream().visitAdditionalChildren(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(ReadableStreamBYOBRequest);

template<typename Visitor>
void JSReadableStreamBYOBRequest::visitAdditionalChildren(Visitor& visitor)
{
    // Do not ref `wrapped()` here since this function may get called on a GC thread.
    SUPPRESS_UNCOUNTED_ARG wrapped().visitAdditionalChildren(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSReadableStreamBYOBRequest);

} // namespace WebCore
