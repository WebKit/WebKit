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
namespace Platform {
class String;
}

namespace WebKit {
class WebPagePrivate;
}
}

namespace WebCore {
class DocumentWriter;
class HTMLInputElement;

class ColorPickerClient : public PagePopupClient {
public:
    ColorPickerClient(const BlackBerry::Platform::String& value, BlackBerry::WebKit::WebPagePrivate*, HTMLInputElement*);

    void generateHTML(const BlackBerry::Platform::String& value);
    void writeDocument(DocumentWriter&);
    IntSize contentSize();
    String htmlSource() const;
    virtual Locale& locale();
    void setValueAndClosePopup(int, const String&);
    void setValue(const String&);
    void closePopup();
    void didClosePopup();

private:
    String m_source;
    BlackBerry::WebKit::WebPagePrivate* m_webPage;
    RefPtr<HTMLInputElement> m_element;
};
} // namespace WebCore
#endif
