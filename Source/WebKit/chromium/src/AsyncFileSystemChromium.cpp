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
#include "AsyncFileSystemChromium.h"

#if ENABLE(FILE_SYSTEM)

#include "AsyncFileSystemCallbacks.h"
#include "AsyncFileWriterChromium.h"
#include "BlobURL.h"
#include "FileMetadata.h"
#include "SecurityOrigin.h"
#include "ThreadableBlobRegistry.h"
#include "WebFileSystemCallbacksImpl.h"
#include "WebFileWriter.h"
#include <public/Platform.h>
#include <public/WebFileInfo.h>
#include <public/WebFileSystem.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

namespace {

// Specialized callback class for createSnapshotFileAndReadMetadata.
class SnapshotFileCallbacks : public AsyncFileSystemCallbacks {
public:
    static PassOwnPtr<SnapshotFileCallbacks> create(const KURL& internalBlobURL, PassOwnPtr<WebCore::AsyncFileSystemCallbacks> callbacks)
    {
        return adoptPtr(new SnapshotFileCallbacks(internalBlobURL, callbacks));
    }

    virtual void didReadMetadata(const FileMetadata& metadata)
    {
        ASSERT(m_callbacks);

        // This will create a new File object using the metadata.
        m_callbacks->didReadMetadata(metadata);

        // Now that we've registered the snapshot file, we can unregister our internalBlobURL which has played a placeholder for the file during the IPC.
        ThreadableBlobRegistry::unregisterBlobURL(m_internalBlobURL);
    }

    virtual void didFail(int error)
    {
        ASSERT(m_callbacks);
        m_callbacks->didFail(error);
    }

private:
    SnapshotFileCallbacks(const KURL& internalBlobURL, PassOwnPtr<WebCore::AsyncFileSystemCallbacks> callbacks)
        : m_internalBlobURL(internalBlobURL)
        , m_callbacks(callbacks)
    {
    }

    KURL m_internalBlobURL;
    OwnPtr<WebCore::AsyncFileSystemCallbacks> m_callbacks;
};

} // namespace

bool AsyncFileSystem::isAvailable()
{
    return true;
}

PassOwnPtr<AsyncFileSystem> AsyncFileSystem::create()
{
    return AsyncFileSystemChromium::create();
}

AsyncFileSystemChromium::AsyncFileSystemChromium()
    : m_webFileSystem(WebKit::Platform::current()->fileSystem())
{
    ASSERT(m_webFileSystem);
}

AsyncFileSystemChromium::~AsyncFileSystemChromium()
{
}

void AsyncFileSystemChromium::move(const KURL& sourcePath, const KURL& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->move(sourcePath, destinationPath, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::copy(const KURL& sourcePath, const KURL& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->copy(sourcePath, destinationPath, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::remove(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->remove(path, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::removeRecursively(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->removeRecursively(path, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::readMetadata(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->readMetadata(path, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::createFile(const KURL& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->createFile(path, exclusive, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::createDirectory(const KURL& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->createDirectory(path, exclusive, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::fileExists(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->fileExists(path, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::directoryExists(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->directoryExists(path, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::readDirectory(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->readDirectory(path, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

class FileWriterHelperCallbacks : public WebKit::WebFileSystemCallbacks {
public:
    FileWriterHelperCallbacks(AsyncFileWriterClient* client, const KURL& path, WebKit::WebFileSystem* webFileSystem, PassOwnPtr<WebCore::AsyncFileSystemCallbacks> callbacks)
        : m_client(client)
        , m_path(path)
        , m_webFileSystem(webFileSystem)
        , m_callbacks(callbacks)
    {
    }

    virtual void didSucceed()
    {
        ASSERT_NOT_REACHED();
        delete this;
    }
    virtual void didReadMetadata(const WebKit::WebFileInfo& info)
    {
        ASSERT(m_callbacks);
        if (info.type != WebKit::WebFileInfo::TypeFile || info.length < 0)
            m_callbacks->didFail(WebKit::WebFileErrorInvalidState);
        else {
            OwnPtr<AsyncFileWriterChromium> asyncFileWriterChromium = adoptPtr(new AsyncFileWriterChromium(m_client));
            OwnPtr<WebKit::WebFileWriter> webFileWriter = adoptPtr(m_webFileSystem->createFileWriter(m_path, asyncFileWriterChromium.get()));
            asyncFileWriterChromium->setWebFileWriter(webFileWriter.release());
            m_callbacks->didCreateFileWriter(asyncFileWriterChromium.release(), info.length);
        }
        delete this;
    }

    virtual void didReadDirectory(const WebKit::WebVector<WebKit::WebFileSystemEntry>& entries, bool hasMore)
    {
        ASSERT_NOT_REACHED();
        delete this;
    }
    virtual void didOpenFileSystem(const WebKit::WebString& name, const WebKit::WebURL& rootURL)
    {
        ASSERT_NOT_REACHED();
        delete this;
    }

    virtual void didFail(WebKit::WebFileError error)
    {
        ASSERT(m_callbacks);
        m_callbacks->didFail(error);
        delete this;
    }

private:
    AsyncFileWriterClient* m_client;
    KURL m_path;
    WebKit::WebFileSystem* m_webFileSystem;
    OwnPtr<WebCore::AsyncFileSystemCallbacks> m_callbacks;
};

void AsyncFileSystemChromium::createWriter(AsyncFileWriterClient* client, const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->readMetadata(path, new FileWriterHelperCallbacks(client, path, m_webFileSystem, callbacks));
}

void AsyncFileSystemChromium::createSnapshotFileAndReadMetadata(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    KURL internalBlobURL = BlobURL::createInternalURL();

    // This will create a snapshot file and register the file to a blob using the given internalBlobURL.
    m_webFileSystem->createSnapshotFileAndReadMetadata(internalBlobURL, path, new WebKit::WebFileSystemCallbacksImpl(createSnapshotFileCallback(internalBlobURL, callbacks)));
}

PassOwnPtr<AsyncFileSystemCallbacks> AsyncFileSystemChromium::createSnapshotFileCallback(const KURL& internalBlobURL, PassOwnPtr<AsyncFileSystemCallbacks> callbacks) const
{
    return SnapshotFileCallbacks::create(internalBlobURL, callbacks);
}

} // namespace WebCore

#endif
