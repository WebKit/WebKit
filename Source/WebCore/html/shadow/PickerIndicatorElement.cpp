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
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "PickerIndicatorElement.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Event.h"
#include "Page.h"
#include "RenderDetailsMarker.h"

using namespace WTF::Unicode;

namespace WebCore {

using namespace HTMLNames;

inline PickerIndicatorElement::PickerIndicatorElement(Document* document, PickerIndicatorOwner& pickerIndicatorOwner)
    : HTMLDivElement(divTag, document)
    , m_pickerIndicatorOwner(&pickerIndicatorOwner)
{
    setPseudo(AtomicString("-webkit-calendar-picker-indicator", AtomicString::ConstructFromLiteral));
}

PassRefPtr<PickerIndicatorElement> PickerIndicatorElement::create(Document* document, PickerIndicatorOwner& pickerIndicatorOwner)
{
    return adoptRef(new PickerIndicatorElement(document, pickerIndicatorOwner));
}

PickerIndicatorElement::~PickerIndicatorElement()
{
    closePopup();
    ASSERT(!m_chooser);
}

RenderObject* PickerIndicatorElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderDetailsMarker(this);
}

void PickerIndicatorElement::defaultEventHandler(Event* event)
{
    if (!renderer())
        return;
    if (!m_pickerIndicatorOwner || m_pickerIndicatorOwner->isPickerIndicatorOwnerDisabledOrReadOnly())
        return;

    if (event->type() == eventNames().clickEvent) {
        openPopup();
        event->setDefaultHandled();
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

bool PickerIndicatorElement::willRespondToMouseClickEvents()
{
    if (renderer() && m_pickerIndicatorOwner && !m_pickerIndicatorOwner->isPickerIndicatorOwnerDisabledOrReadOnly())
        return true;

    return HTMLDivElement::willRespondToMouseClickEvents();
}

void PickerIndicatorElement::didChooseValue(const String& value)
{
    if (!m_pickerIndicatorOwner)
        return;
    m_pickerIndicatorOwner->pickerIndicatorChooseValue(value);
}

void PickerIndicatorElement::didEndChooser()
{
    m_chooser.clear();
}

void PickerIndicatorElement::openPopup()
{
    if (m_chooser)
        return;
    if (!document()->page())
        return;
    if (!m_pickerIndicatorOwner)
        return;
    Chrome* chrome = document()->page()->chrome();
    if (!chrome)
        return;
    DateTimeChooserParameters parameters;
    if (!m_pickerIndicatorOwner->setupDateTimeChooserParameters(parameters))
        return;
    m_chooser = chrome->client()->openDateTimeChooser(this, parameters);
}

void PickerIndicatorElement::closePopup()
{
    if (!m_chooser)
        return;
    m_chooser->endChooser();
}

void PickerIndicatorElement::detach()
{
    closePopup();
    HTMLDivElement::detach();
}

}

#endif
