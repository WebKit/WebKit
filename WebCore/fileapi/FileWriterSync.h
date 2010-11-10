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

#ifndef FileWriterSync_h
#define FileWriterSync_h

#if ENABLE(FILE_SYSTEM)

#include "ActiveDOMObject.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Blob;

typedef int ExceptionCode;

// FIXME: This is an empty shell waiting for implementation.
class FileWriterSync : public RefCounted<FileWriterSync>, public ActiveDOMObject {
public:
    static PassRefPtr<FileWriterSync> create(ScriptExecutionContext* context)
    {
        return adoptRef(new FileWriterSync(context));
    }

    void write(Blob* data, ExceptionCode&);
    void seek(long long position, ExceptionCode&);
    void truncate(long long length, ExceptionCode&);

    long long position() const { return m_position; }
    long long length() const { return m_length; }

    // ActiveDOMObject
    virtual bool canSuspend() const;
    virtual bool hasPendingActivity() const;
    virtual void stop();

    using RefCounted<FileWriterSync>::ref;
    using RefCounted<FileWriterSync>::deref;

private:
    FileWriterSync(ScriptExecutionContext*);
    virtual ~FileWriterSync();

    friend class RefCounted<FileWriterSync>;

    long long m_position;
    long long m_length;
};

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)

#endif // FileWriter_h
