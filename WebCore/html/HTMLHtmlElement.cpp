/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
#include "config.h"
#include "HTMLHtmlElement.h"

#include "ApplicationCacheGroup.h"
#include "Document.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLHtmlElement::HTMLHtmlElement(const QualifiedName& tagName, Document* doc)
    : HTMLElement(tagName, doc)
{
    ASSERT(hasTagName(htmlTag));
}

HTMLHtmlElement::~HTMLHtmlElement()
{
}

String HTMLHtmlElement::version() const
{
    return getAttribute(versionAttr);
}

void HTMLHtmlElement::setVersion(const String &value)
{
    setAttribute(versionAttr, value);
}

bool HTMLHtmlElement::checkDTD(const Node* newChild)
{
    return newChild->hasTagName(headTag) || newChild->hasTagName(bodyTag) ||
           newChild->hasTagName(framesetTag) || newChild->hasTagName(noframesTag);
}

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
void HTMLHtmlElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    
    if (!document()->parsing())
        return;

    if (!document()->frame())
        return;

    // Check the manifest attribute
    AtomicString manifest = getAttribute(manifestAttr);
    if (manifest.isNull())
        ApplicationCacheGroup::selectCacheWithoutManifestURL(document()->frame());
    else
        ApplicationCacheGroup::selectCache(document()->frame(), document()->completeURL(manifest));
}
#endif

}
