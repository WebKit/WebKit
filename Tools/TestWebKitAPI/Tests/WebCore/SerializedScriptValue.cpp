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

#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <WebCore/IDBBindingUtilities.h>
#include <WebCore/IDBValue.h>
#include <WebCore/ParsedContentRange.h>
#include <WebCore/ParsedRequestRange.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ThreadSafeDataBuffer.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace TestWebKitAPI {

TEST(WebCore, SerializedScriptValueReadRTCCertificate)
{
    WTF::initialize();
    WTF::initializeMainThread();
    JSC::initialize();
    WebCore::Process::identifier();

    std::array<uint8_t, 149> bytes {
        0x0d, 0x00, 0x00, 0x00, 0x2c, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0xe7, 0x1b, 0x86, 0xc8, 0xd0, 0xdb, 0x71, 0x6a, 0xac, 0x80, 0xf4,
        0x6f, 0x76, 0xc0, 0x35, 0x5c, 0xc8, 0xc0, 0x4e, 0x49, 0xd8, 0xc4, 0x6d,
        0x9c, 0x4e, 0x3f, 0xbd, 0x9e, 0xdc, 0xe5, 0xcd, 0x81, 0x5e, 0x71, 0x14,
        0xb3, 0x1a, 0xad, 0xa3, 0xf5, 0xcf, 0x1f, 0x82, 0x58, 0xd8, 0xca, 0xbe,
        0xb1, 0x2c, 0xd9, 0xef, 0xde, 0x05, 0x26, 0xef, 0xdc, 0x21, 0x4f, 0x3d,
        0x20, 0x00, 0x32, 0x4b, 0xd8, 0x85, 0x86, 0xf4, 0x68, 0xb7, 0x1f, 0x71,
        0xa7, 0xd8, 0x5e, 0x50, 0xcf, 0x9e, 0xc6, 0x91, 0x44, 0x36, 0x41, 0xdc,
        0x54, 0x68, 0xc8, 0xf8, 0x5f, 0x93, 0xb4, 0x07, 0xfa, 0x73, 0xd7, 0x90,
        0xa8, 0x9f, 0x07, 0xd2, 0x50, 0x03, 0x5e, 0x05, 0x77, 0x4a, 0x56, 0xdd,
        0x1e, 0x68, 0xd3, 0x62, 0x8f, 0x58, 0x7e, 0x7c, 0x1e, 0xc6, 0x0f, 0xcc,
        0x01, 0x6e, 0x88, 0x4b, 0x32
    };

    Vector<uint8_t> vector { std::span<uint8_t>(bytes) };
    const WebCore::ThreadSafeDataBuffer value = WebCore::ThreadSafeDataBuffer::create(WTFMove(vector));
    WebCore::callOnIDBSerializationThreadAndWait([&](auto& globalObject) {
        auto jsValue = WebCore::deserializeIDBValueToJSValue(globalObject, value);
        UNUSED_VARIABLE(jsValue);
    });
}

}
