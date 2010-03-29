/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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
#include "RenderPartObject.h"

#include "Frame.h"
#include "FrameLoaderClient.h"
#include "HTMLEmbedElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParamElement.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "RenderView.h"
#include "RenderWidgetProtector.h"
#include "Settings.h"
#include "Text.h"

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
#include "HTMLVideoElement.h"
#endif

namespace WebCore {

using namespace HTMLNames;

RenderPartObject::RenderPartObject(Element* element)
    : RenderPart(element)
{
}

bool RenderPartObject::flattenFrame() const
{
    if (!node() || !node()->hasTagName(iframeTag))
        return false;

    HTMLIFrameElement* element = static_cast<HTMLIFrameElement*>(node());
    if (element->scrollingMode() == ScrollbarAlwaysOff
        || style()->width().isFixed()
        || style()->height().isFixed())
        return false;

    Document* document = element ? element->document() : 0;
    return document->frame() && document->frame()->settings()->frameFlatteningEnabled();
}

void RenderPartObject::calcHeight()
{
    RenderPart::calcHeight();
    if (flattenFrame())
        setHeight(max(height(), static_cast<FrameView*>(widget())->contentsHeight()));
}

void RenderPartObject::calcWidth()
{
    RenderPart::calcWidth();
    if (flattenFrame())
        setWidth(max(width(), static_cast<FrameView*>(widget())->contentsWidth()));
}

void RenderPartObject::layout()
{
    ASSERT(needsLayout());

    calcWidth();
    calcHeight();

    if (flattenFrame()) {
        layoutWithFlattening(style()->width().isFixed(), style()->height().isFixed());
        return;
    }

    RenderPart::layout();

    m_overflow.clear();
    addShadowOverflow();

    setNeedsLayout(false);
}

void RenderPartObject::viewCleared()
{
    if (node() && widget() && widget()->isFrameView()) {
        FrameView* view = static_cast<FrameView*>(widget());
        int marginw = -1;
        int marginh = -1;
        if (node()->hasTagName(iframeTag)) {
            HTMLIFrameElement* frame = static_cast<HTMLIFrameElement*>(node());
            marginw = frame->getMarginWidth();
            marginh = frame->getMarginHeight();
        }
        if (marginw != -1)
            view->setMarginWidth(marginw);
        if (marginh != -1)
            view->setMarginHeight(marginh);
    }
}

}
