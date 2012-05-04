/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
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

#ifndef ChromiumDataObject_h
#define ChromiumDataObject_h

#include "ChromiumDataObjectItem.h"
#include "PlatformString.h"
#include "Supplementable.h"
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class KURL;
class SharedBuffer;

typedef int ExceptionCode;

// A data object for holding data that would be in a clipboard or moved
// during a drag-n-drop operation.  This is the data that WebCore is aware
// of and is not specific to a platform.
class ChromiumDataObject : public RefCounted<ChromiumDataObject>, public Supplementable<ChromiumDataObject> {
public:
    static PassRefPtr<ChromiumDataObject> createFromPasteboard();
    static PassRefPtr<ChromiumDataObject> create();

    PassRefPtr<ChromiumDataObject> copy() const;

    // DataTransferItemList support.
    size_t length() const;
    PassRefPtr<ChromiumDataObjectItem> item(unsigned long index);
    // FIXME: Implement V8DataTransferItemList::indexedPropertyDeleter to get this called.
    void deleteItem(unsigned long index);
    void clearAll();
    void add(const String& data, const String& type, ExceptionCode&);
    void add(PassRefPtr<File>, ScriptExecutionContext*);

    // WebCore helpers.
    void clearData(const String& type);
    void clearAllExceptFiles();

    HashSet<String> types() const;
    String getData(const String& type) const;
    bool setData(const String& type, const String& data);

    void urlAndTitle(String& url, String* title = 0) const;
    void setURLAndTitle(const String& url, const String& title);
    void htmlAndBaseURL(String& html, KURL& baseURL) const;
    void setHTMLAndBaseURL(const String& html, const KURL& baseURL);

    // Used for dragging in files from the desktop.
    bool containsFilenames() const;
    Vector<String> filenames() const;
    void addFilename(const String& filename, const String& displayName);

    // Used to handle files (images) being dragged out.
    void addSharedBuffer(const String& name, PassRefPtr<SharedBuffer>);

private:
    ChromiumDataObject();
    explicit ChromiumDataObject(const ChromiumDataObject&);

    PassRefPtr<ChromiumDataObjectItem> findStringItem(const String& type) const;
    bool internalAddStringItem(PassRefPtr<ChromiumDataObjectItem>);
    void internalAddFileItem(PassRefPtr<ChromiumDataObjectItem>);

    Vector<RefPtr<ChromiumDataObjectItem> > m_itemList;
};

} // namespace WebCore

#endif
