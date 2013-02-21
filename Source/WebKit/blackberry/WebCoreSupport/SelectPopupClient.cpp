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
#include "NotImplemented.h"
#include "Page.h"
#include "PagePopup.h"
#include "PopupPicker.h"
#include "RenderObject.h"
#include "WebPage_p.h"

#include <BlackBerryPlatformString.h>
#include <wtf/text/StringBuilder.h>

#define CELL_HEIGHT 30

namespace WebCore {

SelectPopupClient::SelectPopupClient(bool multiple, int size, const ScopeArray<BlackBerry::Platform::String>& labels, bool* enableds,
    const int* itemType, bool* selecteds, BlackBerry::WebKit::WebPagePrivate* webPage, HTMLSelectElement* element)
    : m_multiple(multiple)
    , m_size(size)
    , m_webPage(webPage)
    , m_element(element)
    , m_notifyChangeTimer(this, &SelectPopupClient::notifySelectionChange)
{
    generateHTML(multiple, size, labels, enableds, itemType, selecteds);
}

SelectPopupClient::~SelectPopupClient()
{
}

void SelectPopupClient::update(bool multiple, int size, const ScopeArray<BlackBerry::Platform::String>& labels, bool* enableds,
    const int* itemType, bool* selecteds, BlackBerry::WebKit::WebPagePrivate*, HTMLSelectElement* element)
{
    m_multiple = multiple;
    m_size = size;
    m_element = element;
    generateHTML(multiple, size, labels, enableds, itemType, selecteds);
}

void SelectPopupClient::generateHTML(bool multiple, int size, const ScopeArray<BlackBerry::Platform::String>& labels, bool* enableds,
    const int* itemType, bool* selecteds)
{
    StringBuilder source;
    source.appendLiteral("<style>\n");
    // Include CSS file.
    source.append(popupControlBlackBerryCss,
            sizeof(popupControlBlackBerryCss));
    source.appendLiteral("</style>\n<style>");
    source.append(selectControlBlackBerryCss,
            sizeof(selectControlBlackBerryCss));
    source.appendLiteral("</style></head><body>\n"
                         "<script>\n"
                         "window.addEventListener('load', function () {");
    if (m_multiple)
        source.appendLiteral("window.select.show(true, ");
    else
        source.appendLiteral("window.select.show(false, ");
    // Add labels.
    source.append('[');
    for (int i = 0; i < size; i++) {
        source.append("'" + String(labels[i]).replaceWithLiteral('\\', "\\\\").replaceWithLiteral('\'', "\\'") + "'");
        // Don't append ',' to last element.
        if (i != size - 1)
            source.appendLiteral(", ");
    }
    source.appendLiteral("], ");
    // Add enables.
    source.append('[');
    for (int i = 0; i < size; i++) {
        if (enableds[i])
            source.appendLiteral("true");
        else
            source.appendLiteral("false");
        // Don't append ',' to last element.
        if (i != size - 1)
            source.appendLiteral(", ");
    }
    source.appendLiteral("], ");
    // Add itemType.
    source.append('[');
    for (int i = 0; i < size; i++) {
        source.appendNumber(itemType[i]);
        // Don't append ',' to last element.
        if (i != size - 1)
            source.appendLiteral(", ");
    }
    source.appendLiteral("], ");
    // Add selecteds
    source.append('[');
    for (int i = 0; i < size; i++) {
        if (selecteds[i])
            source.appendLiteral("true");
        else
            source.appendLiteral("false");
        // Don't append ',' to last element.
        if (i != size - 1)
            source.appendLiteral(", ");
    }
    source.appendLiteral("] "
                         ", 'Cancel'");
    // If multi-select, add OK button for confirm.
    if (m_multiple)
        source.appendLiteral(", 'OK'");
    source.appendLiteral("); \n }); \n");
    source.append(selectControlBlackBerryJs, sizeof(selectControlBlackBerryJs));
    source.appendLiteral("</script>\n"
                         "</body> </html>\n");
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

Locale& SelectPopupClient::locale()
{
    return m_element->document()->getCachedLocale();
}

void SelectPopupClient::setValueAndClosePopup(int, const String& stringValue)
{
    ASSERT(m_element);

    static const char* cancelValue = "-1";
    if (stringValue == cancelValue) {
        closePopup();
        return;
    }

    if (m_size > 0) {
        bool selecteds[m_size];
        for (unsigned i = 0; i < m_size; i++)
            selecteds[i] = stringValue[i] - '0';

        const Vector<HTMLElement*>& items = m_element->listItems();

        // If element changed after select UI showed, do nothing but closePopup().
        if (items.size() != static_cast<unsigned>(m_size)) {
            closePopup();
            return;
        }

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
    if (m_element->renderer())
        m_element->renderer()->repaint();

    m_notifyChangeTimer.startOneShot(0);
}

void SelectPopupClient::setValue(const String&)
{
    notImplemented();
}

void SelectPopupClient::didClosePopup()
{
    m_webPage = 0;
    m_element = 0;
}

void SelectPopupClient::writeDocument(DocumentWriter& writer)
{
    writer.addData(m_source.utf8().data(), m_source.utf8().length());
}

void SelectPopupClient::notifySelectionChange(WebCore::Timer<SelectPopupClient>*)
{
    if (m_element)
        m_element->dispatchFormControlChangeEvent();
    closePopup();
}

}

