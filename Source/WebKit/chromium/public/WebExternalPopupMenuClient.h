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

#ifndef WebExternalPopupMenuClient_h
#define WebExternalPopupMenuClient_h

#include "platform/WebVector.h"

namespace WebKit {

class WebExternalPopupMenuClient {
public:
    // Should be called when the currently selected item in the popup menu
    // changed. Can be -1 if there is no selection.
    virtual void didChangeSelection(int index) = 0;

    // Should be called when an index has been accepted.
    // Note that it is not safe to access this WebExternalPopupClientMenu after
    // this has been called as it might not be valid anymore.
    virtual void didAcceptIndex(int index) = 0;

    // Should be called when a set of indices have been selected.
    // Note that it is not safe to access this WebExternalPopupClientMenu after
    // this has been called as it might not be valid anymore.
    virtual void didAcceptIndices(const WebVector<int>& indices) = 0;

    // Should be called when the popup menu was discarded (closed without a
    // selection.
    // Note that it is not safe to access this WebExternalPopupClientMenu after
    // this has been called as it might not be valid anymore.
    virtual void didCancel() = 0;
};

} // namespace WebKit

#endif // WebExternalPopupMenuClient_h
