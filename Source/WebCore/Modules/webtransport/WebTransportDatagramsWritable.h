/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#include "WritableStream.h"

namespace WebCore {

class InternalWritableStream;
class ScriptExecutionContext;
class WebTransportSendGroup;
class WebTransport;
struct WebTransportSendOptions;

class WebTransportDatagramsWritable : public WritableStream {
public:
    static ExceptionOr<Ref<WebTransportDatagramsWritable>> create(ScriptExecutionContext&, RefPtr<WebTransport>&&, WebTransportSendOptions&&);
    ~WebTransportDatagramsWritable();

    const RefPtr<WebTransportSendGroup>& NODELETE sendGroup();
    void NODELETE setSendGroup(WebTransportSendGroup*);

    int64_t NODELETE sendOrder();
    void NODELETE setSendOrder(int64_t);

private:
    WebTransportDatagramsWritable(Ref<InternalWritableStream>&&, WebTransportSendOptions&&);
    WritableStream::Type type() const final { return WritableStream::Type::WebTransportDatagrams; }

    RefPtr<WebTransportSendGroup> m_sendGroup;
    int64_t m_sendOrder { 0 };
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WebTransportDatagramsWritable)
static bool isType(const WebCore::WritableStream& stream) { return stream.type() == WebCore::WritableStream::Type::WebTransportDatagrams; }
SPECIALIZE_TYPE_TRAITS_END()
