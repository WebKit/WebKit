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

#include "KURL.h"
#include "PlatformString.h"
#include "SharedBuffer.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

// A data object for holding data that would be in a clipboard or moved
// during a drag-n-drop operation.  This is the data that WebCore is aware
// of and is not specific to a platform.
class ChromiumDataObject : public RefCounted<ChromiumDataObject> {
public:
    enum StorageMode {
        Buffered,
        Pasteboard,
    };

    static PassRefPtr<ChromiumDataObject> createFromPasteboard()
    {
        return adoptRef(new ChromiumDataObject(Pasteboard));
    }

    static PassRefPtr<ChromiumDataObject> create()
    {
        return adoptRef(new ChromiumDataObject(Buffered));
    }

    PassRefPtr<ChromiumDataObject> copy() const
    {
        return adoptRef(new ChromiumDataObject(*this));
    }

    StorageMode storageMode() const { return m_storageMode; }

    void clearData(const String& type);
    void clearAll();
    void clearAllExceptFiles();

    bool hasData() const;

    HashSet<String> types() const;
    String getData(const String& type, bool& success) const;
    bool setData(const String& type, const String& data);

    // Special handlers for URL/HTML metadata.
    String urlTitle() const { return m_urlTitle; }
    void setUrlTitle(const String& urlTitle) { m_urlTitle = urlTitle; }
    KURL htmlBaseUrl() const { return m_htmlBaseUrl; }
    void setHtmlBaseUrl(const KURL& url) { m_htmlBaseUrl = url; }

    // Used to handle files being dragged in.
    bool containsFilenames() const;
    Vector<String> filenames() const { return m_filenames; }
    void setFilenames(const Vector<String>& filenames) { m_filenames = filenames; }

    // Used to handle files (images) being dragged out.
    String fileExtension() const { return m_fileExtension; }
    void setFileExtension(const String& fileExtension) { m_fileExtension = fileExtension; }
    String fileContentFilename() const { return m_fileContentFilename; }
    void setFileContentFilename(const String& fileContentFilename) { m_fileContentFilename = fileContentFilename; }
    PassRefPtr<SharedBuffer> fileContent() const { return m_fileContent; }
    void setFileContent(PassRefPtr<SharedBuffer> fileContent) { m_fileContent = fileContent; }
    const HashMap<String, String>& customData() const { return m_customData; }
    HashMap<String, String>& customData() { return m_customData; }

private:
    explicit ChromiumDataObject(StorageMode);
    explicit ChromiumDataObject(const ChromiumDataObject&);

    StorageMode m_storageMode;

    String m_urlTitle;

    String m_downloadMetadata;

    String m_fileExtension;
    Vector<String> m_filenames;

    String m_plainText;

    String m_textHtml;
    KURL m_htmlBaseUrl;

    String m_fileContentFilename;
    RefPtr<SharedBuffer> m_fileContent;

    HashMap<String, String> m_customData;

    // These two are linked. Setting m_url will set m_uriList to the same
    // string value; setting m_uriList will cause its contents to be parsed
    // according to RFC 2483 and the first URL found will be set in m_url.
    KURL m_url;
    String m_uriList;
};

} // namespace WebCore

#endif
