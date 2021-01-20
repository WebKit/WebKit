/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include "DataTransfer.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class DOMFileSystem;
class DataTransferListItem;
class File;
class FileSystemEntry;
class ScriptExecutionContext;
class StringCallback;

class DataTransferItem : public RefCounted<DataTransferItem> {
public:
    static Ref<DataTransferItem> create(WeakPtr<DataTransferItemList>&&, const String&);
    static Ref<DataTransferItem> create(WeakPtr<DataTransferItemList>&&, const String&, Ref<File>&&);

    ~DataTransferItem();

    RefPtr<File> file() { return m_file; }
    void clearListAndPutIntoDisabledMode();

    bool isFile() const { return m_file; }
    String kind() const;
    String type() const;
    void getAsString(Document&, RefPtr<StringCallback>&&) const;
    RefPtr<File> getAsFile() const;
    RefPtr<FileSystemEntry> getAsEntry(ScriptExecutionContext&) const;

private:
    DataTransferItem(WeakPtr<DataTransferItemList>&&, const String&);
    DataTransferItem(WeakPtr<DataTransferItemList>&&, const String&, Ref<File>&&);

    bool isInDisabledMode() const { return !m_list; }

    WeakPtr<DataTransferItemList> m_list;
    const String m_type;
    RefPtr<File> m_file;
};

}
