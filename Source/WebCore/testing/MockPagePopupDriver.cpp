/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "MockPagePopupDriver.h"

#if ENABLE(PAGE_POPUP)
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "DocumentLoader.h"
#include "DocumentWriter.h"
#include "Frame.h"
#include "HTMLIFrameElement.h"
#include "PagePopup.h"
#include "PagePopupClient.h"
#include "PagePopupController.h"
#include "Timer.h"
#include "WebCoreTestSupport.h"

namespace WebCore {

class MockPagePopup : public PagePopup, public RefCounted<MockPagePopup> {
public:
    static PassRefPtr<MockPagePopup> create(PagePopupClient*, const IntRect& originBoundsInRootView, Frame*);
    virtual ~MockPagePopup();
    void closeLater();

private:
    MockPagePopup(PagePopupClient*, const IntRect& originBoundsInRootView, Frame*);
    void close(Timer<MockPagePopup>*);

    PagePopupClient* m_popupClient;
    RefPtr<HTMLIFrameElement> m_iframe;
    Timer<MockPagePopup> m_closeTimer;
};

inline MockPagePopup::MockPagePopup(PagePopupClient* client, const IntRect& originBoundsInRootView, Frame* mainFrame)
    : m_popupClient(client)
    , m_closeTimer(this, &MockPagePopup::close)
{
    Document* document = mainFrame->document();
    m_iframe = HTMLIFrameElement::create(HTMLNames::iframeTag, document);
    m_iframe->setIdAttribute("mock-page-popup");
    m_iframe->setInlineStyleProperty(CSSPropertyBorderWidth, 0.0, CSSPrimitiveValue::CSS_PX);
    m_iframe->setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);
    m_iframe->setInlineStyleProperty(CSSPropertyLeft, originBoundsInRootView.x(), CSSPrimitiveValue::CSS_PX, true);
    m_iframe->setInlineStyleProperty(CSSPropertyTop, originBoundsInRootView.maxY(), CSSPrimitiveValue::CSS_PX, true);
    if (document->body())
        document->body()->appendChild(m_iframe.get());
    Frame* contentFrame = m_iframe->contentFrame();
    DocumentWriter* writer = contentFrame->loader()->activeDocumentLoader()->writer();
    writer->setMIMEType("text/html");
    writer->setEncoding("UTF-8", false);
    writer->begin();
    const char scriptToSetUpPagePopupController[] = "<script>window.pagePopupController = parent.internals.pagePopupController;</script>";
    writer->addData(scriptToSetUpPagePopupController, sizeof(scriptToSetUpPagePopupController));
    m_popupClient->writeDocument(*writer);
    writer->end();
}

PassRefPtr<MockPagePopup> MockPagePopup::create(PagePopupClient* client, const IntRect& originBoundsInRootView, Frame* mainFrame)
{
    return adoptRef(new MockPagePopup(client, originBoundsInRootView, mainFrame));
}

void MockPagePopup::closeLater()
{
    ref();
    m_popupClient->didClosePopup();
    m_popupClient = 0;
    // This can be called in detach(), and we should not change DOM structure
    // during detach().
    m_closeTimer.startOneShot(0);
}

void MockPagePopup::close(Timer<MockPagePopup>*)
{
    deref();
}

MockPagePopup::~MockPagePopup()
{
    if (m_iframe && m_iframe->parentNode())
        m_iframe->parentNode()->removeChild(m_iframe.get());
}

inline MockPagePopupDriver::MockPagePopupDriver(Frame* mainFrame)
    : m_mainFrame(mainFrame)
{
}

PassOwnPtr<MockPagePopupDriver> MockPagePopupDriver::create(Frame* mainFrame)
{
    return adoptPtr(new MockPagePopupDriver(mainFrame));
}

MockPagePopupDriver::~MockPagePopupDriver()
{
    closePagePopup(m_mockPagePopup.get());
}

PagePopup* MockPagePopupDriver::openPagePopup(PagePopupClient* client, const IntRect& originBoundsInRootView)
{
    if (m_mockPagePopup)
        closePagePopup(m_mockPagePopup.get());
    if (!client || !m_mainFrame)
        return 0;
    m_pagePopupController = PagePopupController::create(client);
    m_mockPagePopup = MockPagePopup::create(client, originBoundsInRootView, m_mainFrame);
    return m_mockPagePopup.get();
}

void MockPagePopupDriver::closePagePopup(PagePopup* popup)
{
    if (!popup || popup != m_mockPagePopup.get())
        return;
    m_mockPagePopup->closeLater();
    m_mockPagePopup.clear();
    m_pagePopupController->clearPagePopupClient();
    m_pagePopupController.clear();
}

}
#endif
