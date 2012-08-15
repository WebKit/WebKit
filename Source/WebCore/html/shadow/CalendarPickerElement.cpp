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
#include "Event.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Language.h"
#include "LocalizedDate.h"
#include "LocalizedStrings.h"
#include "Page.h"
#include "PickerCommon.h"
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
    // JavaScript code can't create CalendarPickerElement objects. This is
    // always in shadow of <input>.
    ASSERT(shadowHost());
    ASSERT(shadowHost()->hasTagName(inputTag));
    return static_cast<HTMLInputElement*>(shadowHost());
}

void CalendarPickerElement::defaultEventHandler(Event* event)
{
    if (!renderer())
        return;
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

bool CalendarPickerElement::willRespondToMouseClickEvents()
{
    const HTMLInputElement* input = hostInput();
    if (renderer() && !input->readOnly() && !input->disabled())
        return true;

    return HTMLDivElement::willRespondToMouseClickEvents();
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
    return IntSize(0, 0);
}

void CalendarPickerElement::writeDocument(DocumentWriter& writer)
{
    HTMLInputElement* input = hostInput();
    DateComponents date;
    date.setMillisecondsSinceEpochForDate(input->minimum());
    String minString = date.toString();
    date.setMillisecondsSinceEpochForDate(input->maximum());
    String maxString = date.toString();
    Decimal step;
    String stepString = input->fastGetAttribute(stepAttr);
    if (stepString.isEmpty() || !input->getAllowedValueStep(&step))
        stepString = "1";

    addString("<!DOCTYPE html><head><meta charset='UTF-8'><style>\n", writer);
    writer.addData(pickerCommonCss, sizeof(pickerCommonCss));
    writer.addData(calendarPickerCss, sizeof(calendarPickerCss));
    if (document()->page()) {
        CString extraStyle = document()->page()->theme()->extraCalendarPickerStyleSheet();
        if (extraStyle.length())
            writer.addData(extraStyle.data(), extraStyle.length());
    }
    addString("</style></head><body><div id=main>Loading...</div><script>\n"
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
    addString("}\n", writer);

    writer.addData(pickerCommonJs, sizeof(pickerCommonJs));
    writer.addData(calendarPickerJs, sizeof(calendarPickerJs));
    addString("</script></body>\n", writer);
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
