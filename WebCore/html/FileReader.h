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

#ifndef FileReader_h
#define FileReader_h

#if ENABLE(FILE_READER)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "FileError.h"
#include "FileStreamClient.h"
#include "PlatformString.h"
#include "ScriptString.h"
#include "TextEncoding.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class Blob;
class File;
class FileStreamProxy;
class ScriptExecutionContext;

class FileReader : public RefCounted<FileReader>, public ActiveDOMObject, public EventTarget, public FileStreamClient {
public:
    static PassRefPtr<FileReader> create(ScriptExecutionContext* context)
    {
        return adoptRef(new FileReader(context));
    }

    virtual ~FileReader();

    enum ReadyState {
        Empty = 0,
        Loading = 1,
        Done = 2
    };

    void readAsBinaryString(Blob*);
    void readAsText(Blob*, const String& encoding = "");
    void readAsDataURL(File*);
    void abort();

    ReadyState readyState() const { return m_state; }
    PassRefPtr<FileError> error() { return m_error; }
    const ScriptString& result();

    // ActiveDOMObject
    virtual bool canSuspend() const;
    virtual void stop();
    virtual bool hasPendingActivity() const;

    // EventTarget
    virtual FileReader* toFileReader() { return this; }
    virtual ScriptExecutionContext* scriptExecutionContext() const { return ActiveDOMObject::scriptExecutionContext(); }

    // FileStreamClient
    virtual void didStart();
    virtual void didGetSize(long long);
    virtual void didRead(const char*, int);
    virtual void didFinish();
    virtual void didFail(ExceptionCode);

    using RefCounted<FileReader>::ref;
    using RefCounted<FileReader>::deref;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(progress);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(load);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadend);

private:
    enum ReadType {
        ReadFileAsBinaryString,
        ReadFileAsText,
        ReadFileAsDataURL
    };

    FileReader(ScriptExecutionContext*);

    // EventTarget
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
    virtual EventTargetData* eventTargetData() { return &m_eventTargetData; }
    virtual EventTargetData* ensureEventTargetData() { return &m_eventTargetData; }

    void terminate();
    void readInternal(Blob*, ReadType);
    void fireEvent(const AtomicString& type);
    void convertToText();
    void convertToDataURL();

    ReadyState m_state;
    EventTargetData m_eventTargetData;

    RefPtr<Blob> m_fileBlob;
    ReadType m_readType;

    // Like XMLHttpRequest.m_responseText, we keep this as a ScriptString, not a WebCore::String.
    // That's because these strings can easily get huge (they are filled from the file) and
    // because JS can easily observe many intermediate states, so it's very useful to be
    // able to share the buffer with JavaScript versions of the whole or partial string.
    // In contrast, this string doesn't interact much with the rest of the engine so it's not that
    // big a cost that it isn't a String.
    ScriptString m_result;

    // The raw data. We have to keep track of all the raw data for it to be converted to text or data URL data.
    Vector<char> m_rawData;
    bool m_isRawDataConverted;

    // Encoding scheme used to decode the data.
    TextEncoding m_encoding;

    // Needed to create data URL.
    String m_fileType;

    RefPtr<FileStreamProxy> m_streamProxy;
    Vector<char> m_buffer;
    RefPtr<FileError> m_error;
    long long m_bytesLoaded;
    long long m_totalBytes;
    double m_lastProgressNotificationTimeMS;
    bool m_alreadyStarted;
};

} // namespace WebCore

#endif

#endif // FileReader_h
