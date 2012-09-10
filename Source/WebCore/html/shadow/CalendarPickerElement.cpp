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

#include "Chrome.h"
#include "ChromeClient.h"
#include "Event.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "Page.h"
#include "RenderDetailsMarker.h"

using namespace WTF::Unicode;

namespace WebCore {

using namespace HTMLNames;

inline CalendarPickerElement::CalendarPickerElement(Document* document)
    : HTMLDivElement(divTag, document)
    , m_chooser(nullptr)
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
    ASSERT(!m_chooser);
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

void CalendarPickerElement::didChooseValue(const String& value)
{
    hostInput()->setValue(value, DispatchChangeEvent);
}

void CalendarPickerElement::didEndChooser()
{
    m_chooser.clear();
}

void CalendarPickerElement::openPopup()
{
    if (m_chooser)
        return;
    if (!document()->page())
        return;
    Chrome* chrome = document()->page()->chrome();
    if (!chrome)
        return;
    if (!document()->view())
        return;

    HTMLInputElement* input = hostInput();
    DateTimeChooserParameters parameters;
    parameters.type = input->type();
    parameters.minimum = input->minimum();
    parameters.maximum = input->maximum();
    parameters.required = input->required();
    Decimal step;
    if (hostInput()->getAllowedValueStep(&step))
        parameters.step = 1.0;
    else
        parameters.step = step.toDouble();
    parameters.anchorRectInRootView = document()->view()->contentsToRootView(hostInput()->pixelSnappedBoundingBox());
    parameters.currentValue = input->value();
    // FIXME: parameters.suggestionValues and suggestionLabels will be used when we support datalist.
    m_chooser = chrome->client()->openDateTimeChooser(this, parameters);
}

void CalendarPickerElement::closePopup()
{
    if (!m_chooser)
        return;
    m_chooser->endChooser();
}

void CalendarPickerElement::detach()
{
    closePopup();
    HTMLDivElement::detach();
}

}

#endif
