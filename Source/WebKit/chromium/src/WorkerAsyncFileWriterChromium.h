/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WorkerAsyncFileWriterChromium_h
#define WorkerAsyncFileWriterChromium_h

#if ENABLE(FILE_SYSTEM)

#include "AsyncFileWriter.h"
#include <wtf/PassOwnPtr.h>

namespace WebKit {
    class WebFileSystem;
    class WebFileWriter;
    class WorkerFileWriterCallbacksBridge;
}

namespace WTF {
    class String;
}
using WTF::String;

namespace WebCore {

class AsyncFileSystem;
class AsyncFileWriterClient;
class Blob;
class WorkerContext;

class WorkerAsyncFileWriterChromium : public AsyncFileWriter {
public:
    enum WriterType {
        Asynchronous,
        Synchronous,
    };

    static PassOwnPtr<WorkerAsyncFileWriterChromium> create(WebKit::WebFileSystem* webFileSystem, const String& path, WorkerContext* workerContext, AsyncFileWriterClient* client, WriterType type)
    {
        return adoptPtr(new WorkerAsyncFileWriterChromium(webFileSystem, path, workerContext, client, type));
    }
    ~WorkerAsyncFileWriterChromium();
    
    bool waitForOperationToComplete();

    // FileWriter
    virtual void write(long long position, Blob* data);
    virtual void truncate(long long length);
    virtual void abort();

private:

    WorkerAsyncFileWriterChromium(WebKit::WebFileSystem*, const String& path, WorkerContext*, AsyncFileWriterClient*, WriterType);
    RefPtr<WebKit::WorkerFileWriterCallbacksBridge> m_bridge;
    WriterType m_type;
};

} // namespace

#endif // ENABLE(FILE_SYSTEM)

#endif // AsyncFileWriterChromium_h
