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
#include "config.h"
#include "WebFileSystemCallbacksImpl.h"

#if ENABLE(FILE_SYSTEM)

#include "AsyncFileSystemCallbacks.h"
#include "AsyncFileSystemChromium.h"
#include "FileMetadata.h"
#include "ScriptExecutionContext.h"
#include "WebFileSystemEntry.h"
#include "WorkerAsyncFileSystemChromium.h"
#include <public/WebFileInfo.h>
#include <public/WebFileSystem.h>
#include <public/WebString.h>
#include <wtf/Vector.h>

using namespace WebCore;

namespace WebKit {

WebFileSystemCallbacksImpl::WebFileSystemCallbacksImpl(PassOwnPtr<AsyncFileSystemCallbacks> callbacks, ScriptExecutionContext* context, FileSystemSynchronousType synchronousType)
    : m_callbacks(callbacks)
    , m_context(context)
    , m_synchronousType(synchronousType)
{
    ASSERT(m_callbacks);
}

WebFileSystemCallbacksImpl::~WebFileSystemCallbacksImpl()
{
}

void WebFileSystemCallbacksImpl::didSucceed()
{
    m_callbacks->didSucceed();
    delete this;
}

void WebFileSystemCallbacksImpl::didReadMetadata(const WebFileInfo& webFileInfo)
{
    FileMetadata fileMetadata;
    fileMetadata.modificationTime = webFileInfo.modificationTime;
    fileMetadata.length = webFileInfo.length;
    fileMetadata.type = static_cast<FileMetadata::Type>(webFileInfo.type);
    fileMetadata.platformPath = webFileInfo.platformPath;
    m_callbacks->didReadMetadata(fileMetadata);
    delete this;
}

void WebFileSystemCallbacksImpl::didReadDirectory(const WebVector<WebFileSystemEntry>& entries, bool hasMore)
{
    for (size_t i = 0; i < entries.size(); ++i)
        m_callbacks->didReadDirectoryEntry(entries[i].name, entries[i].isDirectory);
    m_callbacks->didReadDirectoryEntries(hasMore);
    delete this;
}

void WebFileSystemCallbacksImpl::didOpenFileSystem(const WebString& name, const WebURL& rootURL)
{
    // This object is intended to delete itself on exit.
    OwnPtr<WebFileSystemCallbacksImpl> callbacks = adoptPtr(this);

#if ENABLE(WORKERS)
    if (m_context && m_context->isWorkerContext()) {
        m_callbacks->didOpenFileSystem(name, rootURL, WorkerAsyncFileSystemChromium::create(m_context, m_synchronousType));
        return;
    }
#endif
    m_callbacks->didOpenFileSystem(name, rootURL, AsyncFileSystemChromium::create());
}

void WebFileSystemCallbacksImpl::didFail(WebFileError error)
{
    m_callbacks->didFail(error);
    delete this;
}

} // namespace WebKit

#endif // ENABLE(FILE_SYSTEM)
