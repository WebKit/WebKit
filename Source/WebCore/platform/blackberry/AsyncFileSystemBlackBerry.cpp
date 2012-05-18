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

PassOwnPtr<AsyncFileSystem> AsyncFileSystem::create()
{
    return adoptPtr(new AsyncFileSystemBlackBerry());
}

void AsyncFileSystem::openFileSystem(const String& basePath, const String& storageIdentifier, bool, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(basePath);
    UNUSED_PARAM(storageIdentifier);
    UNUSED_PARAM(callbacks);

    notImplemented();
    callbacks->didFail(NOT_SUPPORTED_ERR);
}

AsyncFileSystemBlackBerry::AsyncFileSystemBlackBerry()
{
    notImplemented();
}

AsyncFileSystemBlackBerry::~AsyncFileSystemBlackBerry()
{
    notImplemented();
}

void AsyncFileSystemBlackBerry::move(const KURL& sourcePath, const KURL& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(sourcePath);
    UNUSED_PARAM(destinationPath);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::copy(const KURL& sourcePath, const KURL& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(sourcePath);
    UNUSED_PARAM(destinationPath);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::remove(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::removeRecursively(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::readMetadata(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::createFile(const KURL& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(exclusive);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::createDirectory(const KURL& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(exclusive);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::fileExists(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::directoryExists(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::readDirectory(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}


void AsyncFileSystemBlackBerry::createWriter(AsyncFileWriterClient* client, const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(client);
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

void AsyncFileSystemBlackBerry::createSnapshotFileAndReadMetadata(const KURL& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    UNUSED_PARAM(path);
    UNUSED_PARAM(callbacks);

    notImplemented();
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
