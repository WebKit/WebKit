/*
 * Copyright (C) 2012 ChangSeok Oh <shivamidow@gmail.com>
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

#include "AsyncFileSystemGtk.h"

#include "AsyncFileSystemCallbacks.h"
#include "ExceptionCode.h"
#include "NotImplemented.h"

namespace WebCore {

bool AsyncFileSystem::isAvailable()
{
    notImplemented();
    return false;
}

bool AsyncFileSystem::isValidType(Type type)
{
    notImplemented();
    return false;
}

PassOwnPtr<AsyncFileSystem> AsyncFileSystem::create(Type type)
{
    return adoptPtr(new AsyncFileSystemGtk(type));
}

void AsyncFileSystem::openFileSystem(const String& basePath, const String& storageIdentifier, Type type, bool, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    notImplemented();
    callbacks->didFail(NOT_SUPPORTED_ERR);
}

bool AsyncFileSystem::crackFileSystemURL(const KURL& url, AsyncFileSystem::Type& type, String& filePath)
{
    notImplemented();
    return false;
}

AsyncFileSystemGtk::AsyncFileSystemGtk(AsyncFileSystem::Type type)
    : AsyncFileSystem(type)
{
    notImplemented();
}

AsyncFileSystemGtk::~AsyncFileSystemGtk()
{
    notImplemented();
}

String AsyncFileSystemGtk::toURL(const String& originString, const String& fullPath)
{
    notImplemented();
    return String();
}

void AsyncFileSystemGtk::move(const String& sourcePath, const String& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    notImplemented();
}

void AsyncFileSystemGtk::copy(const String& sourcePath, const String& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    notImplemented();
}

void AsyncFileSystemGtk::remove(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    notImplemented();
}

void AsyncFileSystemGtk::removeRecursively(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    notImplemented();
}

void AsyncFileSystemGtk::readMetadata(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    notImplemented();
}

void AsyncFileSystemGtk::createFile(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    notImplemented();
}

void AsyncFileSystemGtk::createDirectory(const String& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    notImplemented();
}

void AsyncFileSystemGtk::fileExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    notImplemented();
}

void AsyncFileSystemGtk::directoryExists(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    notImplemented();
}

void AsyncFileSystemGtk::readDirectory(const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    notImplemented();
}


void AsyncFileSystemGtk::createWriter(AsyncFileWriterClient* client, const String& path, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    notImplemented();
}

} // namespace WebCore

#endif
