/*
 * Copyright (C) 2010, 2011 Google Inc. All rights reserved.
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

#ifndef WebFileChooserParams_h
#define WebFileChooserParams_h

#include "WebFileChooserCompletion.h"
#include "platform/WebString.h"
#include "platform/WebVector.h"

namespace WebKit {

struct WebFileChooserParams {
    // If |multiSelect| is true, the dialog allows the user to select multiple files.
    bool multiSelect;
    // If |directory| is true, the dialog allows the user to select a directory.
    bool directory;
    // If |saveAs| is true, the dialog allows the user to select a possibly
    // non-existent file. This can be used for a "Save As" dialog.
    bool saveAs;
    // |title| is the title for a file chooser dialog. It can be an empty string.
    WebString title;
    // |initialValue| is a filename which the dialog should select by default.
    // It can be an empty string.
    WebString initialValue;
    // This contains MIME type strings such as "audio/*" "text/plain".
    // The dialog may restrict selectable files to the specified MIME types.
    // This list comes from an 'accept' attribute value of an INPUT element, and
    // it contains only lower-cased MIME type strings of which format is valid.
    WebVector<WebString> acceptMIMETypes;
    // |selectedFiles| has filenames which a file upload control already selected.
    // A WebViewClient implementation may ask a user to select
    //  - removing a file from the selected files,
    //  - appending other files, or
    //  - replacing with other files
    // before opening a file chooser dialog.
    WebVector<WebString> selectedFiles;
    // See http://www.w3.org/TR/html-media-capture/ for the semantics of the
    // capture attribute. This string will either be empty (meaning the feature
    // is disabled) or one of the following values:
    //  - filesystem (default)
    //  - camera
    //  - camcorder
    //  - microphone
    WebString capture;

    WebFileChooserParams()
        : multiSelect(false)
        , directory(false)
        , saveAs(false)
    {
    }
};

} // namespace WebKit

#endif
