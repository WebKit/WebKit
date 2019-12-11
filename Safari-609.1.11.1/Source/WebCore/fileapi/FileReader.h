/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "FileError.h"
#include "FileReaderLoader.h"
#include "FileReaderLoaderClient.h"
#include <wtf/HashMap.h>
#include <wtf/UniqueRef.h>

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

class Blob;

class FileReader final : public RefCounted<FileReader>, public ActiveDOMObject, public EventTargetWithInlineData, private FileReaderLoaderClient {
    WTF_MAKE_ISO_ALLOCATED(FileReader);
public:
    static Ref<FileReader> create(ScriptExecutionContext&);

    virtual ~FileReader();

    enum ReadyState {
        EMPTY = 0,
        LOADING = 1,
        DONE = 2
    };

    ExceptionOr<void> readAsArrayBuffer(Blob*);
    ExceptionOr<void> readAsBinaryString(Blob*);
    ExceptionOr<void> readAsText(Blob*, const String& encoding);
    ExceptionOr<void> readAsDataURL(Blob*);
    void abort();

    void doAbort();

    ReadyState readyState() const { return m_state; }
    RefPtr<FileError> error() { return m_error; }
    FileReaderLoader::ReadType readType() const { return m_readType; }
    Optional<Variant<String, RefPtr<JSC::ArrayBuffer>>> result() const;

    using RefCounted::ref;
    using RefCounted::deref;

    // ActiveDOMObject.
    bool hasPendingActivity() const final;

private:
    explicit FileReader(ScriptExecutionContext&);

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    void stop() final;

    EventTargetInterface eventTargetInterface() const final { return FileReaderEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    void enqueueTask(Function<void()>&&);

    void didStartLoading() final;
    void didReceiveData() final;
    void didFinishLoading() final;
    void didFail(int errorCode) final;

    ExceptionOr<void> readInternal(Blob&, FileReaderLoader::ReadType);
    void fireErrorEvent(int httpStatusCode);
    void fireEvent(const AtomString& type);

    ReadyState m_state { EMPTY };
    bool m_aborting { false };
    RefPtr<Blob> m_blob;
    FileReaderLoader::ReadType m_readType { FileReaderLoader::ReadAsBinaryString };
    String m_encoding;
    std::unique_ptr<FileReaderLoader> m_loader;
    RefPtr<FileError> m_error;
    MonotonicTime m_lastProgressNotificationTime { MonotonicTime::nan() };
    HashMap<uint64_t, Function<void()>> m_pendingTasks;
};

} // namespace WebCore
