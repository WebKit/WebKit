/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "PushEvent.h"

#include "PushMessageData.h"
#include <JavaScriptCore/JSArrayBuffer.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSCInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(PushEvent);

static Vector<uint8_t> dataFromPushMessageDataInit(PushMessageDataInit& data)
{
    return WTF::switchOn(data, [](RefPtr<JSC::ArrayBuffer>& value) -> Vector<uint8_t> {
        if (!value)
            return { };
        return { reinterpret_cast<const uint8_t*>(value->data()), value->byteLength() };
    }, [](RefPtr<JSC::ArrayBufferView>& value) -> Vector<uint8_t> {
        if (!value)
            return { };
        return { reinterpret_cast<const uint8_t*>(value->baseAddress()), value->byteLength() };
    }, [](String& value) -> Vector<uint8_t> {
        auto utf8 = value.utf8();
        return Vector<uint8_t> { reinterpret_cast<const uint8_t*>(utf8.data()), utf8.length() };
    });
}

Ref<PushEvent> PushEvent::create(const AtomString& type, PushEventInit&& initializer, IsTrusted isTrusted)
{
    std::optional<Vector<uint8_t>> data;
    if (initializer.data)
        data = dataFromPushMessageDataInit(*initializer.data);
    return create(type, WTFMove(initializer), WTFMove(data), isTrusted);
}

Ref<PushEvent> PushEvent::create(const AtomString& type, ExtendableEventInit&& initializer, std::optional<Vector<uint8_t>>&& data, IsTrusted isTrusted)
{
    return adoptRef(*new PushEvent(type, WTFMove(initializer), WTFMove(data), isTrusted));
}

static inline RefPtr<PushMessageData> pushMessageDataFromOptionalVector(std::optional<Vector<uint8_t>>&& data)
{
    if (!data)
        return nullptr;
    return PushMessageData::create(WTFMove(*data));
}

PushEvent::PushEvent(const AtomString& type, ExtendableEventInit&& eventInit, std::optional<Vector<uint8_t>>&& data, IsTrusted isTrusted)
    : ExtendableEvent(type, WTFMove(eventInit), isTrusted)
    , m_data(pushMessageDataFromOptionalVector(WTFMove(data)))
{
}

PushEvent::~PushEvent()
{
}

} // namespace WebCore
