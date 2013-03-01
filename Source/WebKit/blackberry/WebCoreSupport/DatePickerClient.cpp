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
#include "Frame.h"
#include "HTMLInputElement.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PagePopup.h"
#include "PopupPicker.h"
#include "RenderObject.h"
#include "WebPage_p.h"

#include <BlackBerryPlatformString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

DatePickerClient::DatePickerClient(BlackBerry::Platform::BlackBerryInputType type, const BlackBerry::Platform::String& value, const BlackBerry::Platform::String& min, const BlackBerry::Platform::String& max, double step, BlackBerry::WebKit::WebPagePrivate* webPage, HTMLInputElement* element)
    : m_type(type)
    , m_webPage(webPage)
    , m_element(element)
{
    generateHTML(type, value, min, max, step);
}

DatePickerClient::~DatePickerClient()
{
}

void DatePickerClient::generateHTML(BlackBerry::Platform::BlackBerryInputType type, const BlackBerry::Platform::String& value, const BlackBerry::Platform::String& min, const BlackBerry::Platform::String& max, double step)
{
    StringBuilder source;
    source.appendLiteral("<style>\n");
    // Include CSS file.
    source.append(popupControlBlackBerryCss,
            sizeof(popupControlBlackBerryCss));
    source.appendLiteral("</style>\n<style>");
    source.append(timeControlBlackBerryCss,
            sizeof(timeControlBlackBerryCss));
    source.appendLiteral("</style></head><body>\n"
                         "<script>\n"
                         "window.addEventListener('load', function () {");
    switch (type) {
    case BlackBerry::Platform::InputTypeDate:
        source.appendLiteral("window.popupcontrol.show(\"Date\", ");
        break;
    case BlackBerry::Platform::InputTypeTime:
        source.appendLiteral("window.popupcontrol.show(\"Time\", ");
        break;
    case BlackBerry::Platform::InputTypeDateTime:
        source.appendLiteral("window.popupcontrol.show(\"DateTime\", ");
        break;
    case BlackBerry::Platform::InputTypeDateTimeLocal:
        source.appendLiteral("window.popupcontrol.show(\"DateTimeLocal\", ");
        break;
    case BlackBerry::Platform::InputTypeMonth:
        source.appendLiteral("window.popupcontrol.show(\"Month\", ");
        break;
    case BlackBerry::Platform::InputTypeWeek:
    default:
        break;
    }
    if (!value.empty())
        source.append("\"" + String(value) + "\", ");
    else
        source.appendLiteral("0, ");

    if (!min.empty())
        source.append(String(min) + ", ");
    else
        source.appendLiteral("0, ");
    if (!max.empty())
        source.append(String(max) + ", ");
    else
        source.appendLiteral("0, ");
    source.append(String::number(step));
    source.appendLiteral("); \n }); \n");
    source.append(timeControlBlackBerryJs, sizeof(timeControlBlackBerryJs));
    source.appendLiteral("</script>\n"
                         "</body> </html>\n");
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

Locale& DatePickerClient::locale()
{
    return m_element->document()->getCachedLocale();
}

void DatePickerClient::setValueAndClosePopup(int, const String& value)
{
    // Return -1 if user cancel the selection.
    ASSERT(m_element);

    // We hide caret when we select date input field, restore it when we close date picker.
    m_element->document()->frame()->selection()->setCaretVisible(true);

    if (value != "-1")
        m_element->setValue(value);
    closePopup();
}

void DatePickerClient::setValue(const String& value)
{
    notImplemented();
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
