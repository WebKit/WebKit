/*
 * Copyright (C) 2021-2024 Apple Inc. All rights reserved.
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

#include "ExceptionOr.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <JavaScriptCore/Uint8Array.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class Blob;
class JSDOMGlobalObject;
class ScriptExecutionContext;

class PushMessageData final : public RefCounted<PushMessageData> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(PushMessageData);
public:
    static Ref<PushMessageData> create(Vector<uint8_t>&& data) { return adoptRef(*new PushMessageData(WTFMove(data))); }

    ExceptionOr<Ref<JSC::ArrayBuffer>> arrayBuffer();
    Ref<Blob> blob(ScriptExecutionContext&);
    ExceptionOr<Ref<JSC::Uint8Array>> bytes();
    ExceptionOr<JSC::JSValue> json(JSDOMGlobalObject&);
    String text();

private:
    explicit PushMessageData(Vector<uint8_t>&&);

    Vector<uint8_t> m_data;
};

inline PushMessageData::PushMessageData(Vector<uint8_t>&& data)
    : m_data(WTFMove(data))
{
}

} // namespace WebCore
