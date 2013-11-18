/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PageLoadState_h
#define PageLoadState_h

#include <wtf/text/WTFString.h>

namespace WebKit {

class PageLoadState {
public:
    PageLoadState();
    ~PageLoadState();

    enum class State {
        Provisional,
        Committed,
        Finished
    };

    const String& pendingAPIRequestURL() const;
    void setPendingAPIRequestURL(const String&);
    void clearPendingAPIRequestURL();

    void didStartProvisionalLoad(const String& url, const String& unreachableURL);
    void didReceiveServerRedirectForProvisionalLoad(const String& url);
    void didFailProvisionalLoad();

    void didCommitLoad();
    void didFinishLoad();
    void didFailLoad();

    void didSameDocumentNavigation(const String& url);

private:
    State m_state;

    String m_pendingAPIRequestURL;

    String m_provisionalURL;
    String m_url;
};

} // namespace WebKit

#endif // PageLoadState_h
