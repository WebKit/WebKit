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

#ifndef ChromiumDataObjectItem_h 
#define ChromiumDataObjectItem_h

#if ENABLE(DATA_TRANSFER_ITEMS)

#include "File.h"
#include "KURL.h"
#include "SharedBuffer.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Blob;
class ScriptExecutionContext;
class StringCallback;

class ChromiumDataObjectItem : public RefCounted<ChromiumDataObjectItem> {
public:
    static PassRefPtr<ChromiumDataObjectItem> createFromString(const String& type, const String& data);
    static PassRefPtr<ChromiumDataObjectItem> createFromFile(PassRefPtr<File>);
    static PassRefPtr<ChromiumDataObjectItem> createFromURL(const String& url, const String& title);
    static PassRefPtr<ChromiumDataObjectItem> createFromHTML(const String& html, const KURL& baseURL);
    static PassRefPtr<ChromiumDataObjectItem> createFromSharedBuffer(const String& filename, PassRefPtr<SharedBuffer>);
    static PassRefPtr<ChromiumDataObjectItem> createFromPasteboard(const String& type, uint64_t sequenceNumber);

    String kind() const { return m_kind; }
    String type() const { return m_type; }
    void getAsString(PassRefPtr<StringCallback>, ScriptExecutionContext*) const;
    PassRefPtr<Blob> getAsFile() const;

    // Used to support legacy DataTransfer APIs and renderer->browser serialization.
    String internalGetAsString() const;
    PassRefPtr<SharedBuffer> sharedBuffer() const { return m_sharedBuffer; }
    String title() const { return m_title; }
    KURL baseURL() const { return m_baseURL; }
    bool isFilename() const;

private:
    enum DataSource {
        PasteboardSource,
        InternalSource,
    };

    ChromiumDataObjectItem(const String& kind, const String& type);
    ChromiumDataObjectItem(const String& kind, const String& type, uint64_t sequenceNumber);

    DataSource m_source;
    String m_kind;
    String m_type;

    String m_data;
    RefPtr<File> m_file;
    RefPtr<SharedBuffer> m_sharedBuffer;
    // Optional metadata. Currently used for URL, HTML, and dragging files in.
    String m_title;
    KURL m_baseURL;

    uint64_t m_sequenceNumber; // Only valid when m_source == PasteboardSource
};

} // namespace WebCore

#endif // ENABLE(DATA_TRANSFER_ITEMS)

#endif // ChromiumDataObjectItem_h
