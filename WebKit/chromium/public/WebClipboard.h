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

#ifndef WebClipboard_h
#define WebClipboard_h

#include "WebCommon.h"

namespace WebKit {

class WebImage;
class WebString;
class WebURL;

class WebClipboard {
public:
    enum Format {
        FormatHTML,
        FormatBookmark,
        FormatSmartPaste
    };

    enum Buffer {
        BufferStandard,
        // Used on platforms like the X Window System that treat selection
        // as a type of clipboard.
        BufferSelection
    };

    virtual bool isFormatAvailable(Format, Buffer) = 0;

    virtual WebString readPlainText(Buffer) = 0;
    virtual WebString readHTML(Buffer, WebURL*) = 0;

    virtual void writePlainText(const WebString&) = 0;
    virtual void writeHTML(
        const WebString& htmlText, const WebURL&,
        const WebString& plainText, bool writeSmartPaste) = 0;
    virtual void writeURL(
        const WebURL&, const WebString& title) = 0;
    virtual void writeImage(
        const WebImage&, const WebURL&, const WebString& title) = 0;

protected:
    ~WebClipboard() {}
};

} // namespace WebKit

#endif
