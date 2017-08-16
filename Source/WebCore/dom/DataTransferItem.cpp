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

#include "File.h"
#include "ScriptExecutionContext.h"
#include "StringCallback.h"

namespace WebCore {

DataTransferItem::DataTransferItem(DataTransfer& dataTransfer, const String& type)
    : m_dataTransfer(dataTransfer)
    , m_type(type)
{
}

DataTransferItem::DataTransferItem(DataTransfer& dataTransfer, const String& type, Ref<File>&& file)
    : m_dataTransfer(dataTransfer)
    , m_type(type)
    , m_file(WTFMove(file))
{
}

DataTransferItem::~DataTransferItem()
{
}

String DataTransferItem::kind() const
{
    return m_file ? ASCIILiteral("file") : ASCIILiteral("string");
}

void DataTransferItem::getAsString(ScriptExecutionContext& context, RefPtr<StringCallback>&& callback) const
{
    if (!callback || !m_dataTransfer.canReadData() || m_file)
        return;

    // FIXME: Make this async.
    callback->scheduleCallback(context, m_dataTransfer.getData(m_type));
}

RefPtr<File> DataTransferItem::getAsFile() const
{
    if (!m_dataTransfer.canReadData())
        return nullptr;
    return m_file.copyRef();
}

} // namespace WebCore
