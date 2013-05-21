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
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class Frame;
}

namespace BlackBerry {
namespace WebKit {
class WebPage;
class WebPagePrivate;
class PagePopupBlackBerryClient;

class PagePopupBlackBerry {
public:
    PagePopupBlackBerry(BlackBerry::WebKit::WebPagePrivate*, PagePopupBlackBerryClient*);
    ~PagePopupBlackBerry();

    bool init(BlackBerry::WebKit::WebPage*);
    void closePopup();

    class SharedClientPointer : public RefCounted<SharedClientPointer> {
    public:
        explicit SharedClientPointer(PagePopupBlackBerryClient* client) : m_client(client) { }
        void clear() { m_client = 0; }
        PagePopupBlackBerryClient* get() const { return m_client; }
    private:
        PagePopupBlackBerryClient* m_client;
    };

private:
    void generateHTML(BlackBerry::WebKit::WebPage*);
    void installDOMFunction(WebCore::Frame*);

    BlackBerry::WebKit::WebPagePrivate* m_webPagePrivate;
    OwnPtr<PagePopupBlackBerryClient> m_client;
    RefPtr<SharedClientPointer> m_sharedClientPointer;
};

}
}

#endif // PagePopupBlackBerry_h
