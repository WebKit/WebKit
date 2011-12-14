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

#ifndef DataTransferItemChromium_h
#define DataTransferItemChromium_h

#if ENABLE(DATA_TRANSFER_ITEMS)

#include "DataTransferItem.h"
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Clipboard;
class ScriptExecutionContext;

class DataTransferItemChromium : public DataTransferItem {
public:
    static PassRefPtr<DataTransferItemChromium> createFromPasteboard(PassRefPtr<Clipboard> owner, ScriptExecutionContext*, const String& type);
    static PassRefPtr<DataTransferItemChromium> create(PassRefPtr<Clipboard> owner, ScriptExecutionContext*, const String& data, const String& type);

    virtual void getAsString(PassRefPtr<StringCallback>);
    virtual PassRefPtr<Blob> getAsFile();

private:
    enum DataSource {
        PasteboardSource,
        InternalSource,
    };

    DataTransferItemChromium(PassRefPtr<Clipboard> owner, ScriptExecutionContext*, DataSource, const String& kind, const String& type, const String& data);

    ScriptExecutionContext* m_context;
    const DataSource m_source;
    const String m_data;
};

} // namespace WebCore

#endif // ENABLE(DATA_TRANSFER_ITEMS)

#endif // DataTransferItem_h
