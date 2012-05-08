/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef PagePopupBlackBerry_h
#define PagePopupBlackBerry_h

#include "IntRect.h"
#include "PagePopup.h"


namespace BlackBerry {
namespace WebKit {
class WebPage;
class WebPagePrivate;
}
}

namespace WebCore {
class Frame;
class Page;
class PagePopupChromeClient;
class PagePopupClient;
class PlatformMouseEvent;

class PagePopupBlackBerry : public PagePopup {
public:
    PagePopupBlackBerry(BlackBerry::WebKit::WebPagePrivate*, PagePopupClient*, const IntRect&);
    ~PagePopupBlackBerry();

    void sendCreatePopupWebViewRequest();
    bool init(BlackBerry::WebKit::WebPage*);
    void closePopup();
    void installDomFunction(Frame*);
    void setRect();
    void closeWebPage();

    bool handleMouseEvent(PlatformMouseEvent&);


private:
    BlackBerry::WebKit::WebPagePrivate* m_webPagePrivate;
    OwnPtr<PagePopupClient> m_client;
    IntRect m_rect;
    OwnPtr<WebCore::Page> m_page;
    OwnPtr<PagePopupChromeClient> m_chromeClient;

    friend class PagePopupChromeClient;
};

}

#endif // PagePopupBlackBerry_h
