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
#include <wtf/FileSystem.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

DirectoryFileListCreator::~DirectoryFileListCreator()
{
    ASSERT(!m_completionHandler);
}

struct FileInformation {
    RefPtr<StringImpl> path;
    RefPtr<StringImpl> relativePath;
    RefPtr<StringImpl> displayName;

    FileInformation isolatedCopy() const & { return { WTF::isolatedCopy(path), WTF::isolatedCopy(relativePath), WTF::isolatedCopy(displayName) }; }
    FileInformation isolatedCopy() && { return { WTF::isolatedCopy(WTF::move(path)), WTF::isolatedCopy(relativePath), WTF::isolatedCopy(WTF::move(displayName)) }; }
};

static void appendDirectoryFiles(StringImpl* directory, StringImpl* relativePath, Vector<FileInformation>& files)
{
    ASSERT(!isMainThread());
    FileSystem::traverseDirectory(directory, [&](StringImpl* childName, FileSystem::FileType fileType) {
        auto childPath = FileSystem::pathByAppendingComponent(directory, childName);
        if (FileSystem::isHiddenFile(childPath))
            return;

        auto childRelativePath = makeString(relativePath, '/', childName);
        if (fileType == FileSystem::FileType::Directory)
            appendDirectoryFiles(childPath, childRelativePath, files);
        else if (fileType == FileSystem::FileType::Regular)
            files.append(FileInformation { childPath, childRelativePath, { } });
    });
}

static Vector<FileInformation> gatherFileInformation(const Vector<FileChooserFileInfo>& paths)
{
    ASSERT(!isMainThread());
    Vector<FileInformation> files;
    for (auto& info : paths) {
        if (FileSystem::fileType(info.path) == FileSystem::FileType::Directory)
            appendDirectoryFiles(info.path, FileSystem::pathFileName(info.path), files);
        else
            files.append(FileInformation { info.path, { }, info.displayName });
    }
    return files;
}

static Ref<FileList> toFileList(Document* document, const Vector<FileInformation>& files)
{
    ASSERT(isMainThread());
    auto fileObjects = files.map([document](auto& file) {
        if (!file.relativePath)
            return File::create(document, file.path, nullptr, file.displayName);
        return File::createWithRelativePath(document, RefPtr<StringImpl> { file.path }, RefPtr<StringImpl> { file.relativePath });
    });
    return FileList::create(WTF::move(fileObjects));
}

DirectoryFileListCreator::DirectoryFileListCreator(CompletionHandler&& completionHandler)
    : m_workQueue(WorkQueue::create("DirectoryFileListCreator Work Queue"_s))
    , m_completionHandler(WTF::move(completionHandler))
{
}

void DirectoryFileListCreator::start(Document* document, const Vector<FileChooserFileInfo>& paths)
{
    // Resolve directories on a background thread to avoid blocking the main thread.
    protect(m_workQueue)->dispatch([this, protectedThis = Ref { *this }, document = RefPtr { document }, paths = crossThreadCopy(paths)]() mutable {
        auto files = gatherFileInformation(paths);
        callOnMainThread([this, protectedThis = WTF::move(protectedThis), document = WTF::move(document), files = crossThreadCopy(files)]() mutable {
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
