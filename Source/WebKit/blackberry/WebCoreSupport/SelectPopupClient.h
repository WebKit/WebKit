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

#ifndef SelectPopupClient_h
#define SelectPopupClient_h

#include "IntSize.h"
#include "PagePopupClient.h"
#include "ScopePointer.h"
#include "Timer.h"
#include "WTFString.h"
#include "WebString.h"

namespace BlackBerry {
namespace WebKit {
class WebPagePrivate;
}
}

namespace WebCore {
class DocumentWriter;
class HTMLSelectElement;
class PagePopup;

class SelectPopupClient : public PagePopupClient {
public:
    SelectPopupClient(bool multiple, int size, const ScopeArray<BlackBerry::WebKit::WebString>& labels, bool* enableds, const int* itemType, bool* selecteds, BlackBerry::WebKit::WebPagePrivate*, HTMLSelectElement*);
    ~SelectPopupClient();

    void update(bool multiple, int size, const ScopeArray<BlackBerry::WebKit::WebString>& labels, bool* enableds, const int* itemType, bool* selecteds, BlackBerry::WebKit::WebPagePrivate*, HTMLSelectElement*);

    void generateHTML(bool multiple, int size, const ScopeArray<BlackBerry::WebKit::WebString>& labels, bool* enableds, const int* itemType, bool* selecteds);
    void notifySelectionChange(WebCore::Timer<SelectPopupClient> *);

    void writeDocument(DocumentWriter&);
    virtual IntSize contentSize();
    virtual String htmlSource();
    virtual void setValueAndClosePopup(int, const String&);
    virtual void didClosePopup();
    void closePopup();

    bool m_multiple;
    unsigned m_size;
    String m_source;
    BlackBerry::WebKit::WebPagePrivate* m_webPage;
    HTMLSelectElement* m_element;
    WebCore::Timer<SelectPopupClient> m_notifyChangeTimer;
};
} // namespace WebCore
#endif
