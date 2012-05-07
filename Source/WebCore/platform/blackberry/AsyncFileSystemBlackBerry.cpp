/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

#include "config.h"
#if ENABLE(FILE_SYSTEM)
#include "AsyncFileSystemBlackBerry.h"

#include "AsyncFileSystemCallbacks.h"
#include "ExceptionCode.h"
#include "NotImplemented.h"

#include <wtf/UnusedParam.h>

namespace WebCore {

bool AsyncFileSystem::isAvailable()
{
    notImplemented();
    return false;
}

bool AsyncFileSystem::isValidType(FileSystemType type)
{
    UNUSED_PARAM(type);

    notImplemented();
    return false;
}

PassOwnPtr<AsyncFileSystem> AsyncFileSystem::create(FileSystemType type)
{
    return adoptPtr(new AsyncFileSystemBlackBerry(type));
}

void AsyncFileSystem::openFileSystem(const String& basePath, const String& storageIdentifier, FileSystemType type, bool, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(basePath);
    UNUSED_PARAM(storageIdentifier);
    UNUSED_PARAM(type);
    UNUSED_PARAM(callbacks);

    notImplemented();
    callbacks->didFail(NOT_SUPPORTED_ERR);
}

bool AsyncFileSystem::crackFileSystemURL(const KURL& url, FileSystemType& type, String& filePath)
{
    UNUSED_PARAM(url);
    UNUSED_PARAM(type);
    UNUSED_PARAM(filePath);

    notImplemented();
    return false;
}

AsyncFileSystemBlackBerry::AsyncFileSystemBlackBerry(FileSystemType type)
    : AsyncFileSystem(type)
{
    notImplemented();
}

AsyncFileSystemBlackBerry::~AsyncFileSystemBlackBerry()
{
    notImplemented();
}

KURL AsyncFileSystemBlackBerry::toURL(const String& originString, const String& fullPath) const
{
    UNUSED_PARAM(originString);
    UNUSED_PARAM(fullPath);

    notImplemented();
    return KURL();
}

void AsyncFileSystemBlackBerry::move(const String& sourcePath, const String& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(sourcePath);
    UNUSED_PARAM(destinationPath);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::copy(const String& sourcePath, const String& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(sourcePath);
    UNUSED_PARAM(destinationPath);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::remove(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::removeRecursively(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::readMetadata(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::createFile(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(exclusive);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::createDirectory(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(exclusive);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::fileExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::directoryExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::readDirectory(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}


void AsyncFileSystemBlackBerry::createWriter(AsyncFileWriterClient* client, const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(client);
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::createSnapshotFileAndReadMetadata(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
