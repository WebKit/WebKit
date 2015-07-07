/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "config.h"

#include "AsyncFileStream.h"

#include "Blob.h"
#include "FileStream.h"
#include "FileStreamClient.h"
#include "FileThread.h"
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS)
#include "WebCoreThread.h"
#endif

namespace WebCore {

static PassRefPtr<FileThread> createFileThread()
{
    RefPtr<FileThread> thread = FileThread::create();
    if (!thread->start())
        return 0;
    return thread.release();
}

static FileThread* fileThread()
{
    ASSERT(isMainThread());
    static FileThread* thread = createFileThread().leakRef();
    return thread;
}

inline AsyncFileStream::AsyncFileStream(FileStreamClient* client)
    : m_stream(FileStream::create())
    , m_client(client)
{
    ASSERT(isMainThread());
}

PassRefPtr<AsyncFileStream> AsyncFileStream::create(FileStreamClient* client)
{
    RefPtr<AsyncFileStream> proxy = adoptRef(new AsyncFileStream(client));

    // Hold a reference so that the instance will not get deleted while there are tasks on the file thread.
    // This is balanced by the deref in derefProxyOnContext below.
    proxy->ref();

    AsyncFileStream* proxyPtr = proxy.get();
    fileThread()->postTask({ proxyPtr, [=] {
        // FIXME: It is not correct to check m_client from a secondary thread - stop() could be racing with this check.
        if (!proxyPtr->client())
            return;

        proxyPtr->m_stream->start();
        callOnMainThread([proxyPtr] {
            if (proxyPtr->client())
                proxyPtr->client()->didStart();
        });
    } });

    return proxy.release();
}

AsyncFileStream::~AsyncFileStream()
{
}

void AsyncFileStream::stop()
{
    // Clear the client so that we won't be invoking callbacks on the client.
    setClient(0);

    fileThread()->unscheduleTasks(m_stream.get());
    fileThread()->postTask({ this, [this] {
        m_stream->stop();
        callOnMainThread([this] {
            ASSERT(hasOneRef());
            deref();
        });
    } });
}

void AsyncFileStream::getSize(const String& path, double expectedModificationTime)
{
    String pathCopy = path.isolatedCopy();
    fileThread()->postTask({ this, [this, pathCopy, expectedModificationTime] {
        long long size = m_stream->getSize(pathCopy, expectedModificationTime);
        callOnMainThread([this, size] {
            if (client())
                client()->didGetSize(size);
        });
    } });
}

void AsyncFileStream::openForRead(const String& path, long long offset, long long length)
{
    String pathCopy = path.isolatedCopy();
    fileThread()->postTask({ this, [this, pathCopy, offset, length] {
        bool success = m_stream->openForRead(pathCopy, offset, length);
        callOnMainThread([this, success] {
            if (client())
                client()->didOpen(success);
        });
    } });
}

void AsyncFileStream::openForWrite(const String& path)
{
    String pathCopy = path.isolatedCopy();
    fileThread()->postTask({ this, [this, pathCopy] {
        bool success = m_stream->openForWrite(pathCopy);
        callOnMainThread([this, success] {
            if (client())
                client()->didOpen(success);
        });
    } });
}

void AsyncFileStream::close()
{
    fileThread()->postTask({this, [this] {
        m_stream->close();
    } });
}

void AsyncFileStream::read(char* buffer, int length)
{
    fileThread()->postTask({ this, [this, buffer, length] {
        int bytesRead = m_stream->read(buffer, length);
        callOnMainThread([this, bytesRead] {
            if (client())
                client()->didRead(bytesRead);
        });
    } });
}

void AsyncFileStream::write(const URL& blobURL, long long position, int length)
{
    URL blobURLCopy = blobURL.copy();
    fileThread()->postTask({ this, [this, blobURLCopy, position, length] {
        int bytesWritten = m_stream->write(blobURLCopy, position, length);
        callOnMainThread([this, bytesWritten] {
            if (client())
                client()->didWrite(bytesWritten);
        });
    } });
}

void AsyncFileStream::truncate(long long position)
{
    fileThread()->postTask({ this, [this, position] {
        bool success = m_stream->truncate(position);
        callOnMainThread([this, success] {
            if (client())
                client()->didTruncate(success);
        });
    } });
}

} // namespace WebCore
