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

#include "config.h"
#include "DatePickerClient.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "DocumentWriter.h"
#include "HTMLInputElement.h"
#include "Page.h"
#include "PagePopup.h"
#include "PopupPicker.h"
#include "RenderObject.h"
#include "WebPage_p.h"
#include "WebString.h"

#include <wtf/text/StringBuilder.h>

namespace WebCore {

DatePickerClient::DatePickerClient(BlackBerry::Platform::BlackBerryInputType type, const BlackBerry::WebKit::WebString& value, const BlackBerry::WebKit::WebString& min, const BlackBerry::WebKit::WebString& max, double step, BlackBerry::WebKit::WebPagePrivate* webPage, HTMLInputElement* element)
    : m_type(type)
    , m_webPage(webPage)
    , m_element(element)
{
    generateHTML(type, value, min, max, step);
}

DatePickerClient::~DatePickerClient()
{
}

void DatePickerClient::generateHTML(BlackBerry::Platform::BlackBerryInputType type, const BlackBerry::WebKit::WebString& value, const BlackBerry::WebKit::WebString& min, const BlackBerry::WebKit::WebString& max, double step)
{
    StringBuilder source;
    source.append("<style>\n");
    // Include CSS file.
    source.append(popupControlBlackBerryCss,
            sizeof(popupControlBlackBerryCss));
    source.append("</style>\n<style>");
    source.append(timeControlBlackBerryCss,
            sizeof(timeControlBlackBerryCss));
    source.append("</style></head><body>\n");
    source.append("<script>\n");
    source.append("window.addEventListener('load', function () {");
    switch (type) {
    case BlackBerry::Platform::InputTypeDate:
        source.append("window.popupcontrol.show(\"Date\", ");
        break;
    case BlackBerry::Platform::InputTypeTime:
        source.append("window.popupcontrol.show(\"Time\", ");
        break;
    case BlackBerry::Platform::InputTypeDateTime:
        source.append("window.popupcontrol.show(\"DateTime\", ");
        break;
    case BlackBerry::Platform::InputTypeDateTimeLocal:
        source.append("window.popupcontrol.show(\"DateTimeLocal\", ");
        break;
    case BlackBerry::Platform::InputTypeMonth:
    case BlackBerry::Platform::InputTypeWeek:
    default:
        break;
    }
    if (!value.isEmpty())
        source.append("\"" + String(value.impl()) + "\", ");
    else
        source.append("0, ");

    if (!min.isEmpty())
        source.append(String(min.impl()) + ", ");
    else
        source.append("0, ");
    if (!max.isEmpty())
        source.append(String(max.impl()) + ", ");
    else
        source.append("0, ");
    source.append(String::number(step));
    source.append("); \n }); \n");
    source.append(timeControlBlackBerryJs, sizeof(timeControlBlackBerryJs));
    source.append("</script>\n");
    source.append("</body> </html>\n");
    m_source = source.toString();
}

void DatePickerClient::closePopup()
{
    ASSERT(m_webPage);
    m_webPage->m_page->chrome()->client()->closePagePopup(0);
}

IntSize DatePickerClient::contentSize()
{
    // Fixme: will generate content size dynamically
    return IntSize(320, 256);
}

String DatePickerClient::htmlSource()
{
    return m_source;
}

void DatePickerClient::setValueAndClosePopup(int, const String& value)
{
    // Return -1 if user cancel the selection.
    ASSERT(m_element);

    if (!value.contains("-1"))
        m_element->setValue(value);
    closePopup();
}

void DatePickerClient::didClosePopup()
{
    m_webPage = 0;
    m_element = 0;
}

void DatePickerClient::writeDocument(DocumentWriter& writer)
{
    writer.addData(m_source.utf8().data(), m_source.utf8().length());
}
}
