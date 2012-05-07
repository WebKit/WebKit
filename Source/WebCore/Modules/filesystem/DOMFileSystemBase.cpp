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
#include "DOMFileSystemBase.h"

#if ENABLE(FILE_SYSTEM)

#include "DOMFilePath.h"
#include "DirectoryEntry.h"
#include "DirectoryReaderBase.h"
#include "EntriesCallback.h"
#include "EntryArray.h"
#include "EntryBase.h"
#include "EntryCallback.h"
#include "ErrorCallback.h"
#include "FileError.h"
#include "FileSystemCallbacks.h"
#include "MetadataCallback.h"
#include "ScriptExecutionContext.h"
#include "VoidCallback.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

DOMFileSystemBase::DOMFileSystemBase(ScriptExecutionContext* context, const String& name, PassOwnPtr<AsyncFileSystem> asyncFileSystem)
    : m_context(context)
    , m_name(name)
    , m_asyncFileSystem(asyncFileSystem)
{
}

DOMFileSystemBase::~DOMFileSystemBase()
{
}

SecurityOrigin* DOMFileSystemBase::securityOrigin() const
{
    return m_context->securityOrigin();
}

bool DOMFileSystemBase::getMetadata(const EntryBase* entry, PassRefPtr<MetadataCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    m_asyncFileSystem->readMetadata(entry->fullPath(), MetadataCallbacks::create(successCallback, errorCallback));
    return true;
}

static bool verifyAndGetDestinationPathForCopyOrMove(const EntryBase* source, EntryBase* parent, const String& newName, String& destinationPath)
{
    ASSERT(source);

    if (!parent || !parent->isDirectory())
        return false;

    if (!newName.isEmpty() && !DOMFilePath::isValidName(newName))
        return false;

    // It is an error to try to copy or move an entry inside itself at any depth if it is a directory.
    if (source->isDirectory() && DOMFilePath::isParentOf(source->fullPath(), parent->fullPath()))
        return false;

    // It is an error to copy or move an entry into its parent if a name different from its current one isn't provided.
    if ((newName.isEmpty() || source->name() == newName) && DOMFilePath::getDirectory(source->fullPath()) == parent->fullPath())
        return false;

    destinationPath = parent->fullPath();
    if (!newName.isEmpty())
        destinationPath = DOMFilePath::append(destinationPath, newName);
    else
        destinationPath = DOMFilePath::append(destinationPath, source->name());

    return true;
}

static bool pathToAbsolutePath(FileSystemType type, const EntryBase* base, String path, String& absolutePath)
{
    ASSERT(base);

    if (!DOMFilePath::isAbsolute(path))
        path = DOMFilePath::append(base->fullPath(), path);
    absolutePath = DOMFilePath::removeExtraParentReferences(path);

    if ((type == FileSystemTypeTemporary || type == FileSystemTypePersistent) && !DOMFilePath::isValidPath(absolutePath))
        return false;
    return true;
}

bool DOMFileSystemBase::move(const EntryBase* source, EntryBase* parent, const String& newName, PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    String destinationPath;
    if (!verifyAndGetDestinationPathForCopyOrMove(source, parent, newName, destinationPath))
        return false;

    m_asyncFileSystem->move(source->fullPath(), destinationPath, EntryCallbacks::create(successCallback, errorCallback, this, destinationPath, source->isDirectory()));
    return true;
}

bool DOMFileSystemBase::copy(const EntryBase* source, EntryBase* parent, const String& newName, PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    String destinationPath;
    if (!verifyAndGetDestinationPathForCopyOrMove(source, parent, newName, destinationPath))
        return false;

    m_asyncFileSystem->copy(source->fullPath(), destinationPath, EntryCallbacks::create(successCallback, errorCallback, this, destinationPath, source->isDirectory()));
    return true;
}

bool DOMFileSystemBase::remove(const EntryBase* entry, PassRefPtr<VoidCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    ASSERT(entry);
    // We don't allow calling remove() on the root directory.
    if (entry->fullPath() == String(DOMFilePath::root))
        return false;
    m_asyncFileSystem->remove(entry->fullPath(), VoidCallbacks::create(successCallback, errorCallback));
    return true;
}

bool DOMFileSystemBase::removeRecursively(const EntryBase* entry, PassRefPtr<VoidCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    ASSERT(entry && entry->isDirectory());
    // We don't allow calling remove() on the root directory.
    if (entry->fullPath() == String(DOMFilePath::root))
        return false;
    m_asyncFileSystem->removeRecursively(entry->fullPath(), VoidCallbacks::create(successCallback, errorCallback));
    return true;
}

bool DOMFileSystemBase::getParent(const EntryBase* entry, PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    ASSERT(entry);
    String path = DOMFilePath::getDirectory(entry->fullPath());

    m_asyncFileSystem->directoryExists(path, EntryCallbacks::create(successCallback, errorCallback, this, path, true));
    return true;
}

bool DOMFileSystemBase::getFile(const EntryBase* entry, const String& path, PassRefPtr<WebKitFlags> flags, PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    String absolutePath;
    if (!pathToAbsolutePath(m_asyncFileSystem->type(), entry, path, absolutePath))
        return false;

    OwnPtr<EntryCallbacks> callbacks = EntryCallbacks::create(successCallback, errorCallback, this, absolutePath, false);
    if (flags && flags->isCreate())
        m_asyncFileSystem->createFile(absolutePath, flags->isExclusive(), callbacks.release());
    else
        m_asyncFileSystem->fileExists(absolutePath, callbacks.release());
    return true;
}

bool DOMFileSystemBase::getDirectory(const EntryBase* entry, const String& path, PassRefPtr<WebKitFlags> flags, PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    String absolutePath;
    if (!pathToAbsolutePath(m_asyncFileSystem->type(), entry, path, absolutePath))
        return false;

    OwnPtr<EntryCallbacks> callbacks = EntryCallbacks::create(successCallback, errorCallback, this, absolutePath, true);
    if (flags && flags->isCreate())
        m_asyncFileSystem->createDirectory(absolutePath, flags->isExclusive(), callbacks.release());
    else
        m_asyncFileSystem->directoryExists(absolutePath, callbacks.release());
    return true;
}

bool DOMFileSystemBase::readDirectory(PassRefPtr<DirectoryReaderBase> reader, const String& path, PassRefPtr<EntriesCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    ASSERT(DOMFilePath::isAbsolute(path));
    m_asyncFileSystem->readDirectory(path, EntriesCallbacks::create(successCallback, errorCallback, reader, path));
    return true;
}

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM)
