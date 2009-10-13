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
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

    // A data object for holding data that would be in a clipboard or moved
    // during a drag-n-drop operation.  This is the data that WebCore is aware
    // of and is not specific to a platform.
    class ChromiumDataObject : public RefCounted<ChromiumDataObject> {
    public:
        static PassRefPtr<ChromiumDataObject> create()
        {
            return adoptRef(new ChromiumDataObject);
        }

        PassRefPtr<ChromiumDataObject> copy() const
        {
            return adoptRef(new ChromiumDataObject(*this));
        }

        void clear();
        bool hasData() const;

        bool containsMainURL() const { return !url.isEmpty(); }
        bool containsValidMainURL() const { return url.isValid(); }
        KURL mainURL() const { return url; }
        void setMainURL(const KURL& newURL) { url = newURL; }

        bool containsMainURLTitle() const { return !urlTitle.isEmpty(); }
        String mainURLTitle() const { return urlTitle; }
        void setMainURLTitle(const String& newURLTitle) { urlTitle = newURLTitle; }

        bool containsTextPlain() const { return !plainText.isEmpty(); }
        String textPlain() const { return plainText; }
        void setTextPlain(const String& newText) { plainText = newText; }

        bool containsTextHTML() const { return !textHtml.isEmpty(); }
        String textHTML() const { return textHtml; }
        void setTextHTML(const String& newText) { textHtml = newText; }

        bool containsHTMLBaseURL() const { return !htmlBaseUrl.isEmpty(); }
        bool containsValidHTMLBaseURL() const { return htmlBaseUrl.isValid(); }
        KURL htmlBaseURL() const { return htmlBaseUrl; }
        void setHTMLBaseURL(const KURL& newURL) { htmlBaseUrl = newURL; }

        bool containsContent() const { return fileContent; }
        SharedBuffer* content() const { return fileContent.get(); }
        PassRefPtr<SharedBuffer> releaseContent() { return fileContent.release(); }
        void setContent(PassRefPtr<SharedBuffer> newContent) { fileContent = newContent; }

        bool containsContentFileExtension() const { return !fileExtension.isEmpty(); }
        String contentFileExtension() const { return fileExtension; }
        void setContentFileExtension(const String& newFileExtension) { fileExtension = newFileExtension; }

        bool containsContentFileName() const { return !fileContentFilename.isEmpty(); }
        String contentFileName() const { return fileContentFilename; }
        void setContentFileName(const String& newFileName) { fileContentFilename = newFileName; }

        bool containsFileNames() const { return !filenames.isEmpty(); }
        Vector<String> fileNames() const { return filenames; }
        void clearFileNames() { filenames.clear(); }
        size_t countFileNames() const { return filenames.size(); }
        String fileNameAt(size_t index) { return filenames.at(index); }
        void setFileNames(const Vector<String>& newFileNames) { filenames = newFileNames; }
        void appendToFileNames(const String& newFileName)
        {
            ASSERT(!newFileName.isEmpty());
            filenames.append(newFileName);
        }
        String popFileName() {
            if (filenames.isEmpty())
                return String();
            String fileName;
            fileName = filenames.at(0);
            filenames.remove(0);
            return fileName;
        }

        // Interim state: All members will become private, do NOT access them directly! 
        // Rather use the above accessor methods (or devise new ones if necessary).
        KURL url;
        String urlTitle;

        String fileExtension;
        Vector<String> filenames;

        String plainText;

        String textHtml;
        KURL htmlBaseUrl;

        String fileContentFilename;
        RefPtr<SharedBuffer> fileContent;

    private:
        ChromiumDataObject() {}
        ChromiumDataObject(const ChromiumDataObject&);
    };

} // namespace WebCore

#endif
