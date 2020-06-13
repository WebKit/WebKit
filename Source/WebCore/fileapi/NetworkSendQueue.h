/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "ExceptionCode.h"
#include <wtf/Deque.h>
#include <wtf/Function.h>
#include <wtf/UniqueRef.h>
#include <wtf/Variant.h>
#include <wtf/WeakPtr.h>

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

class Blob;
class BlobLoader;
class Document;
class SharedBuffer;

class WEBCORE_EXPORT NetworkSendQueue {
public:
    using WriteString = Function<void(const String&)>;
    using WriteRawData = Function<void(const char*, size_t)>;
    enum class Continue { No, Yes };
    using ProcessError = Function<Continue(ExceptionCode)>;
    NetworkSendQueue(Document&, WriteString&&, WriteRawData&&, ProcessError&&);
    ~NetworkSendQueue();

    void enqueue(const String&);
    void enqueue(const JSC::ArrayBuffer&, unsigned byteOffset, unsigned byteLength);
    void enqueue(Blob&);

    void clear();

private:
    void processMessages();

    using Message = Variant<String, Ref<SharedBuffer>, UniqueRef<BlobLoader>>;
    Deque<Message> m_queue;

    WTF::WeakPtr<Document> m_document;

    WriteString m_writeString;
    WriteRawData m_writeRawData;
    ProcessError m_processError;
};

} // namespace WebCore
