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

#ifndef ColorPickerClient_h
#define ColorPickerClient_h

#include "PagePopupClient.h"
#include <BlackBerryPlatformInputEvents.h>

namespace BlackBerry {
namespace WebKit {
class WebPagePrivate;
class WebString;
}
}

namespace WebCore {
class DocumentWriter;
class HTMLInputElement;

class ColorPickerClient : public PagePopupClient {
public:
    ColorPickerClient(const BlackBerry::WebKit::WebString& value, BlackBerry::WebKit::WebPagePrivate*, HTMLInputElement*);

    void generateHTML(const BlackBerry::WebKit::WebString& value);
    void writeDocument(DocumentWriter&);
    IntSize contentSize();
    String htmlSource() const;
    void setValueAndClosePopup(int, const String&);
    void didClosePopup();

private:
    void closePopup();

    String m_source;
    BlackBerry::WebKit::WebPagePrivate* m_webPage;
    HTMLInputElement* m_element;
};
} // namespace WebCore
#endif
