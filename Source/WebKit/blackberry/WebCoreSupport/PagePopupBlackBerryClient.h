/*
 * Copyright (C) 2013 Research In Motion Limited. All rights reserved.
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


#ifndef PagePopupBlackBerryClient_h
#define PagePopupBlackBerryClient_h

#include "DocumentWriter.h"
#include "IntRect.h"
#include <wtf/text/WTFString.h>

namespace WebCore {
class DocumentWriter;
class HTMLInputElement;
}

namespace BlackBerry {
namespace WebKit {
class WebPagePrivate;

class PagePopupBlackBerryClient {
public:
    virtual WebCore::IntSize contentSize();

    // Provide an HTML source through the specified DocumentWriter. The HTML
    // source is rendered in a PagePopup.
    // The content HTML supports:
    //  - No <select> popups
    //  - window.setValueAndClosePopup(number, string).
    virtual void writeDocument(WebCore::DocumentWriter&);

    // This is called by the content HTML of a PagePopup.
    // An implementation of this function should call ChromeClient::closePagePopup().
    virtual void setValueAndClosePopup(int numValue, const String& stringValue) = 0;

    // This is called by the content HTML of a PagePopup.
    virtual void closePopup();

    // This is called whenever a PagePopup was closed.
    virtual void didClosePopup();

    virtual ~PagePopupBlackBerryClient() { }

protected:
    PagePopupBlackBerryClient(BlackBerry::WebKit::WebPagePrivate*);

    BlackBerry::WebKit::WebPagePrivate* m_webPage;
    String m_source;
};

}
}

#endif
