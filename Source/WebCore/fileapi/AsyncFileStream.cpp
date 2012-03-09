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

#if ENABLE(BLOB)

#include "AsyncFileStream.h"

#include "Blob.h"
#include "CrossThreadTask.h"
#include "FileStream.h"
#include "FileStreamClient.h"
#include "FileThread.h"
#include "FileThreadTask.h"
#include "PlatformString.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

inline AsyncFileStream::AsyncFileStream(ScriptExecutionContext* context, FileStreamClient* client)
    : m_context(context)
    , m_stream(FileStream::create())
    , m_client(client)
{
}

PassRefPtr<AsyncFileStream> AsyncFileStream::create(ScriptExecutionContext* context, FileStreamClient* client)
{
    RefPtr<AsyncFileStream> proxy = adoptRef(new AsyncFileStream(context, client));

    // Hold a reference so that the instance will not get deleted while there are tasks on the file thread.
    // This is balanced by the deref in derefProxyOnContext below.
    proxy->ref();

    proxy->fileThread()->postTask(createFileThreadTask(proxy.get(), &AsyncFileStream::startOnFileThread));

    return proxy.release();
}

AsyncFileStream::~AsyncFileStream()
{
}

FileThread* AsyncFileStream::fileThread()
{
    ASSERT(m_context->isContextThread());
    ASSERT(m_context->fileThread());
    return m_context->fileThread();
}

static void didStart(ScriptExecutionContext*, AsyncFileStream* proxy)
{
    if (proxy->client())
        proxy->client()->didStart();
}

void AsyncFileStream::startOnFileThread()
{
    if (!client())
        return;
    m_stream->start();
    m_context->postTask(createCallbackTask(&didStart, AllowCrossThreadAccess(this)));
}

void AsyncFileStream::stop()
{
    // Clear the client so that we won't be calling callbacks on the client.
    setClient(0);

    fileThread()->unscheduleTasks(m_stream.get());
    fileThread()->postTask(createFileThreadTask(this, &AsyncFileStream::stopOnFileThread));
}

static void derefProxyOnContext(ScriptExecutionContext*, AsyncFileStream* proxy)
{
    ASSERT(proxy->hasOneRef());
    proxy->deref();
}

void AsyncFileStream::stopOnFileThread()
{
    m_stream->stop();
    m_context->postTask(createCallbackTask(&derefProxyOnContext, AllowCrossThreadAccess(this)));
}

static void didGetSize(ScriptExecutionContext*, AsyncFileStream* proxy, long long size)
{
    if (proxy->client())
        proxy->client()->didGetSize(size);
}

void AsyncFileStream::getSize(const String& path, double expectedModificationTime)
{
    fileThread()->postTask(createFileThreadTask(this, &AsyncFileStream::getSizeOnFileThread, path, expectedModificationTime));
}

void AsyncFileStream::getSizeOnFileThread(const String& path, double expectedModificationTime)
{
    long long size = m_stream->getSize(path, expectedModificationTime);
    m_context->postTask(createCallbackTask(&didGetSize, AllowCrossThreadAccess(this), size));
}

static void didOpen(ScriptExecutionContext*, AsyncFileStream* proxy, bool success)
{
    if (proxy->client())
        proxy->client()->didOpen(success);
}

void AsyncFileStream::openForRead(const String& path, long long offset, long long length)
{
    fileThread()->postTask(createFileThreadTask(this, &AsyncFileStream::openForReadOnFileThread, path, offset, length));
}

void AsyncFileStream::openForReadOnFileThread(const String& path, long long offset, long long length)
{
    bool success = m_stream->openForRead(path, offset, length);
    m_context->postTask(createCallbackTask(&didOpen, AllowCrossThreadAccess(this), success));
}

void AsyncFileStream::openForWrite(const String& path)
{
    fileThread()->postTask(
        createFileThreadTask(this,
                             &AsyncFileStream::openForWriteOnFileThread, path));
}

void AsyncFileStream::openForWriteOnFileThread(const String& path)
{
    bool success = m_stream->openForWrite(path);
    m_context->postTask(createCallbackTask(&didOpen, AllowCrossThreadAccess(this), success));
}

void AsyncFileStream::close()
{
    fileThread()->postTask(createFileThreadTask(this, &AsyncFileStream::closeOnFileThread));
}

void AsyncFileStream::closeOnFileThread()
{
    m_stream->close();
}

static void didRead(ScriptExecutionContext*, AsyncFileStream* proxy, int bytesRead)
{
    if (proxy->client())
        proxy->client()->didRead(bytesRead);
}

void AsyncFileStream::read(char* buffer, int length)
{
    fileThread()->postTask(
        createFileThreadTask(this, &AsyncFileStream::readOnFileThread,
                             AllowCrossThreadAccess(buffer), length));
}

void AsyncFileStream::readOnFileThread(char* buffer, int length)
{
    int bytesRead = m_stream->read(buffer, length);
    m_context->postTask(createCallbackTask(&didRead, AllowCrossThreadAccess(this), bytesRead));
}

static void didWrite(ScriptExecutionContext*, AsyncFileStream* proxy, int bytesWritten)
{
    if (proxy->client())
        proxy->client()->didWrite(bytesWritten);
}

void AsyncFileStream::write(const KURL& blobURL, long long position, int length)
{
    fileThread()->postTask(createFileThreadTask(this, &AsyncFileStream::writeOnFileThread, blobURL, position, length));
}

void AsyncFileStream::writeOnFileThread(const KURL& blobURL, long long position, int length)
{
    int bytesWritten = m_stream->write(blobURL, position, length);
    m_context->postTask(createCallbackTask(&didWrite, AllowCrossThreadAccess(this), bytesWritten));
}

static void didTruncate(ScriptExecutionContext*, AsyncFileStream* proxy, bool success)
{
    if (proxy->client())
        proxy->client()->didTruncate(success);
}

void AsyncFileStream::truncate(long long position)
{
    fileThread()->postTask(createFileThreadTask(this, &AsyncFileStream::truncateOnFileThread, position));
}

void AsyncFileStream::truncateOnFileThread(long long position)
{
    bool success = m_stream->truncate(position);
    m_context->postTask(createCallbackTask(&didTruncate, AllowCrossThreadAccess(this), success));
}

} // namespace WebCore

#endif // ENABLE(BLOB)
