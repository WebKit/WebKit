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

#include "SelectPopupClient.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "DocumentWriter.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "Page.h"
#include "PagePopup.h"
#include "RenderObject.h"
#include "UserAgentStyleSheets.h"
#include "WebPage_p.h"

#include <wtf/text/StringBuilder.h>

#define CELL_HEIGHT 30

namespace WebCore {

SelectPopupClient::SelectPopupClient(bool multiple, int size, const ScopeArray<BlackBerry::WebKit::WebString>& labels, bool* enableds,
    const int* itemType, bool* selecteds, BlackBerry::WebKit::WebPagePrivate* webPage, HTMLSelectElement* element)
    : m_multiple(multiple)
    , m_size(size)
    , m_webPage(webPage)
    , m_element(element)
{
    generateHTML(multiple, size, labels, enableds, itemType, selecteds);
}

SelectPopupClient::~SelectPopupClient()
{
}

void SelectPopupClient::update(bool multiple, int size, const ScopeArray<BlackBerry::WebKit::WebString>& labels, bool* enableds,
    const int* itemType, bool* selecteds, BlackBerry::WebKit::WebPagePrivate*, HTMLSelectElement* element)
{
    m_multiple = multiple;
    m_size = size;
    m_element = element;
    generateHTML(multiple, size, labels, enableds, itemType, selecteds);
}

void SelectPopupClient::generateHTML(bool multiple, int size, const ScopeArray<BlackBerry::WebKit::WebString>& labels, bool* enableds,
    const int* itemType, bool* selecteds)
{
    StringBuilder source;
    String fullPath(RESOURCE_PATH);
    String singleSelectImage("singleSelect.png");
    String multiSelectImage("multiSelect.png");
    source.append("<head><style>\n");
    // Include CSS file.
    source.append(popupControlBlackBerryUserAgentStyleSheet,
            sizeof(popupControlBlackBerryUserAgentStyleSheet));
    source.append("</style></head><body>\n");
    source.append("<script>\n");
    source.append("var options=new Array(" + String::number(size) + ");");
    source.append("for (var i = 0; i < " + String::number(size) + "; i++ )");
    source.append("{ options[i] = false ;");
    source.append("var imageid = document.getElementById(\"image\" + parseInt(i)); imageid.style.visibility = false; }");
    source.append("function Ok() { var selecteds = \"\";");
    source.append("for (var i = 0; i < " + String::number(size) + "; i++ )");
    source.append("{  if (options[i]) selecteds += '1'; else selecteds += '0';}");
    source.append("window.setValueAndClosePopup(selecteds, window.popUp); window.close();}");
    source.append("function Cancel() { var selecteds = \"\";");
    source.append("for (var i = 0; i < " + String::number(size) + "; i++ )");
    source.append("selecteds += '0';");
    source.append("window.setValueAndClosePopup(selecteds, window.popUp); window.close();}");
    if (m_multiple)
        source.append("function Select(i) { options[i] = !options[i]; var imageid = document.getElementById(\"image\" + parseInt(i)); imageid.style.visibility = options[i]; }");
    else {
        source.append("function Select(i) { for (var j = 0; j < " + String::number(size) + "; j++ )");
        source.append("{ options[j] = false; ");
        source.append("var imageid = document.getElementById(\"image\" + parseInt(j)); imageid.style.visibility = false; }");
        source.append("options[i] = true; ");
        source.append("var imageid = document.getElementById(\"image\" + parseInt(i)); imageid.style.visibility = true; }");
    }
    source.append("</script>\n");

    int tableWidth = contentSize().width();
    int tableHeight = CELL_HEIGHT * size;
    source.append("<table width=\"" + String::number(tableWidth) + "\" height=\"" + String::number(tableHeight)
        + "\" border=\"0\" frame=\"void\" rules=\"rows\"> ");
    for (int i = 0; i < size; i++) {
        source.append(" <tr> <td bgcolor=\"#E2E4E3\" width=\"80%\"><input class=\"tablebutton\" id=\"button" + String::number(i)
            + "\" type=\"button\" value=\"" + String(labels[i].impl()) + "\" onclick=\"Select(" + String::number(i) + ");\" />");
        source.append("</td>");
        source.append("<td bgcolor=\"#E2E4E3\" width=\"20%\"><input type=\"image\" id=\"image" + String::number(i) + "\" src=\" " + fullPath
            + singleSelectImage + " \" /></td> </tr>");
    }
    source.append("</table>");
    source.append("<table border=\"0\" cellpadding=\"0\" cellspacing=\"0\"> <tr> <td>");
    source.append("<input class=\"bottombuttonOK\" name=\"btnOk\" type=\"button\" value=\"Ok\" ");
    source.append("onclick=\"Ok();\" /></td> <td nowrap=\"nowrap\"> <input class=\"bottombuttonCancel\" type=\"button\" value=\"Cancel\" onclick=\"Cancel();\" />");
    source.append("</td> </tr></table>");
    source.append("</body>\n");
    m_source = source.toString();
}

void SelectPopupClient::closePopup()
{
    ASSERT(m_webPage);
    m_webPage->m_page->chrome()->client()->closePagePopup(0);
}

IntSize SelectPopupClient::contentSize()
{
    // Fixme: will generate content size dynamically
    return IntSize(320, 256);
}

String SelectPopupClient::htmlSource()
{
    return m_source;
}

void SelectPopupClient::setValueAndClosePopup(int, const String& stringValue)
{

    ASSERT(m_size == stringValue.length());

    if (m_size > 0) {
        bool selecteds[m_size];
        for (unsigned i = 0; i < m_size; i++)
            selecteds[i] = stringValue[i] - '0';

        const WTF::Vector<HTMLElement*>& items = m_element->listItems();

        if (items.size() != static_cast<unsigned int>(m_size))
            return;

        HTMLOptionElement* option;
        for (unsigned i = 0; i < m_size; i++) {
            if (items[i]->hasTagName(HTMLNames::optionTag)) {
                option = static_cast<HTMLOptionElement*>(items[i]);
                option->setSelectedState(selecteds[i]);
            }
        }
    }
    // Force repaint because we do not send mouse events to the select element
    // and the element doesn't automatically repaint itself.
    m_element->dispatchFormControlChangeEvent();
    m_element->renderer()->repaint();
    closePopup();
}

void SelectPopupClient::didClosePopup()
{
    m_webPage = 0;
    m_element = 0;
}

void SelectPopupClient::writeDocument(DocumentWriter& writer)
{
    writer.setMIMEType("text/html");
    writer.begin(KURL());
    writer.addData(m_source.utf8().data(), m_source.utf8().length());
    writer.end();
}
}

