/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DOMFileSystem.h"

#include "File.h"
#include "FileMetadata.h"
#include "FileSystem.h"
#include "FileSystemDirectoryEntry.h"
#include "FileSystemFileEntry.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/UUID.h>

namespace WebCore {

struct ListedChild {
    String filename;
    FileMetadata::Type type;

    ListedChild isolatedCopy() const { return { filename.isolatedCopy(), type }; }
};

static ExceptionOr<Vector<ListedChild>> listDirectoryWithMetadata(const String& fullPath)
{
    ASSERT(!isMainThread());
    if (!fileIsDirectory(fullPath))
        return Exception { NotFoundError, "Path no longer exists or is no longer a directory" };

    auto childPaths = listDirectory(fullPath, "*");
    Vector<ListedChild> listedChildren;
    listedChildren.reserveInitialCapacity(childPaths.size());
    for (auto& childPath : childPaths) {
        FileMetadata metadata;
        if (!getFileMetadata(childPath, metadata))
            continue;
        listedChildren.uncheckedAppend(ListedChild { pathGetFileName(childPath), metadata.type });
    }
    return WTFMove(listedChildren);
}

static ExceptionOr<Vector<Ref<FileSystemEntry>>> toFileSystemEntries(DOMFileSystem& fileSystem, ExceptionOr<Vector<ListedChild>>&& listedChildren, const String& parentVirtualPath)
{
    ASSERT(isMainThread());
    if (listedChildren.hasException())
        return listedChildren.releaseException();

    Vector<Ref<FileSystemEntry>> entries;
    entries.reserveInitialCapacity(listedChildren.returnValue().size());
    for (auto& child : listedChildren.returnValue()) {
        String virtualPath = parentVirtualPath + "/" + child.filename;
        switch (child.type) {
        case FileMetadata::TypeFile:
            entries.uncheckedAppend(FileSystemFileEntry::create(fileSystem, virtualPath));
            break;
        case FileMetadata::TypeDirectory:
            entries.uncheckedAppend(FileSystemDirectoryEntry::create(fileSystem, virtualPath));
            break;
        default:
            break;
        }
    }
    return WTFMove(entries);
}

DOMFileSystem::DOMFileSystem(Ref<File>&& file)
    : m_name(createCanonicalUUIDString())
    , m_file(WTFMove(file))
    , m_rootPath(directoryName(m_file->path()))
    , m_workQueue(WorkQueue::create("DOMFileSystem work queue"))
{
    ASSERT(!m_rootPath.endsWith('/'));
}

DOMFileSystem::~DOMFileSystem()
{
}

Ref<FileSystemDirectoryEntry> DOMFileSystem::root()
{
    return FileSystemDirectoryEntry::create(*this, ASCIILiteral("/"));
}

Ref<FileSystemEntry> DOMFileSystem::fileAsEntry()
{
    if (m_file->isDirectory())
        return FileSystemDirectoryEntry::create(*this, "/" + m_file->name());
    return FileSystemFileEntry::create(*this, "/" + m_file->name());
}

// https://wicg.github.io/entries-api/#evaluate-a-path
String DOMFileSystem::evaluatePath(const String& virtualPath)
{
    ASSERT(virtualPath[0] == '/');
    auto components = virtualPath.split('/');

    Vector<String> resolvedComponents;
    resolvedComponents.reserveInitialCapacity(components.size());
    for (auto& component : components) {
        if (component == ".")
            continue;
        if (component == "..") {
            if (!resolvedComponents.isEmpty())
                resolvedComponents.removeLast();
            continue;
        }
        resolvedComponents.uncheckedAppend(component);
    }

    return pathByAppendingComponents(m_rootPath, resolvedComponents);
}

void DOMFileSystem::listDirectory(FileSystemDirectoryEntry& directory, DirectoryListingCallback&& completionHandler)
{
    ASSERT(&directory.filesystem() == this);

    String directoryVirtualPath = directory.virtualPath();
    auto fullPath = evaluatePath(directoryVirtualPath);
    if (fullPath == m_rootPath) {
        Vector<Ref<FileSystemEntry>> children;
        children.append(fileAsEntry());
        completionHandler(WTFMove(children));
        return;
    }

    m_workQueue->dispatch([this, completionHandler = WTFMove(completionHandler), fullPath = fullPath.isolatedCopy(), directoryVirtualPath = directoryVirtualPath.isolatedCopy()]() mutable {
        auto listedChildren = listDirectoryWithMetadata(fullPath);
        callOnMainThread([this, completionHandler = WTFMove(completionHandler), listedChildren = crossThreadCopy(listedChildren), directoryVirtualPath = directoryVirtualPath.isolatedCopy()]() mutable {
            completionHandler(toFileSystemEntries(*this, WTFMove(listedChildren), directoryVirtualPath));
        });
    });
}

} // namespace WebCore
