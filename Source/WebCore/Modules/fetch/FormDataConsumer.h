/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ScriptExecutionContextIdentifier.h"
#include <span>
#include <wtf/FastMalloc.h>
#include <wtf/Function.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

class BlobLoader;
class FormData;
class ScriptExecutionContext;

class FormDataConsumer : public CanMakeWeakPtr<FormDataConsumer> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Callback = Function<void(ExceptionOr<std::span<const uint8_t>>)>;
    FormDataConsumer(const FormData&, ScriptExecutionContext&, Callback&&);
    WEBCORE_EXPORT ~FormDataConsumer();

    void cancel();

private:
    void consumeData(const Vector<uint8_t>&);
    void consumeFile(const String&);
    void consumeBlob(const URL&);

    void consume(std::span<const uint8_t>);
    void read();
    void didFail(Exception&&);
    bool isCancelled() { return !m_context; }

    Ref<FormData> m_formData;
    RefPtr<ScriptExecutionContext> m_context;
    Callback m_callback;

    size_t m_currentElementIndex { 0 };
    Ref<WorkQueue> m_fileQueue;
    std::unique_ptr<BlobLoader> m_blobLoader;
};

} // namespace WebCore
