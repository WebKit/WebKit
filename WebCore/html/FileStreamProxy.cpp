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

#include "config.h"

#if ENABLE(FILE_READER) || ENABLE(FILE_WRITER)

#include "FileStreamProxy.h"

#include "Blob.h"
#include "FileStream.h"
#include "FileThread.h"
#include "FileThreadTask.h"
#include "GenericWorkerTask.h"
#include "PlatformString.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

FileStreamProxy::FileStreamProxy(ScriptExecutionContext* context, FileStreamClient* client)
    : m_context(context)
    , m_client(client)
    , m_stream(FileStream::create(this))
{
    // Holds an extra ref so that the instance will not get deleted while there can be any tasks on the file thread.
    ref();

    fileThread()->postTask(createFileThreadTask(m_stream.get(), &FileStream::start));
}

FileStreamProxy::~FileStreamProxy()
{
}

void FileStreamProxy::openForRead(Blob* blob)
{
    fileThread()->postTask(createFileThreadTask(m_stream.get(), &FileStream::openForRead, blob));
}

void FileStreamProxy::openForWrite(const String& path)
{
    fileThread()->postTask(createFileThreadTask(m_stream.get(), &FileStream::openForWrite, path));
}

void FileStreamProxy::close()
{
    fileThread()->postTask(createFileThreadTask(m_stream.get(), &FileStream::close));
}

void FileStreamProxy::read(char* buffer, int length)
{
    fileThread()->postTask(createFileThreadTask(m_stream.get(), &FileStream::read, buffer, length));
}

void FileStreamProxy::write(Blob* blob, long long position, int length)
{
    fileThread()->postTask(createFileThreadTask(m_stream.get(), &FileStream::write, blob, position, length));
}

void FileStreamProxy::truncate(long long position)
{
    fileThread()->postTask(createFileThreadTask(m_stream.get(), &FileStream::truncate, position));
}

FileThread* FileStreamProxy::fileThread()
{
    ASSERT(m_context->isContextThread());
    ASSERT(m_context->fileThread());
    return m_context->fileThread();
}

void FileStreamProxy::stop()
{
    // Clear the client so that we won't be calling callbacks on the client.
    m_client = 0;

    fileThread()->unscheduleTasks(m_stream.get());
    fileThread()->postTask(createFileThreadTask(m_stream.get(), &FileStream::stop));
}

static void notifyGetSizeOnContext(ScriptExecutionContext*, FileStreamProxy* proxy, long long size)
{
    if (proxy->client())
        proxy->client()->didGetSize(size);
}

void FileStreamProxy::didGetSize(long long size)
{
    ASSERT(!m_context->isContextThread());
    m_context->postTask(createCallbackTask(&notifyGetSizeOnContext, this, size));
}

static void notifyReadOnContext(ScriptExecutionContext*, FileStreamProxy* proxy, const char* data, int bytesRead)
{
    if (proxy->client())
        proxy->client()->didRead(data, bytesRead);
}

void FileStreamProxy::didRead(const char* data, int bytesRead)
{
    ASSERT(!m_context->isContextThread());
    m_context->postTask(createCallbackTask(&notifyReadOnContext, this, data, bytesRead));
}

static void notifyWriteOnContext(ScriptExecutionContext*, FileStreamProxy* proxy, int bytesWritten)
{
    if (proxy->client())
        proxy->client()->didWrite(bytesWritten);
}

void FileStreamProxy::didWrite(int bytesWritten)
{
    ASSERT(!m_context->isContextThread());
    m_context->postTask(createCallbackTask(&notifyWriteOnContext, this, bytesWritten));
}

static void notifyStartOnContext(ScriptExecutionContext*, FileStreamProxy* proxy)
{
    if (proxy->client())
        proxy->client()->didStart();
}

void FileStreamProxy::didStart()
{
    ASSERT(!m_context->isContextThread());
    m_context->postTask(createCallbackTask(&notifyStartOnContext, this));
}

static void notifyFinishOnContext(ScriptExecutionContext*, FileStreamProxy* proxy)
{
    if (proxy->client())
        proxy->client()->didFinish();
}

void FileStreamProxy::didFinish()
{
    ASSERT(!m_context->isContextThread());
    m_context->postTask(createCallbackTask(&notifyFinishOnContext, this));
}

static void notifyFailOnContext(ScriptExecutionContext*, FileStreamProxy* proxy, ExceptionCode ec)
{
    if (proxy->client())
        proxy->client()->didFail(ec);
}

void FileStreamProxy::didFail(ExceptionCode ec)
{
    ASSERT(!m_context->isContextThread());
    m_context->postTask(createCallbackTask(&notifyFailOnContext, this, ec));
}

static void derefProxyOnContext(ScriptExecutionContext*, FileStreamProxy* proxy)
{
    ASSERT(proxy->hasOneRef());
    proxy->deref();
}

void FileStreamProxy::didStop()
{
    m_context->postTask(createCallbackTask(&derefProxyOnContext, this));
}

} // namespace WebCore

#endif // ENABLE(FILE_WRITER)
