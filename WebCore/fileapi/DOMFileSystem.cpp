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
#include "DOMFileSystem.h"

#if ENABLE(FILE_SYSTEM)

#include "AsyncFileSystem.h"
#include "DOMFilePath.h"
#include "DirectoryEntry.h"
#include "EntriesCallback.h"
#include "Entry.h"
#include "EntryCallback.h"
#include "ErrorCallback.h"
#include "FileError.h"
#include "FileSystemCallbacks.h"
#include "MetadataCallback.h"
#include "ScriptExecutionContext.h"
#include "VoidCallback.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

DOMFileSystem::DOMFileSystem(ScriptExecutionContext* context, const String& name, PassOwnPtr<AsyncFileSystem> asyncFileSystem)
    : ActiveDOMObject(context, this)
    , m_name(name)
    , m_asyncFileSystem(asyncFileSystem)
{
}

DOMFileSystem::~DOMFileSystem()
{
}

PassRefPtr<DirectoryEntry> DOMFileSystem::root()
{
    return DirectoryEntry::create(this, DOMFilePath::root);
}

void DOMFileSystem::stop()
{
    m_asyncFileSystem->stop();
}

bool DOMFileSystem::hasPendingActivity() const
{
    return m_asyncFileSystem->hasPendingActivity();
}

void DOMFileSystem::contextDestroyed()
{
    m_asyncFileSystem->stop();
}

void DOMFileSystem::getMetadata(const Entry* entry, PassRefPtr<MetadataCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    String platformPath = m_asyncFileSystem->virtualToPlatformPath(entry->fullPath());
    m_asyncFileSystem->readMetadata(platformPath, new MetadataCallbacks(successCallback, errorCallback));
}

static bool checkValidityForForCopyOrMove(const Entry* src, Entry* parent, const String& newName)
{
    ASSERT(src);

    if (!parent || !parent->isDirectory())
        return false;

    if (!newName.isEmpty() && !DOMFilePath::isValidName(newName))
        return false;

    // It is an error to try to copy or move an entry inside itself at any depth if it is a directory.
    if (src->isDirectory() && DOMFilePath::isParentOf(src->fullPath(), parent->fullPath()))
        return false;

    // It is an error to copy or move an entry into its parent if a name different from its current one isn't provided.
    if ((newName.isEmpty() || src->name() == newName) && DOMFilePath::getDirectory(src->fullPath()) == parent->fullPath())
        return false;

    return true;
}

void DOMFileSystem::move(const Entry* src, PassRefPtr<Entry> parent, const String& newName, PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    if (!checkValidityForForCopyOrMove(src, parent.get(), newName)) {
        scheduleCallback(scriptExecutionContext(), errorCallback, FileError::create(INVALID_MODIFICATION_ERR));
        return;
    }

    String destPath = parent->fullPath();
    if (!newName.isEmpty())
        destPath = DOMFilePath::append(destPath, newName);
    else
        destPath = DOMFilePath::append(destPath, src->name());

    String srcPlatformPath = m_asyncFileSystem->virtualToPlatformPath(src->fullPath());
    String destPlatformPath = parent->filesystem()->asyncFileSystem()->virtualToPlatformPath(destPath);
    m_asyncFileSystem->move(srcPlatformPath, destPlatformPath, new EntryCallbacks(successCallback, errorCallback, this, destPath, src->isDirectory()));
}

void DOMFileSystem::copy(const Entry* src, PassRefPtr<Entry> parent, const String& newName, PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    if (!checkValidityForForCopyOrMove(src, parent.get(), newName)) {
        scheduleCallback(scriptExecutionContext(), errorCallback, FileError::create(INVALID_MODIFICATION_ERR));
        return;
    }

    String destPath = parent->fullPath();
    if (!newName.isEmpty())
        destPath = DOMFilePath::append(destPath, newName);
    else
        destPath = DOMFilePath::append(destPath, src->name());

    String srcPlatformPath = m_asyncFileSystem->virtualToPlatformPath(src->fullPath());
    String destPlatformPath = parent->filesystem()->asyncFileSystem()->virtualToPlatformPath(destPath);
    m_asyncFileSystem->copy(srcPlatformPath, destPlatformPath, new EntryCallbacks(successCallback, errorCallback, this, destPath, src->isDirectory()));
}

void DOMFileSystem::remove(const Entry* entry, PassRefPtr<VoidCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    ASSERT(entry);
    String platformPath = m_asyncFileSystem->virtualToPlatformPath(entry->fullPath());
    m_asyncFileSystem->remove(platformPath, new VoidCallbacks(successCallback, errorCallback));
}

void DOMFileSystem::getParent(const Entry* entry, PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    ASSERT(entry);
    String path = DOMFilePath::getDirectory(entry->fullPath());
    String platformPath = m_asyncFileSystem->virtualToPlatformPath(path);
    m_asyncFileSystem->directoryExists(platformPath, new EntryCallbacks(successCallback, errorCallback, this, path, true));
}

void DOMFileSystem::getFile(const Entry* base, const String& path, PassRefPtr<Flags> flags, PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    ASSERT(base);
    if (!DOMFilePath::isValidPath(path)) {
        scheduleCallback(scriptExecutionContext(), errorCallback, FileError::create(INVALID_MODIFICATION_ERR));
        return;
    }

    String absolutePath = path;
    if (!DOMFilePath::isAbsolute(path))
        absolutePath = DOMFilePath::removeExtraParentReferences(DOMFilePath::append(base->fullPath(), path));
    String platformPath = m_asyncFileSystem->virtualToPlatformPath(absolutePath);

    OwnPtr<EntryCallbacks> callbacks = adoptPtr(new EntryCallbacks(successCallback, errorCallback, this, absolutePath, false));
    if (flags && flags->isCreate())
        m_asyncFileSystem->createFile(platformPath, flags->isExclusive(), callbacks.release());
    else
        m_asyncFileSystem->fileExists(platformPath, callbacks.release());
}

void DOMFileSystem::getDirectory(const Entry* base, const String& path, PassRefPtr<Flags> flags, PassRefPtr<EntryCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    ASSERT(base);
    if (!DOMFilePath::isValidPath(path)) {
        scheduleCallback(scriptExecutionContext(), errorCallback, FileError::create(INVALID_MODIFICATION_ERR));
        return;
    }

    String absolutePath = path;
    if (!DOMFilePath::isAbsolute(path))
        absolutePath = DOMFilePath::removeExtraParentReferences(DOMFilePath::append(base->fullPath(), path));
    String platformPath = m_asyncFileSystem->virtualToPlatformPath(absolutePath);

    OwnPtr<EntryCallbacks> callbacks = adoptPtr(new EntryCallbacks(successCallback, errorCallback, this, absolutePath, true));
    if (flags && flags->isCreate())
        m_asyncFileSystem->createDirectory(platformPath, flags->isExclusive(), callbacks.release());
    else
        m_asyncFileSystem->directoryExists(platformPath, callbacks.release());
}

void DOMFileSystem::readDirectory(const String& path, PassRefPtr<EntriesCallback> successCallback, PassRefPtr<ErrorCallback> errorCallback)
{
    ASSERT(DOMFilePath::isAbsolute(path));
    String platformPath = m_asyncFileSystem->virtualToPlatformPath(path);
    m_asyncFileSystem->readDirectory(platformPath, new EntriesCallbacks(successCallback, errorCallback, this, path));
}

} // namespace

#endif // ENABLE(FILE_SYSTEM)
