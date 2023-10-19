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

#include "config.h"
#include "DataTransferItem.h"

#include "DOMFileSystem.h"
#include "DataTransferItemList.h"
#include "Document.h"
#include "File.h"
#include "FileSystemDirectoryEntry.h"
#include "FileSystemFileEntry.h"
#include "ScriptExecutionContext.h"
#include "StringCallback.h"
#include <wtf/FileSystem.h>

namespace WebCore {

Ref<DataTransferItem> DataTransferItem::create(WeakPtr<DataTransferItemList>&& list, const String& type)
{
    return adoptRef(*new DataTransferItem(WTFMove(list), type));
}

Ref<DataTransferItem> DataTransferItem::create(WeakPtr<DataTransferItemList>&& list, const String& type, Ref<File>&& file)
{
    return adoptRef(*new DataTransferItem(WTFMove(list), type, WTFMove(file)));
}

DataTransferItem::DataTransferItem(WeakPtr<DataTransferItemList>&& list, const String& type)
    : m_list(WTFMove(list))
    , m_type(type)
{
}

DataTransferItem::DataTransferItem(WeakPtr<DataTransferItemList>&& list, const String& type, Ref<File>&& file)
    : m_list(WTFMove(list))
    , m_type(type)
    , m_file(WTFMove(file))
{
}

DataTransferItem::~DataTransferItem() = default;

void DataTransferItem::clearListAndPutIntoDisabledMode()
{
    m_list.clear();
}

String DataTransferItem::kind() const
{
    return m_file ? "file"_s : "string"_s;
}

String DataTransferItem::type() const
{
    return isInDisabledMode() ? String() : m_type;
}

void DataTransferItem::getAsString(Document& document, RefPtr<StringCallback>&& callback) const
{
    if (!callback || !m_list || m_file)
        return;

    Ref dataTransfer = m_list->dataTransfer();
    if (!dataTransfer->canReadData())
        return;

    // FIXME: Make this async.
    callback->scheduleCallback(document, dataTransfer->getDataForItem(document, m_type));
}

RefPtr<File> DataTransferItem::getAsFile() const
{
    if (!m_list || !m_list->dataTransfer().canReadData())
        return nullptr;
    return m_file.copyRef();
}

RefPtr<FileSystemEntry> DataTransferItem::getAsEntry(ScriptExecutionContext& context) const
{
    auto file = getAsFile();
    if (!file)
        return nullptr;

    return DOMFileSystem::createEntryForFile(context, *file);
}

} // namespace WebCore
