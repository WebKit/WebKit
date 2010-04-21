/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2008 Luke Kenneth Casson Leighton <lkcl@lkcl.net>
 *  Copyright (C) 2008 Martin Soto <soto@freedesktop.org>
 *  Copyright (C) 2009, 2010 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WebKitDOMBinding.h"

#include "Event.h"
#include "EventException.h"
#include "HTMLNames.h"
#include "WebKitDOMNode.h"
#include "WebKitDOMNodePrivate.h"

namespace WebKit {

using namespace WebCore;
using namespace WebCore::HTMLNames;

// DOMObjectCache

typedef HashMap<void*, gpointer> DOMObjectMap;

static DOMObjectMap& domObjects()
{
    static DOMObjectMap staticDOMObjects;
    return staticDOMObjects;
}

gpointer DOMObjectCache::get(void* objectHandle)
{
    return domObjects().get(objectHandle);
}

gpointer DOMObjectCache::put(void* objectHandle, gpointer wrapper)
{
    domObjects().set(objectHandle, wrapper);
    return wrapper;
}

void DOMObjectCache::forget(void* objectHandle)
{
    domObjects().take(objectHandle);
}

// kit methods

static gpointer createWrapper(Node* node)
{
    ASSERT(node);
    ASSERT(!ScriptInterpreter::getDOMObject(node));

    gpointer wrappedNode = 0;

    if (node->nodeType())
        wrappedNode = wrapNode(node);

    return DOMObjectCache::put(node, wrappedNode);
}

gpointer kit(Node* node)
{
    if (!node)
        return 0;

    gpointer kitNode = DOMObjectCache::get(node);
    if (kitNode)
        return kitNode;

    return createWrapper(node);
}

} // namespace WebKit
