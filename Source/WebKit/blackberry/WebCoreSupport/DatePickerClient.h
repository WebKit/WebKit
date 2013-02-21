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

#ifndef DatePickerClient_h
#define DatePickerClient_h

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

class DatePickerClient : public PagePopupClient {
public:
    DatePickerClient(BlackBerry::Platform::BlackBerryInputType, const BlackBerry::Platform::String& value, const BlackBerry::Platform::String& min, const BlackBerry::Platform::String& max, double step, BlackBerry::WebKit::WebPagePrivate*, HTMLInputElement*);
    ~DatePickerClient();

    void generateHTML(BlackBerry::Platform::BlackBerryInputType, const BlackBerry::Platform::String& value, const BlackBerry::Platform::String& min, const BlackBerry::Platform::String& max, double step);

    void writeDocument(DocumentWriter&);
    virtual IntSize contentSize();
    virtual String htmlSource();
    virtual Locale& locale();
    void setValueAndClosePopup(int, const String&);
    void setValue(const String&);
    void didClosePopup();
    void closePopup();

private:
    BlackBerry::Platform::BlackBerryInputType m_type;
    String m_source;
    BlackBerry::WebKit::WebPagePrivate* m_webPage;
    RefPtr<HTMLInputElement> m_element;
};
} // namespace WebCore
#endif
