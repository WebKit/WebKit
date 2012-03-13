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

#ifndef WebFileChooserCompletion_h
#define WebFileChooserCompletion_h

#include "platform/WebString.h"

namespace WebKit {

template <typename T> class WebVector;

// Gets called back when WebViewClient finished choosing a file.
class WebFileChooserCompletion {
public:
    struct SelectedFileInfo {
        // The actual path of the selected file.
        WebString path;

        // The display name of the file that is to be exposed as File.name in
        // the DOM layer. If it is empty the base part of the |path| is used.
        WebString displayName;
    };

    // Called with zero or more file names. Zero-lengthed vector means that
    // the user cancelled or that file choosing failed. The callback instance
    // is destroyed when this method is called.
    virtual void didChooseFile(const WebVector<WebString>& fileNames) = 0;

    // Called with zero or more files, given as a vector of SelectedFileInfo.
    // Zero-lengthed vector means that the user cancelled or that file
    // choosing failed. The callback instance is destroyed when this method
    // is called.
    // FIXME: Deprecate either one of the didChooseFile (and rename it to
    // didChooseFile*s*).
    virtual void didChooseFile(const WebVector<SelectedFileInfo>&) { }
protected:
    virtual ~WebFileChooserCompletion() {}
};

} // namespace WebKit

#endif
