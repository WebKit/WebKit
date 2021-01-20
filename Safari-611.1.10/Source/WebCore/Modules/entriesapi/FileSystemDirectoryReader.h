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

#include "ActiveDOMObject.h"
#include "Exception.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ErrorCallback;
class FileSystemDirectoryEntry;
class FileSystemEntriesCallback;
class ScriptExecutionContext;

class FileSystemDirectoryReader final : public ScriptWrappable, public ActiveDOMObject, public RefCounted<FileSystemDirectoryReader> {
    WTF_MAKE_ISO_ALLOCATED(FileSystemDirectoryReader);
public:
    static Ref<FileSystemDirectoryReader> create(ScriptExecutionContext& context, FileSystemDirectoryEntry& directory)
    {
        return adoptRef(*new FileSystemDirectoryReader(context, directory));
    }

    ~FileSystemDirectoryReader();

    void readEntries(ScriptExecutionContext&, Ref<FileSystemEntriesCallback>&&, RefPtr<ErrorCallback>&&);

private:
    FileSystemDirectoryReader(ScriptExecutionContext&, FileSystemDirectoryEntry&);

    const char* activeDOMObjectName() const final;
    Document* document() const;

    Ref<FileSystemDirectoryEntry> m_directory;
    Optional<Exception> m_error;
    bool m_isReading { false };
    bool m_isDone { false };
};

}
