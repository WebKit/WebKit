/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CalendarPickerElement.h"

#if ENABLE(CALENDAR_PICKER)

#include "CalendarPicker.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DateComponents.h"
#include "DocumentWriter.h"
#include "Event.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Language.h"
#include "LocalizedDate.h"
#include "LocalizedStrings.h"
#include "Page.h"
#include "RenderDetailsMarker.h"
#include "RenderTheme.h"
#include <wtf/text/StringBuilder.h>

using namespace WTF::Unicode;

namespace WebCore {

using namespace HTMLNames;

inline CalendarPickerElement::CalendarPickerElement(Document* document)
    : HTMLDivElement(divTag, document)
    , m_popup(0)
{
    setShadowPseudoId("-webkit-calendar-picker-indicator");
}

PassRefPtr<CalendarPickerElement> CalendarPickerElement::create(Document* document)
{
    return adoptRef(new CalendarPickerElement(document));
}

CalendarPickerElement::~CalendarPickerElement()
{
    closePopup();
    ASSERT(!m_popup);
}

RenderObject* CalendarPickerElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderDetailsMarker(this);
}

inline HTMLInputElement* CalendarPickerElement::hostInput()
{
    ASSERT(shadowAncestorNode());
    ASSERT(shadowAncestorNode()->hasTagName(inputTag));
    return static_cast<HTMLInputElement*>(shadowAncestorNode());
}

void CalendarPickerElement::defaultEventHandler(Event* event)
{
    HTMLInputElement* input = hostInput();
    if (input->readOnly() || input->disabled())
        return;

    if (event->type() == eventNames().clickEvent) {
        openPopup();
        event->setDefaultHandled();
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

void CalendarPickerElement::openPopup()
{
    if (m_popup)
        return;
    if (!document()->page())
        return;
    Chrome* chrome = document()->page()->chrome();
    if (!chrome)
        return;
    if (!document()->view())
        return;
    IntRect elementRectInRootView = document()->view()->contentsToRootView(hostInput()->getPixelSnappedRect());
    m_popup = chrome->client()->openPagePopup(this, elementRectInRootView);
}

void CalendarPickerElement::closePopup()
{
    if (!m_popup)
        return;
    if (!document()->page())
        return;
    Chrome* chrome = document()->page()->chrome();
    if (!chrome)
        return;
    chrome->client()->closePagePopup(m_popup);
}

void CalendarPickerElement::detach()
{
    closePopup();
    HTMLDivElement::detach();
}

IntSize CalendarPickerElement::contentSize()
{
    return IntSize(100, 100);
}

#define addLiteral(literal, writer)    writer.addData(literal, sizeof(literal) - 1)

static inline void addString(const String& str, DocumentWriter& writer)
{
    CString str8 = str.utf8();
    writer.addData(str8.data(), str8.length());
}

static void addJavaScriptString(const String& str, DocumentWriter& writer)
{
    addLiteral("\"", writer);
    StringBuilder builder;
    builder.reserveCapacity(str.length());
    for (unsigned i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' || str[i] == '"')
            builder.append('\\');
        builder.append(str[i]);
    }
    addString(builder.toString(), writer);
    addLiteral("\"", writer);
}

static void addProperty(const char* name, const String& value, DocumentWriter& writer)
{
    writer.addData(name, strlen(name));
    addLiteral(": ", writer);
    addJavaScriptString(value, writer);
    addLiteral(",\n", writer);
}

static void addProperty(const char* name, unsigned value, DocumentWriter& writer)
{
    writer.addData(name, strlen(name));
    addLiteral(": ", writer);
    addString(String::number(value), writer);
    addLiteral(",\n", writer);
}

static void addProperty(const char* name, bool value, DocumentWriter& writer)
{
    writer.addData(name, strlen(name));
    addLiteral(": ", writer);
    if (value)
        addLiteral("true", writer);
    else
        addLiteral("false", writer);
    addLiteral(",\n", writer);
}

static void addProperty(const char* name, const Vector<String>& values, DocumentWriter& writer)
{
    writer.addData(name, strlen(name));
    addLiteral(": [", writer);
    for (unsigned i = 0; i < values.size(); ++i) {
        if (i)
            addLiteral(",", writer);
        addJavaScriptString(values[i], writer);
    }
    addLiteral("],\n", writer);
}

void CalendarPickerElement::writeDocument(DocumentWriter& writer)
{
    HTMLInputElement* input = hostInput();
    DateComponents date;
    date.setMillisecondsSinceEpochForDate(input->minimum());
    String minString = date.toString();
    date.setMillisecondsSinceEpochForDate(input->maximum());
    String maxString = date.toString();
    double step;
    String stepString = input->fastGetAttribute(stepAttr);
    if (stepString.isEmpty() || !input->getAllowedValueStep(&step))
        stepString = "1";

    addLiteral("<!DOCTYPE html><head><meta charset='UTF-8'><style>\n", writer);
    writer.addData(calendarPickerCss, sizeof(calendarPickerCss));
    if (document()->page()) {
        CString extraStyle = document()->page()->theme()->extraCalendarPickerStyleSheet();
        if (extraStyle.length())
            writer.addData(extraStyle.data(), extraStyle.length());
    }
    addLiteral("</style></head><body><div id=main>Loading...</div><script>\n"
               "window.dialogArguments = {\n", writer);
    addProperty("min", minString, writer);
    addProperty("max", maxString, writer);
    addProperty("step", stepString, writer);
    addProperty("required", input->required(), writer);
    addProperty("currentValue", input->value(), writer);
    addProperty("locale", defaultLanguage(), writer);
    addProperty("todayLabel", calendarTodayText(), writer);
    addProperty("clearLabel", calendarClearText(), writer);
    addProperty("weekStartDay", firstDayOfWeek(), writer);
    addProperty("monthLabels", monthLabels(), writer);
    addProperty("dayLabels", weekDayShortLabels(), writer);
    Direction dir = direction(monthLabels()[0][0]);
    addProperty("isRTL", dir == RightToLeft || dir == RightToLeftArabic, writer);
    addLiteral("}\n", writer);

    writer.addData(calendarPickerJs, sizeof(calendarPickerJs));
    addLiteral("</script></body>\n", writer);
}

void CalendarPickerElement::setValueAndClosePopup(int numValue, const String& stringValue)
{
    ASSERT(m_popup);
    closePopup();
    if (numValue >= 0)
        hostInput()->setValue(stringValue, DispatchChangeEvent);
}

void CalendarPickerElement::didClosePopup()
{
    m_popup = 0;
}

}

#endif
