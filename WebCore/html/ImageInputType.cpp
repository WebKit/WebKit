/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ImageInputType.h"

#include "FormDataList.h"
#include "HTMLInputElement.h"
#include "MouseEvent.h"
#include "RenderImage.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

PassOwnPtr<InputType> ImageInputType::create(HTMLInputElement* element)
{
    return adoptPtr(new ImageInputType(element));
}

const AtomicString& ImageInputType::formControlType() const
{
    return InputTypeNames::image();
}

bool ImageInputType::isFormDataAppendable() const
{
    return true;
}

bool ImageInputType::appendFormData(FormDataList& encoding, bool) const
{
    if (!element()->isActivatedSubmit())
        return false;
    const AtomicString& name = element()->name();
    encoding.appendData(name.isEmpty() ? "x" : (name + ".x"), m_clickLocation.x());
    encoding.appendData(name.isEmpty() ? "y" : (name + ".y"), m_clickLocation.y());
    if (!name.isEmpty() && !element()->value().isEmpty())
        encoding.appendData(name, element()->value());
    return true;
}

bool ImageInputType::supportsValidation() const
{
    return false;
}

bool ImageInputType::handleDOMActivateEvent(Event* event)
{
    RefPtr<HTMLInputElement> element = this->element();
    if (element->disabled() || !element->form())
        return false;
    element->setActivatedSubmit(true);
    if (event->underlyingEvent() && event->underlyingEvent()->isMouseEvent()) {
        MouseEvent* mouseEvent = static_cast<MouseEvent*>(event->underlyingEvent());
        m_clickLocation = IntPoint(mouseEvent->offsetX(), mouseEvent->offsetY());
    } else
        m_clickLocation = IntPoint();
    element->form()->prepareSubmit(event); // Event handlers can run.
    element->setActivatedSubmit(false);
    event->setDefaultHandled();
    return true;
}

RenderObject* ImageInputType::createRenderer(RenderArena* arena, RenderStyle*) const
{
    RenderImage* image = new (arena) RenderImage(element());
    image->setImageResource(RenderImageResource::create());
    return image;
}

} // namespace WebCore
