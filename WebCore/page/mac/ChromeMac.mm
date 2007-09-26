// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 */

#import "config.h"
#import "Chrome.h"

#import "BlockExceptions.h"
#import "Frame.h"
#import "Page.h"

namespace WebCore {

void Chrome::focusNSView(NSView* view)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebCoreFrameBridge *bridge = m_page->mainFrame()->bridge();
    NSResponder *firstResponder = [bridge firstResponder];
    if (firstResponder == view)
        return;

    if (![view window] || ![view superview] || ![view acceptsFirstResponder])
        return;

    [bridge makeFirstResponder:view];

    // Setting focus can actually cause a style change which might
    // remove the view from its superview while it's being made
    // first responder. This confuses AppKit so we must restore
    // the old first responder.
    if (![view superview])
        [bridge makeFirstResponder:firstResponder];

    END_BLOCK_OBJC_EXCEPTIONS;
}

} // namespace WebCore

