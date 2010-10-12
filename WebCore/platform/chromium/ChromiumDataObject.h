/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#include "ChromiumDataObjectLegacy.h"
#include "ReadableDataObject.h"
#include "WritableDataObject.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class ChromiumDataObject : public RefCounted<ChromiumDataObject> {
public:
    static PassRefPtr<ChromiumDataObject> create(PassRefPtr<ChromiumDataObjectLegacy> data);
    static PassRefPtr<ChromiumDataObject> createReadable(Clipboard::ClipboardType);
    static PassRefPtr<ChromiumDataObject> createWritable(Clipboard::ClipboardType);

    void clearData(const String& type);
    void clearAll();
    void clearAllExceptFiles();

    bool hasData() const;

    HashSet<String> types() const;
    String getData(const String& type, bool& success);
    bool setData(const String& type, const String& data);

    // Special handlers for URL/HTML metadata.
    String urlTitle() const;
    void setUrlTitle(const String& urlTitle);
    KURL htmlBaseUrl() const;
    void setHtmlBaseUrl(const KURL& url);

    // Used to handle files being dragged in.
    bool containsFilenames() const;
    Vector<String> filenames() const;
    void setFilenames(const Vector<String>& filenames);

    // Used to handle files (images) being dragged out.
    String fileExtension() const;
    void setFileExtension(const String& fileExtension);
    String fileContentFilename() const;
    void setFileContentFilename(const String& fileContentFilename);
    PassRefPtr<SharedBuffer> fileContent() const;
    void setFileContent(PassRefPtr<SharedBuffer> fileContent);

private:
    ChromiumDataObject(PassRefPtr<ChromiumDataObjectLegacy>);
    ChromiumDataObject(PassRefPtr<ReadableDataObject>);
    ChromiumDataObject(PassRefPtr<WritableDataObject>);

    RefPtr<ChromiumDataObjectLegacy> m_legacyData;
    RefPtr<ReadableDataObject> m_readableData;
    RefPtr<WritableDataObject> m_writableData;
};

}

#endif
