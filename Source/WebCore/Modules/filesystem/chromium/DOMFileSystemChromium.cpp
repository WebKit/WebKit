/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "DOMFileSystemChromium.h"

#if ENABLE(FILE_SYSTEM)

#include "DOMFilePath.h"
#include "FileSystemType.h"
#include "PlatformSupport.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

const char isolatedPathPrefix[] = "isolated";
const char externalPathPrefix[] = "external";

// static
bool DOMFileSystemBase::isValidType(FileSystemType type)
{
    return type == FileSystemTypeTemporary || type == FileSystemTypePersistent || type == FileSystemTypeIsolated || type == FileSystemTypeExternal;
}

// static
bool DOMFileSystemBase::crackFileSystemURL(const KURL& url, FileSystemType& type, String& filePath)
{
    if (!url.protocolIs("filesystem"))
        return false;

    if (!url.innerURL())
        return false;

    String typeString = url.innerURL()->path().substring(1);
    if (typeString == temporaryPathPrefix)
        type = FileSystemTypeTemporary;
    else if (typeString == persistentPathPrefix)
        type = FileSystemTypePersistent;
    else if (typeString == externalPathPrefix)
        type = FileSystemTypeExternal;
    else
        return false;

    filePath = decodeURLEscapeSequences(url.path());
    return true;
}

bool DOMFileSystemBase::supportsToURL() const
{
    ASSERT(isValidType(m_type));
    return m_type != FileSystemTypeIsolated;
}

KURL DOMFileSystemBase::createFileSystemURL(const String& fullPath) const
{
    ASSERT(DOMFilePath::isAbsolute(fullPath));

    if (type() == FileSystemTypeExternal) {
        // For external filesystem originString could be different from what we have in m_filesystemRootURL.
        StringBuilder result;
        result.append("filesystem:");
        result.append(securityOrigin()->toString());
        result.append("/");
        result.append(externalPathPrefix);
        result.append(encodeWithURLEscapeSequences(fullPath));
        return KURL(ParsedURLString, result.toString());
    }

    // For regular types we can just append the entry's fullPath to the m_filesystemRootURL that should look like 'filesystem:<origin>/<typePrefix>'.
    ASSERT(!m_filesystemRootURL.isEmpty());
    KURL url = m_filesystemRootURL;
    // Remove the extra leading slash.
    url.setPath(url.path() + encodeWithURLEscapeSequences(fullPath.substring(1)));
    return url;
}

// static
PassRefPtr<DOMFileSystem> DOMFileSystemChromium::createIsolatedFileSystem(ScriptExecutionContext* context, const String& filesystemId)
{
    StringBuilder filesystemName;
    filesystemName.append(context->securityOrigin()->databaseIdentifier());
    filesystemName.append(":");
    filesystemName.append(isolatedPathPrefix);
    filesystemName.append("_");
    filesystemName.append(filesystemId);

    // The rootURL created here is going to be attached to each filesystem request and
    // is to be validated each time the request is being handled.
    StringBuilder rootURL;
    rootURL.append("filesystem:");
    rootURL.append(context->securityOrigin()->toString());
    rootURL.append("/");
    rootURL.append(isolatedPathPrefix);
    rootURL.append("/");
    rootURL.append(filesystemId);
    rootURL.append("/");

    return DOMFileSystem::create(context, filesystemName.toString(), FileSystemTypeIsolated, KURL(ParsedURLString, rootURL.toString()), PlatformSupport::createAsyncFileSystem());
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
