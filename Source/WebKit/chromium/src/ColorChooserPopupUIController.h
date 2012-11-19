/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef ColorChooserPopupUIController_h
#define ColorChooserPopupUIController_h

#if ENABLE(INPUT_TYPE_COLOR) && ENABLE(PAGE_POPUP)
#include "ColorChooserUIController.h"
#include "PagePopupClient.h"
#include <wtf/OwnPtr.h>

namespace WebCore {
class ColorChooserClient;
class PagePopup;
}

namespace WebKit {

class ColorChooserPopupUIController : public ColorChooserUIController, public WebCore::PagePopupClient  {

public:
    ColorChooserPopupUIController(ChromeClientImpl*, WebCore::ColorChooserClient*);
    virtual ~ColorChooserPopupUIController();

    // ColorChooserUIController functions:
    virtual void openUI() OVERRIDE;

    // ColorChooser functions
    void endChooser() OVERRIDE;

    // PagePopupClient functions:
    virtual WebCore::IntSize contentSize() OVERRIDE;
    virtual void writeDocument(WebCore::DocumentWriter&) OVERRIDE;
    virtual WebCore::Locale& locale() OVERRIDE;
    virtual void setValueAndClosePopup(int, const String&) OVERRIDE;
    virtual void didClosePopup() OVERRIDE;

private:
    void openPopup();
    void closePopup();

    ChromeClientImpl* m_chromeClient;
    WebCore::ColorChooserClient* m_client;
    WebCore::PagePopup* m_popup;
    OwnPtr<WebCore::Locale> m_locale;
};
}

#endif // ENABLE(INPUT_TYPE_COLOR) && ENABLE(PAGE_POPUP)

#endif // ColorChooserPopupUIController_h
