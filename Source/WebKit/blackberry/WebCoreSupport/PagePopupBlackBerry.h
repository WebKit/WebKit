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
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>


namespace BlackBerry {
namespace WebKit {
class WebPage;
class WebPagePrivate;
}
}

namespace WebCore {
class Frame;
class Page;
class PagePopupClient;
class PlatformMouseEvent;

class PagePopupBlackBerry : public PagePopup {
public:
    PagePopupBlackBerry(BlackBerry::WebKit::WebPagePrivate*, PagePopupClient*, const IntRect&);
    ~PagePopupBlackBerry();

    bool sendCreatePopupWebViewRequest();
    bool init(BlackBerry::WebKit::WebPage*);
    void closePopup();
    void setRect();

    class SharedClientPointer : public RefCounted<SharedClientPointer> {
    public:
        explicit SharedClientPointer(PagePopupClient* client) : m_client(client) { }
        void clear() { m_client = 0; }
        PagePopupClient* get() const { return m_client; }
    private:
        PagePopupClient* m_client;
    };

private:
    void generateHTML(BlackBerry::WebKit::WebPage*);
    void installDOMFunction(Frame*);

    BlackBerry::WebKit::WebPagePrivate* m_webPagePrivate;
    OwnPtr<PagePopupClient> m_client;
    RefPtr<SharedClientPointer> m_sharedClientPointer;
    IntRect m_rect;
};

}

#endif // PagePopupBlackBerry_h
