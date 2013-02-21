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
#include "ColorPickerClient.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "DocumentWriter.h"
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

ColorPickerClient::ColorPickerClient(const BlackBerry::Platform::String& value, BlackBerry::WebKit::WebPagePrivate* webPage, HTMLInputElement* element)
    : m_webPage(webPage)
    , m_element(element)
{
    generateHTML(value);
}

void ColorPickerClient::generateHTML(const BlackBerry::Platform::String& value)
{
    StringBuilder source;
    source.appendLiteral("<style>\n");
    // Include CSS file.
    source.append(popupControlBlackBerryCss,
            sizeof(popupControlBlackBerryCss));
    source.appendLiteral("</style>\n<style>");
    source.append(colorControlBlackBerryCss,
            sizeof(colorControlBlackBerryCss));
    source.appendLiteral("</style></head><body>\n");
    source.appendLiteral("<script>\n");
    source.appendLiteral("window.addEventListener('load', function () {");
    source.appendLiteral("window.popupcontrol.show(");
    if (!value.empty())
        source.append("\"" + String(value) + "\"); \n }); \n");
    else
        source.appendLiteral("); \n }); \n");
    source.append(colorControlBlackBerryJs, sizeof(colorControlBlackBerryJs));
    source.appendLiteral("</script>\n");
    source.appendLiteral("</body> </html>\n");
    m_source = source.toString();
}

void ColorPickerClient::closePopup()
{
    ASSERT(m_webPage);
    m_webPage->m_page->chrome()->client()->closePagePopup(0);
}

IntSize ColorPickerClient::contentSize()
{
    // FIXME: will generate content size dynamically
    return IntSize(320, 256);
}

String ColorPickerClient::htmlSource() const
{
    return m_source;
}

Locale& ColorPickerClient::locale()
{
    return m_element->document()->getCachedLocale();
}

void ColorPickerClient::setValueAndClosePopup(int, const String& value)
{
    ASSERT(m_element);

    static const char* cancelValue = "-1";
    if (value != cancelValue)
        m_element->setValue(value);
    closePopup();
}

void ColorPickerClient::setValue(const String&)
{
    notImplemented();
}

void ColorPickerClient::didClosePopup()
{
    m_webPage = 0;
    m_element = 0;
}

void ColorPickerClient::writeDocument(DocumentWriter& writer)
{
    CString sourceString = m_source.utf8();
    writer.addData(sourceString.data(), sourceString.length());
}
}
