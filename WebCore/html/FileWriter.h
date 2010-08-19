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

#if ENABLE(FILE_WRITER)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Blob;
class FileError;
class ScriptExecutionContext;

// FIXME: This is an empty, do-nothing placeholder for implementation yet to come.
class FileWriter : public RefCounted<FileWriter>, public ActiveDOMObject, public EventTarget {
public:
    static PassRefPtr<FileWriter> create(ScriptExecutionContext* context)
    {
        return adoptRef(new FileWriter(context));
    }

    enum ReadyState {
        EMPTY = 0,
        WRITING = 1,
        DONE = 2
    };

    void write(Blob*);
    void seek(long long position);
    void truncate(long long length);
    void abort();

    ReadyState readyState() const;
    FileError* error() const { return m_error; };
    long long position() const { return 0; };
    long long length() const { return 0; };

    // ActiveDOMObject
    virtual bool canSuspend() const;
    virtual void stop();
    virtual bool hasPendingActivity() const;

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

    friend class RefCounted<FileWriter>;

    // EventTarget
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
    virtual EventTargetData* eventTargetData() { return &m_eventTargetData; }
    virtual EventTargetData* ensureEventTargetData() { return &m_eventTargetData; }

    RefPtr<FileError*> m_error;
    EventTargetData m_eventTargetData;
};

} // namespace WebCore

#endif // ENABLE(FILE_WRITER)

#endif // FileWriter_h
