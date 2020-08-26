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
#include "FileSystemDirectoryEntry.h"
#include "FileSystemFileEntry.h"
#include "ScriptExecutionContext.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/FileMetadata.h>
#include <wtf/FileSystem.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/UUID.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DOMFileSystem);

struct ListedChild {
    String filename;
    FileMetadata::Type type;

    ListedChild isolatedCopy() const { return { filename.isolatedCopy(), type }; }
};

static ExceptionOr<Vector<ListedChild>> listDirectoryWithMetadata(const String& fullPath)
{
    ASSERT(!isMainThread());
    if (!FileSystem::fileIsDirectory(fullPath, FileSystem::ShouldFollowSymbolicLinks::No))
        return Exception { NotFoundError, "Path no longer exists or is no longer a directory" };

    auto childPaths = FileSystem::listDirectory(fullPath, "*");
    Vector<ListedChild> listedChildren;
    listedChildren.reserveInitialCapacity(childPaths.size());
    for (auto& childPath : childPaths) {
        auto metadata = FileSystem::fileMetadata(childPath);
        if (!metadata || metadata.value().isHidden)
            continue;
        listedChildren.uncheckedAppend(ListedChild { FileSystem::pathGetFileName(childPath), metadata.value().type });
    }
    return listedChildren;
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
        case FileMetadata::Type::File:
            entries.uncheckedAppend(FileSystemFileEntry::create(context, fileSystem, virtualPath));
            break;
        case FileMetadata::Type::Directory:
            entries.uncheckedAppend(FileSystemDirectoryEntry::create(context, fileSystem, virtualPath));
            break;
        default:
            break;
        }
    }
    return entries;
}

// https://wicg.github.io/entries-api/#name
static bool isValidPathNameCharacter(UChar c)
{
    return c != '\0' && c != '/' && c != '\\';
}

// https://wicg.github.io/entries-api/#path-segment
static bool isValidPathSegment(StringView segment)
{
    if (segment.isEmpty() || segment == "." || segment == "..")
        return true;

    for (unsigned i = 0; i < segment.length(); ++i) {
        if (!isValidPathNameCharacter(segment[i]))
            return false;
    }
    return true;
}

static bool isZeroOrMorePathSegmentsSeparatedBySlashes(StringView string)
{
    auto segments = string.split('/');
    for (auto segment : segments) {
        if (!isValidPathSegment(segment))
            return false;
    }
    return true;
}

// https://wicg.github.io/entries-api/#relative-path
static bool isValidRelativeVirtualPath(StringView virtualPath)
{
    if (virtualPath.isEmpty())
        return false;

    if (virtualPath[0] == '/')
        return false;

    return isZeroOrMorePathSegmentsSeparatedBySlashes(virtualPath);
}

// https://wicg.github.io/entries-api/#valid-path
static bool isValidVirtualPath(StringView virtualPath)
{
    if (virtualPath.isEmpty())
        return true;
    if (virtualPath[0] == '/') {
        // An absolute path is a string consisting of '/' (U+002F SOLIDUS) followed by one or more path segments joined by '/' (U+002F SOLIDUS).
        return isZeroOrMorePathSegmentsSeparatedBySlashes(virtualPath.substring(1));
    }
    return isValidRelativeVirtualPath(virtualPath);
}

DOMFileSystem::DOMFileSystem(Ref<File>&& file)
    : m_name(createCanonicalUUIDString())
    , m_file(WTFMove(file))
    , m_rootPath(FileSystem::directoryName(m_file->path()))
    , m_workQueue(WorkQueue::create("DOMFileSystem work queue"))
{
    ASSERT(!m_rootPath.endsWith('/'));
}

DOMFileSystem::~DOMFileSystem() = default;

Ref<FileSystemDirectoryEntry> DOMFileSystem::root(ScriptExecutionContext& context)
{
    return FileSystemDirectoryEntry::create(context, *this, "/"_s);
}

Ref<FileSystemEntry> DOMFileSystem::fileAsEntry(ScriptExecutionContext& context)
{
    if (m_file->isDirectory())
        return FileSystemDirectoryEntry::create(context, *this, "/" + m_file->name());
    return FileSystemFileEntry::create(context, *this, "/" + m_file->name());
}

static ExceptionOr<String> validatePathIsExpectedType(const String& fullPath, String&& virtualPath, FileMetadata::Type expectedType)
{
    ASSERT(!isMainThread());

    auto metadata = FileSystem::fileMetadata(fullPath);
    if (!metadata || metadata.value().isHidden)
        return Exception { NotFoundError, "Path does not exist"_s };

    if (metadata.value().type != expectedType)
        return Exception { TypeMismatchError, "Entry at path does not have expected type" };

    return WTFMove(virtualPath);
}

static Optional<FileMetadata::Type> fileType(const String& fullPath)
{
    auto metadata = FileSystem::fileMetadata(fullPath);
    if (!metadata || metadata.value().isHidden)
        return WTF::nullopt;
    return metadata.value().type;
}

// https://wicg.github.io/entries-api/#resolve-a-relative-path
static String resolveRelativeVirtualPath(StringView baseVirtualPath, StringView relativeVirtualPath)
{
    ASSERT(baseVirtualPath[0] == '/');
    if (!relativeVirtualPath.isEmpty() && relativeVirtualPath[0] == '/')
        return relativeVirtualPath.length() == 1 ? relativeVirtualPath.toString() : resolveRelativeVirtualPath("/", relativeVirtualPath.substring(1));

    Vector<StringView> virtualPathSegments;
    for (auto segment : baseVirtualPath.split('/'))
        virtualPathSegments.append(segment);

    for (auto segment : relativeVirtualPath.split('/')) {
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
        return "/"_s;

    StringBuilder builder;
    for (auto& segment : virtualPathSegments) {
        builder.append('/');
        builder.append(segment);
    }
    return builder.toString();
}

// https://wicg.github.io/entries-api/#evaluate-a-path
String DOMFileSystem::evaluatePath(StringView virtualPath)
{
    ASSERT(virtualPath[0] == '/');

    Vector<StringView> resolvedComponents;
    for (auto component : virtualPath.split('/')) {
        if (component == ".")
            continue;
        if (component == "..") {
            if (!resolvedComponents.isEmpty())
                resolvedComponents.removeLast();
            continue;
        }
        resolvedComponents.append(component);
    }

    return FileSystem::pathByAppendingComponents(m_rootPath, resolvedComponents);
}

void DOMFileSystem::listDirectory(ScriptExecutionContext& context, FileSystemDirectoryEntry& directory, DirectoryListingCallback&& completionHandler)
{
    ASSERT(&directory.filesystem() == this);

    auto directoryVirtualPath = directory.virtualPath();
    auto fullPath = evaluatePath(directoryVirtualPath);
    if (fullPath == m_rootPath) {
        Vector<Ref<FileSystemEntry>> children;
        children.append(fileAsEntry(context));
        completionHandler(WTFMove(children));
        return;
    }

    m_workQueue->dispatch([this, context = makeRef(context), completionHandler = WTFMove(completionHandler), fullPath = crossThreadCopy(fullPath), directoryVirtualPath = crossThreadCopy(directoryVirtualPath)]() mutable {
        auto listedChildren = listDirectoryWithMetadata(fullPath);
        callOnMainThread([this, context = WTFMove(context), completionHandler = WTFMove(completionHandler), listedChildren = crossThreadCopy(listedChildren), directoryVirtualPath = directoryVirtualPath.isolatedCopy()]() mutable {
            completionHandler(toFileSystemEntries(context, *this, WTFMove(listedChildren), directoryVirtualPath));
        });
    });
}

void DOMFileSystem::getParent(ScriptExecutionContext& context, FileSystemEntry& entry, GetParentCallback&& completionCallback)
{
    ASSERT(&entry.filesystem() == this);

    auto virtualPath = resolveRelativeVirtualPath(entry.virtualPath(), "..");
    ASSERT(virtualPath[0] == '/');
    auto fullPath = evaluatePath(virtualPath);
    m_workQueue->dispatch([this, context = makeRef(context), fullPath = crossThreadCopy(fullPath), virtualPath = crossThreadCopy(virtualPath), completionCallback = WTFMove(completionCallback)]() mutable {
        auto validatedVirtualPath = validatePathIsExpectedType(fullPath, WTFMove(virtualPath), FileMetadata::Type::Directory);
        callOnMainThread([this, context = WTFMove(context), validatedVirtualPath = crossThreadCopy(validatedVirtualPath), completionCallback = WTFMove(completionCallback)]() mutable {
            if (validatedVirtualPath.hasException())
                completionCallback(validatedVirtualPath.releaseException());
            else
                completionCallback(FileSystemDirectoryEntry::create(context, *this, validatedVirtualPath.releaseReturnValue()));
        });
    });
}

// https://wicg.github.io/entries-api/#dom-filesystemdirectoryentry-getfile
// https://wicg.github.io/entries-api/#dom-filesystemdirectoryentry-getdirectory
void DOMFileSystem::getEntry(ScriptExecutionContext& context, FileSystemDirectoryEntry& directory, const String& virtualPath, const FileSystemDirectoryEntry::Flags& flags, GetEntryCallback&& completionCallback)
{
    ASSERT(&directory.filesystem() == this);

    if (!isValidVirtualPath(virtualPath)) {
        callOnMainThread([completionCallback = WTFMove(completionCallback)] {
            completionCallback(Exception { TypeMismatchError, "Path is invalid"_s });
        });
        return;
    }

    if (flags.create) {
        callOnMainThread([completionCallback = WTFMove(completionCallback)] {
            completionCallback(Exception { SecurityError, "create flag cannot be true"_s });
        });
        return;
    }

    auto resolvedVirtualPath = resolveRelativeVirtualPath(directory.virtualPath(), virtualPath);
    ASSERT(resolvedVirtualPath[0] == '/');
    auto fullPath = evaluatePath(resolvedVirtualPath);
    if (fullPath == m_rootPath) {
        callOnMainThread([this, context = makeRef(context), completionCallback = WTFMove(completionCallback)]() mutable {
            completionCallback(Ref<FileSystemEntry> { root(context) });
        });
        return;
    }

    m_workQueue->dispatch([this, context = makeRef(context), fullPath = crossThreadCopy(fullPath), resolvedVirtualPath = crossThreadCopy(resolvedVirtualPath), completionCallback = WTFMove(completionCallback)]() mutable {
        auto entryType = fileType(fullPath);
        callOnMainThread([this, context = WTFMove(context), resolvedVirtualPath = crossThreadCopy(resolvedVirtualPath), entryType, completionCallback = WTFMove(completionCallback)]() mutable {
            if (!entryType) {
                completionCallback(Exception { NotFoundError, "Cannot find entry at given path"_s });
                return;
            }
            switch (entryType.value()) {
            case FileMetadata::Type::Directory:
                completionCallback(Ref<FileSystemEntry> { FileSystemDirectoryEntry::create(context, *this, resolvedVirtualPath) });
                break;
            case FileMetadata::Type::File:
                completionCallback(Ref<FileSystemEntry> { FileSystemFileEntry::create(context, *this, resolvedVirtualPath) });
                break;
            default:
                completionCallback(Exception { NotFoundError, "Cannot find entry at given path"_s });
                break;
            }
        });
    });
}

void DOMFileSystem::getFile(ScriptExecutionContext& context, FileSystemFileEntry& fileEntry, GetFileCallback&& completionCallback)
{
    auto virtualPath = fileEntry.virtualPath();
    auto fullPath = evaluatePath(virtualPath);
    m_workQueue->dispatch([fullPath = crossThreadCopy(fullPath), virtualPath = crossThreadCopy(virtualPath), context = makeRef(context), completionCallback = WTFMove(completionCallback)]() mutable {
        auto validatedVirtualPath = validatePathIsExpectedType(fullPath, WTFMove(virtualPath), FileMetadata::Type::File);
        callOnMainThread([fullPath = crossThreadCopy(fullPath), validatedVirtualPath = crossThreadCopy(validatedVirtualPath), context = WTFMove(context), completionCallback = WTFMove(completionCallback)]() mutable {
            if (validatedVirtualPath.hasException())
                completionCallback(validatedVirtualPath.releaseException());
            else
                completionCallback(File::create(context.ptr(), fullPath));
        });
    });
}

} // namespace WebCore
