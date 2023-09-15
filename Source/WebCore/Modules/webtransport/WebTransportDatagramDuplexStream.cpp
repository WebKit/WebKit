/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WebTransportDatagramDuplexStream.h"

#include "ReadableStream.h"
#include "WritableStream.h"

namespace WebCore {

Ref<WebTransportDatagramDuplexStream> WebTransportDatagramDuplexStream::create()
{
    return adoptRef(*new WebTransportDatagramDuplexStream());
}

ExceptionOr<Ref<ReadableStream>> WebTransportDatagramDuplexStream::readable(JSC::JSGlobalObject& globalObject)
{
    return ReadableStream::create(globalObject, std::nullopt, std::nullopt);
}

ExceptionOr<Ref<WritableStream>> WebTransportDatagramDuplexStream::writable(JSC::JSGlobalObject& globalObject)
{
    return WritableStream::create(globalObject, std::nullopt, std::nullopt);
}

unsigned WebTransportDatagramDuplexStream::maxDatagramSize()
{
    return 0;
}

double WebTransportDatagramDuplexStream::incomingMaxAge()
{
    return 0;
}

double WebTransportDatagramDuplexStream::outgoingMaxAge()
{
    return 0;
}

double WebTransportDatagramDuplexStream::incomingHighWaterMark()
{
    return 0;
}

double WebTransportDatagramDuplexStream::outgoingHighWaterMark()
{
    return 0;
}

void WebTransportDatagramDuplexStream::setIncomingMaxAge(double)
{
}

void WebTransportDatagramDuplexStream::setOutgoingMaxAge(double)
{
}

void WebTransportDatagramDuplexStream::setIncomingHighWaterMark(double)
{
}

void WebTransportDatagramDuplexStream::setOutgoingHighWaterMark(double)
{
}

}
