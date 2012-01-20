/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef DataTransferItemList_h
#define DataTransferItemList_h

#if ENABLE(DATA_TRANSFER_ITEMS)

#include "DataTransferItem.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Clipboard;
class File;

typedef int ExceptionCode;

class DataTransferItemList : public RefCounted<DataTransferItemList> {
public:
    virtual ~DataTransferItemList() { }

    virtual size_t length();
    virtual PassRefPtr<DataTransferItem> item(unsigned long index);
    // FIXME: Implement V8DataTransferItemList::indexedPropertyDeleter to get this called.
    virtual void deleteItem(unsigned long index, ExceptionCode&);
    virtual void clear();
    virtual void add(const String& data, const String& type, ExceptionCode&);
    virtual void add(PassRefPtr<File>);

protected:
    DataTransferItemList(PassRefPtr<Clipboard>, ScriptExecutionContext*);

protected:
    RefPtr<Clipboard> m_owner;
    // Indirectly owned by our parent.
    ScriptExecutionContext* m_context;
    Vector<RefPtr<DataTransferItem> > m_items;

};

} // namespace WebCore

#endif // ENABLE(DATA_TRANSFER_ITEMS)

#endif // DataTransferItemList_h
