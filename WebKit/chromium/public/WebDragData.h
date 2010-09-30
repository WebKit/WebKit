/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebDragData_h
#define WebDragData_h

#include "WebCommon.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class ChromiumDataObject; }
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {

class WebData;
class WebDragDataPrivate;
class WebString;
class WebURL;
template <typename T> class WebVector;

// Holds data that may be exchanged through a drag-n-drop operation.  It is
// inexpensive to copy a WebDragData object.
class WebDragData {
public:
    ~WebDragData() { reset(); }

    WebDragData() : m_private(0) { }
    WebDragData(const WebDragData& d) : m_private(0) { assign(d); }
    WebDragData& operator=(const WebDragData& d)
    {
        assign(d);
        return *this;
    }

    WEBKIT_API void initialize();
    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebDragData&);

    bool isNull() const { return !m_private; }

    WEBKIT_API WebString url() const;
    WEBKIT_API void setURL(const WebURL&);

    WEBKIT_API WebString urlTitle() const;
    WEBKIT_API void setURLTitle(const WebString&);

    WEBKIT_API WebString downloadMetadata() const;
    WEBKIT_API void setDownloadMetadata(const WebString&);

    WEBKIT_API WebString fileExtension() const;
    WEBKIT_API void setFileExtension(const WebString&);

    WEBKIT_API bool hasFileNames() const;
    WEBKIT_API void fileNames(WebVector<WebString>&) const;
    WEBKIT_API void setFileNames(const WebVector<WebString>&);
    WEBKIT_API void appendToFileNames(const WebString&);

    WEBKIT_API WebString plainText() const;
    WEBKIT_API void setPlainText(const WebString&);

    WEBKIT_API WebString htmlText() const;
    WEBKIT_API void setHTMLText(const WebString&);

    WEBKIT_API WebURL htmlBaseURL() const;
    WEBKIT_API void setHTMLBaseURL(const WebURL&);

    WEBKIT_API WebString fileContentFileName() const;
    WEBKIT_API void setFileContentFileName(const WebString&);

    WEBKIT_API WebData fileContent() const;
    WEBKIT_API void setFileContent(const WebData&);

#if WEBKIT_IMPLEMENTATION
    WebDragData(const WTF::PassRefPtr<WebCore::ChromiumDataObject>&);
    WebDragData& operator=(const WTF::PassRefPtr<WebCore::ChromiumDataObject>&);
    operator WTF::PassRefPtr<WebCore::ChromiumDataObject>() const;
#endif

private:
    void assign(WebDragDataPrivate*);
    void ensureMutable();
    WebDragDataPrivate* m_private;
};

} // namespace WebKit

#endif
