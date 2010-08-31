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

#ifndef ChromiumDataObjectNew_h
#define ChromiumDataObjectNew_h

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
class ChromiumDataObjectNew : public RefCounted<ChromiumDataObjectNew> {
public:
    virtual void clearData(const String& type) = 0;
    virtual void clearAllExceptFiles() = 0;
    virtual void clear() = 0;
    virtual bool hasData() const = 0;
    virtual HashSet<String> types() const = 0;
    virtual String getData(const String& type, bool& succeeded) const = 0;
    virtual bool setData(const String& type, const String& data) = 0;

    // Special accessors for URL and HTML since they carry additional
    // metadata.
    virtual String getURL(String* title) const = 0;
    virtual void setURL(const String& url, const String& title) = 0;
    virtual String getHTML(String* baseURL) const = 0;
    virtual void setHTML(const String& html, const KURL& baseURL) = 0;

    virtual bool hasFilenames() const = 0;
    virtual Vector<String> filenames() const = 0;

    // Accessors used when transferring drag data from the renderer to the
    // browser.
    virtual HashMap<String, String> dataMap() const = 0;
    virtual String urlTitle() const = 0;
    virtual KURL htmlBaseURL() const = 0;

    // Used for transferring file data from the renderer to the browser.
    virtual String fileExtension() const = 0;
    virtual String fileContentFilename() const = 0;
    virtual PassRefPtr<SharedBuffer> fileContent() const = 0;
    virtual void setFileExtension(const String&) = 0;
    virtual void setFileContentFilename(const String&) = 0;
    virtual void setFileContent(PassRefPtr<SharedBuffer>) = 0;
};

} // namespace WebCore

#endif
