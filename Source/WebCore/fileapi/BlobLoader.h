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

#include "Blob.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "FileReaderLoader.h"
#include "FileReaderLoaderClient.h"
#include "SharedBuffer.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Optional.h>

namespace WebCore {

class BlobLoader final : public FileReaderLoaderClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit BlobLoader(CompletionHandler<void(BlobLoader&)>&&);
    ~BlobLoader();

    void start(Blob&, ScriptExecutionContext*, FileReaderLoader::ReadType);

    void cancel();
    bool isLoading() const { return m_loader && m_completionHandler; }
    String stringResult() const { return m_loader ? m_loader->stringResult() : String(); }
    RefPtr<JSC::ArrayBuffer> arrayBufferResult() const { return m_loader ? m_loader->arrayBufferResult() : nullptr; }
    Optional<ExceptionCode> errorCode() const { return m_loader ? m_loader->errorCode() : WTF::nullopt; }

private:
    void didStartLoading() final { }
    void didReceiveData() final { }

    void didFinishLoading() final;
    void didFail(ExceptionCode errorCode) final;
    void complete();

    std::unique_ptr<FileReaderLoader> m_loader;
    CompletionHandler<void(BlobLoader&)> m_completionHandler;
};

inline BlobLoader::BlobLoader(CompletionHandler<void(BlobLoader&)>&& completionHandler)
    : m_completionHandler(WTFMove(completionHandler))
{
}

inline BlobLoader::~BlobLoader()
{
    if (isLoading())
        cancel();
}

inline void BlobLoader::cancel()
{
    if (m_loader)
        m_loader->cancel();

    if (m_completionHandler)
        m_completionHandler(*this);
}

inline void BlobLoader::start(Blob& blob, ScriptExecutionContext* context, FileReaderLoader::ReadType readType)
{
    ASSERT(!m_loader);
    m_loader = makeUnique<FileReaderLoader>(readType, this);
    m_loader->start(context, blob);
}

inline void BlobLoader::didFinishLoading()
{
    m_completionHandler(*this);
}

inline void BlobLoader::didFail(ExceptionCode)
{
    m_completionHandler(*this);
}

} // namespace WebCore
