/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_CODECS)

#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/VM.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/WebCodecsBufferTransfer.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/WorkQueue.h>
#include <wtf/threads/BinarySemaphore.h>

namespace TestWebKitAPI {

using namespace WebCore;

static std::array<uint8_t, 6> testBytes { 0xBA, 0xDF, 0x00, 0xD0, 0xBA, 0xDF };

TEST(WebCodecsBufferTransfer, AdoptedContentsAreNotCopied)
{
    auto content = JSC::ArrayBufferContents::fromSpan(std::span { testBytes });
    ASSERT_TRUE(!!content);
    auto* originalData = content->span().data();

    Ref buffer = adoptArrayBufferContents(WTF::move(*content));

    EXPECT_EQ(buffer->span().data(), originalData);
    EXPECT_EQ(buffer->size(), testBytes.size());
    EXPECT_TRUE(equalSpans(buffer->span(), std::span<const uint8_t> { testBytes }));
}

TEST(WebCodecsBufferTransfer, AdoptedContentsOutliveTheirArrayBuffer)
{
    RefPtr arrayBuffer = JSC::ArrayBuffer::tryCreate(std::span { testBytes });
    ASSERT_TRUE(!!arrayBuffer);
    auto* originalData = static_cast<const uint8_t*>(arrayBuffer->data());

    auto content = JSC::ArrayBufferContents::fromSpan(arrayBuffer->span());
    ASSERT_TRUE(!!content);
    Ref buffer = adoptArrayBufferContents(WTF::move(*content));

    arrayBuffer = nullptr;

    EXPECT_NE(buffer->span().data(), originalData);
    EXPECT_TRUE(equalSpans(buffer->span(), std::span<const uint8_t> { testBytes }));
}

TEST(WebCodecsBufferTransfer, AdoptedContentsCanBeReleasedOffTheMainThread)
{
    auto content = JSC::ArrayBufferContents::fromSpan(std::span { testBytes });
    ASSERT_TRUE(!!content);
    RefPtr buffer = adoptArrayBufferContents(WTF::move(*content));

    Ref queue = WorkQueue::create("WebCodecsBufferTransfer test"_s);
    BinarySemaphore released;
    queue->dispatch([buffer = WTF::move(buffer), &released]() mutable {
        EXPECT_TRUE(equalSpans(buffer->span(), std::span<const uint8_t> { testBytes }));
        buffer = nullptr;
        released.signal();
    });
    released.wait();
}

TEST(WebCodecsBufferTransfer, TransferredArrayBufferIsAdoptedWithoutCopying)
{
    WTF::initializeMainThread();
    JSC::initialize();
    JSC::VM& vm = JSC::VM::create(JSC::HeapType::Small).leakRef();
    JSC::JSLockHolder locker(vm);

    RefPtr arrayBuffer = JSC::ArrayBuffer::tryCreate(std::span { testBytes });
    ASSERT_TRUE(!!arrayBuffer);
    auto* originalData = static_cast<const uint8_t*>(arrayBuffer->data());

    JSC::ArrayBufferContents content;
    EXPECT_TRUE(arrayBuffer->transferTo(vm, content));
    EXPECT_TRUE(arrayBuffer->isDetached());

    Ref buffer = adoptArrayBufferContents(WTF::move(content));

    EXPECT_EQ(buffer->span().data(), originalData);
    EXPECT_EQ(buffer->size(), testBytes.size());
    EXPECT_TRUE(equalSpans(buffer->span(), std::span<const uint8_t> { testBytes }));
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEB_CODECS)
