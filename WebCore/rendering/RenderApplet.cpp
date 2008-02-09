/**
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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
#include "RenderApplet.h"

#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLAppletElement.h"
#include "HTMLNames.h"
#include "HTMLParamElement.h"
#include "Widget.h"

namespace WebCore {

using namespace HTMLNames;

RenderApplet::RenderApplet(HTMLAppletElement* applet, const HashMap<String, String>& args)
    : RenderWidget(applet)
    , m_args(args)
{
    setInline(true);
}

RenderApplet::~RenderApplet()
{
}

IntSize RenderApplet::intrinsicSize() const
{
    // FIXME: This doesn't make sense. We can't just start returning
    // a different size once we've created the widget and expect
    // layout and sizing to be correct. We should remove this and
    // pass the appropriate intrinsic size in the constructor.
    return m_widget ? IntSize(50, 50) : IntSize(150, 150);
}

void RenderApplet::createWidgetIfNecessary()
{
    HTMLAppletElement* element = static_cast<HTMLAppletElement*>(node());
    if (m_widget || !element->isFinishedParsingChildren())
        return;

    // FIXME: Java applets can't be resized (this is a bug in Apple's Java implementation).
    // In order to work around this problem and have a correct size from the start, we will
    // use fixed widths/heights from the style system when we can, since the widget might
    // not have an accurate m_width/m_height.
    int width = style()->width().isFixed() ? style()->width().value() : 
        m_width - borderLeft() - borderRight() - paddingLeft() - paddingRight();
    int height = style()->height().isFixed() ? style()->height().value() :
        m_height - borderTop() - borderBottom() - paddingTop() - paddingBottom();
    for (Node* child = element->firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(paramTag)) {
            HTMLParamElement* p = static_cast<HTMLParamElement*>(child);
            if (!p->name().isEmpty())
                m_args.set(p->name(), p->value());
        }
    }

    Frame* frame = document()->frame();
    ASSERT(frame);
    setWidget(frame->loader()->createJavaAppletWidget(IntSize(width, height), element, m_args));
}

void RenderApplet::layout()
{
    ASSERT(needsLayout());

    calcWidth();
    calcHeight();

    // The applet's widget gets created lazily upon first layout.
    createWidgetIfNecessary();
    setNeedsLayout(false);
}

} // namespace WebCore
