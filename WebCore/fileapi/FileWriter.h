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

#ifndef FileWriter_h
#define FileWriter_h

#if ENABLE(FILE_SYSTEM)

#include "ActiveDOMObject.h"
#include "AsyncFileWriterClient.h"
#include "EventTarget.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class AsyncFileWriter;
class Blob;
class FileError;
class ScriptExecutionContext;

class FileWriter : public RefCounted<FileWriter>, public ActiveDOMObject, public EventTarget, public AsyncFileWriterClient {
public:
    static PassRefPtr<FileWriter> create(ScriptExecutionContext* context)
    {
        return adoptRef(new FileWriter(context));
    }

    void initialize(PassOwnPtr<AsyncFileWriter> writer, long long length);

    enum ReadyState {
        INIT = 0,
        WRITING = 1,
        DONE = 2
    };

    void write(Blob* data, ExceptionCode& ec);
    void seek(long long position, ExceptionCode& ec);
    void truncate(long long length, ExceptionCode& ec);
    void abort(ExceptionCode& ec);

    ReadyState readyState() const { return m_readyState; }
    FileError* error() const { return m_error.get(); }
    long long position() const { return m_position; }
    long long length() const { return m_length; }

    // AsyncFileWriterClient
    void didWrite(long long bytes, bool complete);
    void didTruncate();
    void didFail(ExceptionCode ec);

    // ActiveDOMObject
    virtual bool canSuspend() const;
    virtual bool hasPendingActivity() const;
    virtual void stop();

    // EventTarget
    virtual FileWriter* toFileWriter() { return this; }
    virtual ScriptExecutionContext* scriptExecutionContext() const { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted<FileWriter>::ref;
    using RefCounted<FileWriter>::deref;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(writestart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(progress);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(write);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(writeend);
    
private:
    FileWriter(ScriptExecutionContext*);

    virtual ~FileWriter();

    friend class WTF::RefCounted<FileWriter>;

    // EventTarget
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
    virtual EventTargetData* eventTargetData() { return &m_eventTargetData; }
    virtual EventTargetData* ensureEventTargetData() { return &m_eventTargetData; }

    void fireEvent(const AtomicString& type);

    RefPtr<FileError> m_error;
    EventTargetData m_eventTargetData;
    OwnPtr<AsyncFileWriter> m_writer;
    ReadyState m_readyState;
    long long m_position;
    long long m_length;
    long long m_bytesWritten;
    long long m_bytesToWrite;
    long long m_truncateLength;
};

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)

#endif // FileWriter_h
