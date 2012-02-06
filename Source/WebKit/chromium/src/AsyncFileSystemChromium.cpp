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
#include "SecurityOrigin.h"
#include "WebFileInfo.h"
#include "WebFileSystemCallbacksImpl.h"
#include "WebFileWriter.h"
#include "WebKit.h"
#include "platform/WebFileSystem.h"
#include "platform/WebKitPlatformSupport.h"
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

// ChromeOS-specific filesystem type.
const AsyncFileSystem::Type externalType = static_cast<AsyncFileSystem::Type>(WebKit::WebFileSystem::TypeExternal);
const char externalPathPrefix[] = "external";
const size_t externalPathPrefixLength = sizeof(externalPathPrefix) - 1;

// static
bool AsyncFileSystem::isAvailable()
{
    return true;
}

// static
bool AsyncFileSystem::crackFileSystemURL(const KURL& url, AsyncFileSystem::Type& type, String& filePath)
{
    if (!url.protocolIs("filesystem"))
        return false;

    if (url.innerURL()) {
        String typeString = url.innerURL()->path().substring(1);
        if (typeString == temporaryPathPrefix)
            type = Temporary;
        else if (typeString == persistentPathPrefix)
            type = Persistent;
        else if (typeString == externalPathPrefix)
            type = externalType;
        else
            return false;

        filePath = decodeURLEscapeSequences(url.path());
    } else {
        // FIXME: Remove this clause once http://codereview.chromium.org/7811006
        // lands, which makes this dead code.
        KURL originURL(ParsedURLString, url.path());
        String path = decodeURLEscapeSequences(originURL.path());
        if (path.isEmpty() || path[0] != '/')
            return false;
        path = path.substring(1);

        if (path.startsWith(temporaryPathPrefix)) {
            type = Temporary;
            path = path.substring(temporaryPathPrefixLength);
        } else if (path.startsWith(persistentPathPrefix)) {
            type = Persistent;
            path = path.substring(persistentPathPrefixLength);
        } else if (path.startsWith(externalPathPrefix)) {
            type = externalType;
            path = path.substring(externalPathPrefixLength);
        } else
            return false;

        if (path.isEmpty() || path[0] != '/')
            return false;

        filePath.swap(path);
    }
    return true;
}

// static
bool AsyncFileSystem::isValidType(Type type)
{
    return type == Temporary || type == Persistent || type == static_cast<Type>(WebKit::WebFileSystem::TypeExternal);
}

AsyncFileSystemChromium::AsyncFileSystemChromium(AsyncFileSystem::Type type, const KURL& rootURL)
    : AsyncFileSystem(type)
    , m_webFileSystem(WebKit::webKitPlatformSupport()->fileSystem())
    , m_filesystemRootURL(rootURL)
{
    ASSERT(m_webFileSystem);
}

AsyncFileSystemChromium::~AsyncFileSystemChromium()
{
}

String AsyncFileSystemChromium::toURL(const String& originString, const String& fullPath)
{
    ASSERT(!originString.isEmpty());
    if (originString == "null")
        return String();

    if (type() == externalType) {
        // For external filesystem originString could be different from what we have in m_filesystemRootURL.
        StringBuilder result;
        result.append("filesystem:");
        result.append(originString);
        result.append("/");
        result.append(externalPathPrefix);
        result.append(encodeWithURLEscapeSequences(fullPath));
        return result.toString();
    }

    // For regular types we can just call virtualPathToFileSystemURL which appends the fullPath to the m_filesystemRootURL that should look like 'filesystem:<origin>/<typePrefix>'.
    ASSERT(SecurityOrigin::create(m_filesystemRootURL)->toString() == originString);
    return virtualPathToFileSystemURL(fullPath);
}

void AsyncFileSystemChromium::move(const String& sourcePath, const String& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->move(virtualPathToFileSystemURL(sourcePath), virtualPathToFileSystemURL(destinationPath), new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::copy(const String& sourcePath, const String& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->copy(virtualPathToFileSystemURL(sourcePath), virtualPathToFileSystemURL(destinationPath), new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::remove(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->remove(virtualPathToFileSystemURL(path), new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::removeRecursively(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->removeRecursively(virtualPathToFileSystemURL(path), new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::readMetadata(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->readMetadata(virtualPathToFileSystemURL(path), new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::createFile(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->createFile(virtualPathToFileSystemURL(path), exclusive, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::createDirectory(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->createDirectory(virtualPathToFileSystemURL(path), exclusive, new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::fileExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->fileExists(virtualPathToFileSystemURL(path), new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::directoryExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->directoryExists(virtualPathToFileSystemURL(path), new WebKit::WebFileSystemCallbacksImpl(callbacks));
}

void AsyncFileSystemChromium::readDirectory(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    m_webFileSystem->readDirectory(virtualPathToFileSystemURL(path), new WebKit::WebFileSystemCallbacksImpl(callbacks));
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

void AsyncFileSystemChromium::createWriter(AsyncFileWriterClient* client, const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    KURL pathAsURL = virtualPathToFileSystemURL(path);
    m_webFileSystem->readMetadata(pathAsURL, new FileWriterHelperCallbacks(client, pathAsURL, m_webFileSystem, callbacks));
}

KURL AsyncFileSystemChromium::virtualPathToFileSystemURL(const String& virtualPath) const
{
    ASSERT(!m_filesystemRootURL.isEmpty());
    KURL url = m_filesystemRootURL;
    // Remove the extra leading slash.
    url.setPath(url.path() + encodeWithURLEscapeSequences(virtualPath.substring(1)));
    return url;
}

} // namespace WebCore

#endif
