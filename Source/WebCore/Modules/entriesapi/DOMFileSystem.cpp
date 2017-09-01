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
#include "ScriptExecutionContext.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/UUID.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

struct ListedChild {
    String filename;
    FileMetadata::Type type;

    ListedChild isolatedCopy() const { return { filename.isolatedCopy(), type }; }
};

static ExceptionOr<Vector<ListedChild>> listDirectoryWithMetadata(const String& fullPath)
{
    ASSERT(!isMainThread());
    if (!fileIsDirectory(fullPath, ShouldFollowSymbolicLinks::No))
        return Exception { NotFoundError, "Path no longer exists or is no longer a directory" };

    auto childPaths = listDirectory(fullPath, "*");
    Vector<ListedChild> listedChildren;
    listedChildren.reserveInitialCapacity(childPaths.size());
    for (auto& childPath : childPaths) {
        FileMetadata metadata;
        if (!getFileMetadata(childPath, metadata, ShouldFollowSymbolicLinks::No))
            continue;
        listedChildren.uncheckedAppend(ListedChild { pathGetFileName(childPath), metadata.type });
    }
    return WTFMove(listedChildren);
}

static ExceptionOr<Vector<Ref<FileSystemEntry>>> toFileSystemEntries(ScriptExecutionContext& context, DOMFileSystem& fileSystem, ExceptionOr<Vector<ListedChild>>&& listedChildren, const String& parentVirtualPath)
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
            entries.uncheckedAppend(FileSystemFileEntry::create(context, fileSystem, virtualPath));
            break;
        case FileMetadata::TypeDirectory:
            entries.uncheckedAppend(FileSystemDirectoryEntry::create(context, fileSystem, virtualPath));
            break;
        default:
            break;
        }
    }
    return WTFMove(entries);
}

static bool isAbsoluteVirtualPath(const String& virtualPath)
{
    return !virtualPath.isEmpty() && virtualPath[0] == '/';
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

Ref<FileSystemDirectoryEntry> DOMFileSystem::root(ScriptExecutionContext& context)
{
    return FileSystemDirectoryEntry::create(context, *this, ASCIILiteral("/"));
}

Ref<FileSystemEntry> DOMFileSystem::fileAsEntry(ScriptExecutionContext& context)
{
    if (m_file->isDirectory())
        return FileSystemDirectoryEntry::create(context, *this, "/" + m_file->name());
    return FileSystemFileEntry::create(context, *this, "/" + m_file->name());
}

// https://wicg.github.io/entries-api/#resolve-a-relative-path
static String resolveRelativePath(const String& virtualPath, const String& relativePath)
{
    ASSERT(virtualPath[0] == '/');
    if (isAbsoluteVirtualPath(relativePath))
        return relativePath;

    auto virtualPathSegments = virtualPath.split('/');
    auto relativePathSegments = relativePath.split('/');
    for (auto& segment : relativePathSegments) {
        ASSERT(!segment.isEmpty());
        if (segment == ".")
            continue;
        if (segment == "..") {
            if (!virtualPathSegments.isEmpty())
                virtualPathSegments.removeLast();
            continue;
        }
        virtualPathSegments.append(segment);
    }

    if (virtualPathSegments.isEmpty())
        return ASCIILiteral("/");

    StringBuilder builder;
    for (auto& segment : virtualPathSegments) {
        builder.append('/');
        builder.append(segment);
    }
    return builder.toString();
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

void DOMFileSystem::listDirectory(ScriptExecutionContext& context, FileSystemDirectoryEntry& directory, DirectoryListingCallback&& completionHandler)
{
    ASSERT(&directory.filesystem() == this);

    String directoryVirtualPath = directory.virtualPath();
    auto fullPath = evaluatePath(directoryVirtualPath);
    if (fullPath == m_rootPath) {
        Vector<Ref<FileSystemEntry>> children;
        children.append(fileAsEntry(context));
        completionHandler(WTFMove(children));
        return;
    }

    m_workQueue->dispatch([this, context = makeRef(context), completionHandler = WTFMove(completionHandler), fullPath = fullPath.isolatedCopy(), directoryVirtualPath = directoryVirtualPath.isolatedCopy()]() mutable {
        auto listedChildren = listDirectoryWithMetadata(fullPath);
        callOnMainThread([this, context = WTFMove(context), completionHandler = WTFMove(completionHandler), listedChildren = crossThreadCopy(listedChildren), directoryVirtualPath = directoryVirtualPath.isolatedCopy()]() mutable {
            completionHandler(toFileSystemEntries(context, *this, WTFMove(listedChildren), directoryVirtualPath));
        });
    });
}

void DOMFileSystem::getParent(ScriptExecutionContext& context, FileSystemEntry& entry, GetParentCallback&& completionCallback)
{
    String virtualPath = resolveRelativePath(entry.virtualPath(), ASCIILiteral(".."));
    ASSERT(virtualPath[0] == '/');
    String fullPath = evaluatePath(virtualPath);
    m_workQueue->dispatch([this, context = makeRef(context), fullPath = crossThreadCopy(fullPath), virtualPath = crossThreadCopy(virtualPath), completionCallback = WTFMove(completionCallback)]() mutable {
        if (!fileIsDirectory(fullPath, ShouldFollowSymbolicLinks::No)) {
            callOnMainThread([completionCallback = WTFMove(completionCallback)] {
                completionCallback(Exception { NotFoundError, "Path no longer exists or is no longer a directory" });
            });
            return;
        }
        callOnMainThread([this, context = WTFMove(context), virtualPath = crossThreadCopy(virtualPath), completionCallback = WTFMove(completionCallback)] {
            completionCallback(FileSystemDirectoryEntry::create(context, *this, virtualPath));
        });
    });
}

} // namespace WebCore
