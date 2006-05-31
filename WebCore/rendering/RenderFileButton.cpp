/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderFileButton.h"

#include "FrameView.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "KWQFileButton.h"

namespace WebCore {

RenderFileButton::RenderFileButton(HTMLInputElement* element)
    : RenderFormElement(element)
{
    setWidget(new KWQFileButton(m_view->frame()));
}

void RenderFileButton::calcMinMaxWidth()
{
    ASSERT(!minMaxKnown());

    // Let the widget tell us how big it wants to be.
    int size = static_cast<HTMLInputElement*>(node())->size();
    IntSize s(static_cast<KWQFileButton*>(widget())->sizeForCharacterWidth(size > 0 ? size : 20));

    setIntrinsicWidth(s.width());
    setIntrinsicHeight(s.height());

    RenderFormElement::calcMinMaxWidth();
}

void RenderFileButton::updateFromElement()
{
    static_cast<KWQFileButton*>(widget())->setFilename(
        static_cast<HTMLInputElement*>(node())->value().deprecatedString());

    static_cast<KWQFileButton*>(widget())->setDisabled(
        static_cast<HTMLInputElement*>(node())->disabled());

    RenderFormElement::updateFromElement();
}

void RenderFileButton::returnPressed(Widget*)
{
    if (static_cast<HTMLInputElement*>(node())->form())
        static_cast<HTMLInputElement*>(node())->form()->prepareSubmit();
}

void RenderFileButton::valueChanged(Widget*)
{
    static_cast<HTMLInputElement*>(node())->setValueFromRenderer(static_cast<KWQFileButton*>(widget())->filename());
    static_cast<HTMLInputElement*>(node())->onChange();
}

void RenderFileButton::select()
{
}

void RenderFileButton::click(bool sendMouseEvents)
{
    static_cast<KWQFileButton*>(widget())->click(sendMouseEvents);
}

} // namespace WebCore
