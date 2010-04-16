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

#ifndef FileStreamProxy_h
#define FileStreamProxy_h

#if ENABLE(FILE_READER) || ENABLE(FILE_WRITER)

#include "ExceptionCode.h"
#include "FileStreamClient.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Blob;
class FileStream;
class FileThread;
class ScriptExecutionContext;
class String;

// A proxy module that calls corresponding FileStream methods on the file thread.  Note: you must call stop() first and then release the reference to destruct the FileStreamProxy instance.
class FileStreamProxy : public RefCounted<FileStreamProxy>, public FileStreamClient {
public:
    static PassRefPtr<FileStreamProxy> create(ScriptExecutionContext* context, FileStreamClient* client)
    {
        return adoptRef(new FileStreamProxy(context, client));
    }
    virtual ~FileStreamProxy();

    void openForRead(Blob* blob);
    void openForWrite(const String& path);
    void close();
    void read(char* buffer, int length);
    void write(Blob* blob, long long position, int length);
    void truncate(long long position);

    // Stops the proxy and scedules it to be destructed.  All the pending tasks will be aborted and the file stream will be closed.
    // Note: the caller should deref the instance immediately after calling stop().
    void stop();

    FileStreamClient* client() const { return m_client; }

private:
    FileStreamProxy(ScriptExecutionContext*, FileStreamClient*);

    // FileStreamClient methods.
    virtual void didGetSize(long long);
    virtual void didRead(const char*, int);
    virtual void didWrite(int);
    virtual void didFinish();
    virtual void didFail(ExceptionCode);
    virtual void didStart();
    virtual void didStop();

    FileThread* fileThread();

    RefPtr<ScriptExecutionContext> m_context;
    FileStreamClient* m_client;
    RefPtr<FileStream> m_stream;
};

} // namespace WebCore

#endif // ENABLE(FILE_READER) || ENABLE(FILE_WRITER)

#endif // FileStreamProxy_h
