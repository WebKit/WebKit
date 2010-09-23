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
#include "WebFileInfo.h"
#include "WebFileSystem.h"
#include "WebFileSystemCallbacksImpl.h"
#include "WebFileWriter.h"
#include "WebKit.h"
#include "WebKitClient.h"

#include <wtf/text/CString.h>

namespace WebCore {

AsyncFileSystemChromium::AsyncFileSystemChromium(const String& rootPath)
    : AsyncFileSystem(rootPath)
    , m_webFileSystem(WebKit::webKitClient()->fileSystem())
{
    ASSERT(m_webFileSystem);
}

AsyncFileSystemChromium::~AsyncFileSystemChromium()
{
}

void AsyncFileSystemChromium::move(const String& srcPath, const String& destPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->move(srcPath, destPath, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::copy(const String& srcPath, const String& destPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->copy(srcPath, destPath, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::remove(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->remove(path, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::readMetadata(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->readMetadata(path, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::createFile(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->createFile(path, exclusive, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::createDirectory(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->createDirectory(path, exclusive, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::fileExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->fileExists(path, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::directoryExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->directoryExists(path, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::readDirectory(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->readDirectory(path, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

class FileWriterHelperCallbacks : public WebKit::WebFileSystemCallbacks {
public:
    FileWriterHelperCallbacks(FileWriterClient* client, const String& path, WebKit::WebFileSystem* webFileSystem, PassOwnPtr<WebCore::AsyncFileSystemCallbacks> callbacks)
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
    virtual void didOpenFileSystem(const WebKit::WebString& name, const WebKit::WebString& rootPath)
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
    FileWriterClient* m_client;
    String m_path;
    WebKit::WebFileSystem* m_webFileSystem;
    OwnPtr<WebCore::AsyncFileSystemCallbacks> m_callbacks;
};

void AsyncFileSystemChromium::createWriter(FileWriterClient* client, const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->readMetadata(path, new FileWriterHelperCallbacks(client, path, m_webFileSystem, callbacks));
}

} // namespace WebCore

#endif
