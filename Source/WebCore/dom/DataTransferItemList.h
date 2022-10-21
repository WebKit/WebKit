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

#include "ContextDestructionObserver.h"
#include "DataTransfer.h"
#include "ExceptionOr.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DataTransferItem;
class Document;
class File;

class DataTransferItemList final : public ScriptWrappable, public ContextDestructionObserver, public CanMakeWeakPtr<DataTransferItemList> {
    WTF_MAKE_NONCOPYABLE(DataTransferItemList);
    WTF_MAKE_ISO_ALLOCATED(DataTransferItemList);
public:
    DataTransferItemList(Document&, DataTransfer&);
    ~DataTransferItemList();

    // DataTransfer owns DataTransferItemList, and DataTransfer is kept alive as long as DataTransferItemList is alive.
    void ref() { m_dataTransfer.ref(); }
    void deref() { m_dataTransfer.deref(); }
    DataTransfer& dataTransfer() { return m_dataTransfer; }

    // DOM API
    unsigned length() const;
    RefPtr<DataTransferItem> item(unsigned index);
    ExceptionOr<RefPtr<DataTransferItem>> add(Document&, const String& data, const String& type);
    RefPtr<DataTransferItem> add(Ref<File>&&);
    ExceptionOr<void> remove(unsigned index);
    void clear();

    void didClearStringData(const String& type);
    void didSetStringData(const String& type);
    bool hasItems() const { return m_items.has_value(); }
    const Vector<Ref<DataTransferItem>>& items() const
    {
        ASSERT(m_items);
        return *m_items;
    }

private:
    Vector<Ref<DataTransferItem>>& ensureItems() const;
    Document* document() const;

    DataTransfer& m_dataTransfer;
    mutable std::optional<Vector<Ref<DataTransferItem>>> m_items;
};

} // namespace WebCore
