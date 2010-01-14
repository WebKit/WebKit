/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "WebString.h"
#include "WebVector.h"

namespace WebKit {

struct WebFileChooserParams {
    // If |multiSelect| is true, the dialog allow to select multiple files.
    bool multiSelect;
    // |title| is a title of a file chooser dialog. It can be an empty string.
    WebString title;
    // |initialValue| is a filename which the dialog should select by default.
    // It can be an empty string.
    WebString initialValue;
    // |acceptTypes| has a comma-separated MIME types such as "audio/*,text/plain".
    // The dialog may restrict selectable files to the specified MIME types.
    // This value comes from an 'accept' attribute value of an INPUT element.
    // So it might be a wrong formatted string.
    WebString acceptTypes;
    // |selectedFiles| has filenames which a file upload control already select.
    // A WebViewClient implementation may ask a user to select
    //  - removing a file from the selected files,
    //  - appending other files, or
    //  - replacing with other files
    // before opening a file chooser dialog.
    WebVector<WebString> selectedFiles;
};

} // namespace WebKit

#endif
