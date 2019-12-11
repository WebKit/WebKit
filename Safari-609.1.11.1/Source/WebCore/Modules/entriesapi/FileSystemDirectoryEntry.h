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

#include "FileSystemEntry.h"

namespace WebCore {

class ErrorCallback;
class FileSystemDirectoryReader;
class FileSystemEntryCallback;
class ScriptExecutionContext;

class FileSystemDirectoryEntry final : public FileSystemEntry {
public:
    static Ref<FileSystemDirectoryEntry> create(ScriptExecutionContext& context, DOMFileSystem& filesystem, const String& virtualPath)
    {
        return adoptRef(*new FileSystemDirectoryEntry(context, filesystem, virtualPath));
    }

    Ref<FileSystemDirectoryReader> createReader(ScriptExecutionContext&);

    struct Flags {
        bool create { false };
        bool exclusive { false };
    };

    void getFile(ScriptExecutionContext&, const String& path, const Flags& options, RefPtr<FileSystemEntryCallback>&&, RefPtr<ErrorCallback>&&);
    void getDirectory(ScriptExecutionContext&, const String& path, const Flags& options, RefPtr<FileSystemEntryCallback>&&, RefPtr<ErrorCallback>&&);

private:
    bool isDirectory() const final { return true; }
    using EntryMatchingFunction = WTF::Function<bool(const FileSystemEntry&)>;
    void getEntry(ScriptExecutionContext&, const String& path, const Flags& options, EntryMatchingFunction&&, RefPtr<FileSystemEntryCallback>&&, RefPtr<ErrorCallback>&&);

    FileSystemDirectoryEntry(ScriptExecutionContext&, DOMFileSystem&, const String& virtualPath);
};
static_assert(sizeof(FileSystemDirectoryEntry) == sizeof(FileSystemEntry), "");

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::FileSystemDirectoryEntry)
    static bool isType(const WebCore::FileSystemEntry& entry) { return entry.isDirectory(); }
SPECIALIZE_TYPE_TRAITS_END()
