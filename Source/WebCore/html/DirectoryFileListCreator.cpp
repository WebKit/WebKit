/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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
#include "DirectoryFileListCreator.h"

#include "Document.h"
#include "FileChooser.h"
#include "FileList.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/FileMetadata.h>
#include <wtf/FileSystem.h>

namespace WebCore {

DirectoryFileListCreator::~DirectoryFileListCreator()
{
    ASSERT(!m_completionHandler);
}

struct FileInformation {
    String path;
    String relativePath;
    String displayName;

    FileInformation isolatedCopy() const
    {
        return FileInformation { path.isolatedCopy(), relativePath.isolatedCopy(), displayName.isolatedCopy() };
    }
};

static void appendDirectoryFiles(const String& directory, const String& relativePath, Vector<FileInformation>& files)
{
    ASSERT(!isMainThread());
    for (auto& childPath : FileSystem::listDirectory(directory, "*")) {
        auto metadata = FileSystem::fileMetadata(childPath);
        if (!metadata)
            continue;

        if (metadata.value().isHidden)
            continue;

        String childRelativePath = relativePath + "/" + FileSystem::pathGetFileName(childPath);
        if (metadata.value().type == FileMetadata::Type::Directory)
            appendDirectoryFiles(childPath, childRelativePath, files);
        else if (metadata.value().type == FileMetadata::Type::File)
            files.append(FileInformation { childPath, childRelativePath, { } });
    }
}

static Vector<FileInformation> gatherFileInformation(const Vector<FileChooserFileInfo>& paths)
{
    ASSERT(!isMainThread());
    Vector<FileInformation> files;
    for (auto& info : paths) {
        if (FileSystem::fileIsDirectory(info.path, FileSystem::ShouldFollowSymbolicLinks::No))
            appendDirectoryFiles(info.path, FileSystem::pathGetFileName(info.path), files);
        else
            files.append(FileInformation { info.path, { }, info.displayName });
    }
    return files;
}

static Ref<FileList> toFileList(Document* document, const Vector<FileInformation>& files)
{
    ASSERT(isMainThread());
    Vector<Ref<File>> fileObjects;
    for (auto& file : files) {
        if (file.relativePath.isNull())
            fileObjects.append(File::create(document, file.path, { }, file.displayName));
        else
            fileObjects.append(File::createWithRelativePath(document, file.path, file.relativePath));
    }
    return FileList::create(WTFMove(fileObjects));
}

DirectoryFileListCreator::DirectoryFileListCreator(CompletionHandler&& completionHandler)
    : m_workQueue(WorkQueue::create("DirectoryFileListCreator Work Queue"))
    , m_completionHandler(WTFMove(completionHandler))
{
}

void DirectoryFileListCreator::start(Document* document, const Vector<FileChooserFileInfo>& paths)
{
    // Resolve directories on a background thread to avoid blocking the main thread.
    m_workQueue->dispatch([this, protectedThis = makeRef(*this), document = makeRefPtr(document), paths = crossThreadCopy(paths)]() mutable {
        auto files = gatherFileInformation(paths);
        callOnMainThread([this, protectedThis = WTFMove(protectedThis), document = WTFMove(document), files = crossThreadCopy(files)]() mutable {
            if (auto completionHandler = std::exchange(m_completionHandler, nullptr))
                completionHandler(toFileList(document.get(), files));
        });
    });
}

void DirectoryFileListCreator::cancel()
{
    m_completionHandler = nullptr;
    m_workQueue = nullptr;
}

} // namespace WebCore
