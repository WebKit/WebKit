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

#ifndef ExternalPopupMenu_h
#define ExternalPopupMenu_h

#include "PopupMenu.h"
#include "WebExternalPopupMenuClient.h"

namespace WebCore {
class FrameView;
class IntRect;
class PopupMenuClient;
}

namespace WebKit {

class WebExternalPopupMenu;
class WebViewClient;
struct WebPopupMenuInfo;

// The ExternalPopupMenu connects the actual implementation of the popup menu
// to the WebCore popup menu.
class ExternalPopupMenu : public WebCore::PopupMenu,
                          public WebExternalPopupMenuClient {
public:
    ExternalPopupMenu(WebCore::PopupMenuClient*, WebViewClient*);
    virtual ~ExternalPopupMenu();

private:
    // WebCore::PopupMenu methods:
    virtual void show(const WebCore::IntRect&, WebCore::FrameView*, int index);
    virtual void hide();
    virtual void updateFromElement();
    virtual void disconnectClient();

    // WebExternalPopupClient methods:
    virtual void didChangeSelection(int index);
    virtual void didAcceptIndex(int index);
    virtual void didCancel();

    // Fills |info| with the popup menu information contained in the
    // WebCore::PopupMenuClient associated with this ExternalPopupMenu.
    void getPopupMenuInfo(WebPopupMenuInfo* info);

    WebCore::PopupMenuClient* m_popupMenuClient;
    WebViewClient* m_webViewClient;

    // The actual implementor of the show menu.
    WebExternalPopupMenu* m_webExternalPopupMenu;
}; 

} // namespace WebKit

#endif // ExternalPopupMenu_h
