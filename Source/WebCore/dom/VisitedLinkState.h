/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#pragma once

#include "Element.h"
#include "RenderStyleConstants.h"
#include "SharedStringHash.h"
#include <wtf/HashSet.h>

namespace WebCore {

class Document;

class VisitedLinkState {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit VisitedLinkState(Document&);

    void invalidateStyleForAllLinks();
    void invalidateStyleForLink(SharedStringHash);
    InsideLink determineLinkState(const Element&);

private:
    InsideLink determineLinkStateSlowCase(const Element&);

    Document& m_document;
    HashSet<SharedStringHash, SharedStringHashHash> m_linksCheckedForVisitedState;
};

inline InsideLink VisitedLinkState::determineLinkState(const Element& element)
{
    if (!element.isLink())
        return InsideLink::NotInside;
    return determineLinkStateSlowCase(element);
}

} // namespace WebCore
