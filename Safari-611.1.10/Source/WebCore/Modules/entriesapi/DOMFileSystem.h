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

#pragma once

#include "ExceptionOr.h"
#include "FileSystemDirectoryEntry.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class File;
class FileSystemFileEntry;
class FileSystemEntry;
class ScriptExecutionContext;

class DOMFileSystem final : public ScriptWrappable, public RefCounted<DOMFileSystem> {
    WTF_MAKE_ISO_ALLOCATED(DOMFileSystem);
public:
    static Ref<FileSystemEntry> createEntryForFile(ScriptExecutionContext& context, Ref<File>&& file)
    {
        auto fileSystem = adoptRef(*new DOMFileSystem(WTFMove(file)));
        return fileSystem->fileAsEntry(context);
    }

    ~DOMFileSystem();

    const String& name() const { return m_name; }
    Ref<FileSystemDirectoryEntry> root(ScriptExecutionContext&);

    using DirectoryListingCallback = WTF::Function<void(ExceptionOr<Vector<Ref<FileSystemEntry>>>&&)>;
    void listDirectory(ScriptExecutionContext&, FileSystemDirectoryEntry&, DirectoryListingCallback&&);

    using GetParentCallback = WTF::Function<void(ExceptionOr<Ref<FileSystemDirectoryEntry>>&&)>;
    void getParent(ScriptExecutionContext&, FileSystemEntry&, GetParentCallback&&);

    using GetEntryCallback = WTF::Function<void(ExceptionOr<Ref<FileSystemEntry>>&&)>;
    void getEntry(ScriptExecutionContext&, FileSystemDirectoryEntry&, const String& virtualPath, const FileSystemDirectoryEntry::Flags&, GetEntryCallback&&);

    using GetFileCallback = WTF::Function<void(ExceptionOr<Ref<File>>&&)>;
    void getFile(ScriptExecutionContext&, FileSystemFileEntry&, GetFileCallback&&);

private:
    explicit DOMFileSystem(Ref<File>&&);

    String evaluatePath(StringView virtualPath);
    Ref<FileSystemEntry> fileAsEntry(ScriptExecutionContext&);

    String m_name;
    Ref<File> m_file;
    String m_rootPath;
    Ref<WorkQueue> m_workQueue;
};

} // namespace WebCore
