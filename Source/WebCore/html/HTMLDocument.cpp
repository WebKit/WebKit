/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * Portions are Copyright (C) 2002 Netscape Communications Corporation.
 * Other contributors: David Baron <dbaron@fas.harvard.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the document type parsing portions of this file may be used
 * under the terms of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "HTMLDocument.h"

#include "CSSPropertyNames.h"
#include "CookieJar.h"
#include "DocumentLoader.h"
#include "DocumentType.h"
#include "ExceptionCode.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HashTools.h"
#include "HTMLDocumentParser.h"
#include "HTMLBodyElement.h"
#include "HTMLElementFactory.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "InspectorInstrumentation.h"
#include "KURL.h"
#include "Page.h"
#include "Settings.h"
#include "StyleResolver.h"
#include <wtf/text/CString.h>

namespace WebCore {

using namespace HTMLNames;

HTMLDocument::HTMLDocument(Frame* frame, const KURL& url)
    : Document(frame, url, false, true)
{
    clearXMLVersion();
}

HTMLDocument::~HTMLDocument()
{
}

int HTMLDocument::width()
{
    updateLayoutIgnorePendingStylesheets();
    FrameView* frameView = view();
    return frameView ? frameView->contentsWidth() : 0;
}

int HTMLDocument::height()
{
    updateLayoutIgnorePendingStylesheets();
    FrameView* frameView = view();
    return frameView ? frameView->contentsHeight() : 0;
}

String HTMLDocument::dir()
{
    HTMLElement* b = body();
    if (!b)
        return String();
    return b->getAttribute(dirAttr);
}

void HTMLDocument::setDir(const String& value)
{
    HTMLElement* b = body();
    if (b)
        b->setAttribute(dirAttr, value);
}

String HTMLDocument::designMode() const
{
    return inDesignMode() ? "on" : "off";
}

void HTMLDocument::setDesignMode(const String& value)
{
    InheritedBool mode;
    if (equalIgnoringCase(value, "on"))
        mode = on;
    else if (equalIgnoringCase(value, "off"))
        mode = off;
    else
        mode = inherit;
    Document::setDesignMode(mode);
}

Element* HTMLDocument::activeElement()
{
    if (Node* node = treeScope()->focusedNode())
        return node->isElementNode() ? toElement(node) : body();
    return body();
}

bool HTMLDocument::hasFocus()
{
    Page* page = this->page();
    if (!page)
        return false;
    if (!page->focusController()->isActive())
        return false;
    if (Frame* focusedFrame = page->focusController()->focusedFrame()) {
        if (focusedFrame->tree()->isDescendantOf(frame()))
            return true;
    }
    return false;
}

String HTMLDocument::bgColor()
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (!bodyElement)
        return String();
    return bodyElement->bgColor();
}

void HTMLDocument::setBgColor(const String& value)
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (bodyElement)
        bodyElement->setBgColor(value);
}

String HTMLDocument::fgColor()
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (!bodyElement)
        return String();
    return bodyElement->text();
}

void HTMLDocument::setFgColor(const String& value)
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (bodyElement)
        bodyElement->setText(value);
}

String HTMLDocument::alinkColor()
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (!bodyElement)
        return String();
    return bodyElement->aLink();
}

void HTMLDocument::setAlinkColor(const String& value)
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (bodyElement) {
        // This check is a bit silly, but some benchmarks like to set the
        // document's link colors over and over to the same value and we
        // don't want to incur a style update each time.
        if (bodyElement->aLink() != value)
            bodyElement->setALink(value);
    }
}

String HTMLDocument::linkColor()
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (!bodyElement)
        return String();
    return bodyElement->link();
}

void HTMLDocument::setLinkColor(const String& value)
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (bodyElement) {
        // This check is a bit silly, but some benchmarks like to set the
        // document's link colors over and over to the same value and we
        // don't want to incur a style update each time.
        if (bodyElement->link() != value)
            bodyElement->setLink(value);
    }
}

String HTMLDocument::vlinkColor()
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (!bodyElement)
        return String();
    return bodyElement->vLink();
}

void HTMLDocument::setVlinkColor(const String& value)
{
    HTMLElement* b = body();
    HTMLBodyElement* bodyElement = (b && b->hasTagName(bodyTag)) ? static_cast<HTMLBodyElement*>(b) : 0;

    if (bodyElement) {
        // This check is a bit silly, but some benchmarks like to set the
        // document's link colors over and over to the same value and we
        // don't want to incur a style update each time.
        if (bodyElement->vLink() != value)
            bodyElement->setVLink(value);
    }
}

void HTMLDocument::captureEvents()
{
}

void HTMLDocument::releaseEvents()
{
}

PassRefPtr<DocumentParser> HTMLDocument::createParser()
{
    bool reportErrors = InspectorInstrumentation::collectingHTMLParseErrors(this->page());
    return HTMLDocumentParser::create(this, reportErrors);
}

// --------------------------------------------------------------------------
// not part of the DOM
// --------------------------------------------------------------------------

PassRefPtr<Element> HTMLDocument::createElement(const AtomicString& name, ExceptionCode& ec)
{
    if (!isValidName(name)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }
    return HTMLElementFactory::createHTMLElement(QualifiedName(nullAtom, name.lower(), xhtmlNamespaceURI), this, 0, false);
}

void HTMLDocument::addItemToMap(HashCountedSet<AtomicStringImpl*>& map, const AtomicString& name)
{
    if (name.isEmpty())
        return;
    map.add(name.impl());
    if (Frame* f = frame())
        f->script()->namedItemAdded(this, name);
}

void HTMLDocument::removeItemFromMap(HashCountedSet<AtomicStringImpl*>& map, const AtomicString& name)
{
    if (name.isEmpty())
        return;
    map.remove(name.impl());
    if (Frame* f = frame())
        f->script()->namedItemRemoved(this, name);
}

void HTMLDocument::addNamedItem(const AtomicString& name)
{
    addItemToMap(m_namedItemCounts, name);
}

void HTMLDocument::removeNamedItem(const AtomicString& name)
{ 
    removeItemFromMap(m_namedItemCounts, name);
}

void HTMLDocument::addExtraNamedItem(const AtomicString& name)
{
    addItemToMap(m_extraNamedItemCounts, name);
}

void HTMLDocument::removeExtraNamedItem(const AtomicString& name)
{ 
    removeItemFromMap(m_extraNamedItemCounts, name);
}

void HTMLDocument::clear()
{
    // FIXME: This does nothing, and that seems unlikely to be correct.
    // We've long had a comment saying that IE doesn't support this.
    // But I do see it in the documentation for Mozilla.
}

bool HTMLDocument::isFrameSet() const
{
    HTMLElement* bodyElement = body();
    return bodyElement && bodyElement->hasTagName(framesetTag);
}

}
