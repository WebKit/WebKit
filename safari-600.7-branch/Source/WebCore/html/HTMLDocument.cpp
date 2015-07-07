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
#include "HTMLFrameSetElement.h"
#include "HTMLNames.h"
#include "JSDOMBinding.h"
#include "Page.h"
#include "ScriptController.h"
#include "Settings.h"
#include "StyleResolver.h"
#include <wtf/text/CString.h>

namespace WebCore {

using namespace HTMLNames;

HTMLDocument::HTMLDocument(Frame* frame, const URL& url, DocumentClassFlags documentClasses, unsigned constructionFlags)
    : Document(frame, url, documentClasses | HTMLDocumentClass, constructionFlags)
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

const AtomicString& HTMLDocument::bgColor() const
{
    HTMLElement* bodyElement = body();
    if (!bodyElement || !isHTMLBodyElement(bodyElement))
        return emptyAtom;
    return bodyElement->fastGetAttribute(bgcolorAttr);
}

void HTMLDocument::setBgColor(const String& value)
{
    HTMLElement* bodyElement = body();
    if (!bodyElement || !isHTMLBodyElement(bodyElement))
        return;
    bodyElement->setAttribute(bgcolorAttr, value);
}

const AtomicString& HTMLDocument::fgColor() const
{
    HTMLElement* bodyElement = body();
    if (!bodyElement || !isHTMLBodyElement(bodyElement))
        return emptyAtom;
    return bodyElement->fastGetAttribute(textAttr);
}

void HTMLDocument::setFgColor(const String& value)
{
    HTMLElement* bodyElement = body();
    if (!bodyElement || !isHTMLBodyElement(bodyElement))
        return;
    bodyElement->setAttribute(textAttr, value);
}

const AtomicString& HTMLDocument::alinkColor() const
{
    HTMLElement* bodyElement = body();
    if (!bodyElement || !isHTMLBodyElement(bodyElement))
        return emptyAtom;
    return bodyElement->fastGetAttribute(alinkAttr);
}

void HTMLDocument::setAlinkColor(const String& value)
{
    HTMLElement* bodyElement = body();
    if (!bodyElement || !isHTMLBodyElement(bodyElement))
        return;
    bodyElement->setAttribute(alinkAttr, value);
}

const AtomicString& HTMLDocument::linkColor() const
{
    HTMLElement* bodyElement = body();
    if (!bodyElement || !isHTMLBodyElement(bodyElement))
        return emptyAtom;
    return bodyElement->fastGetAttribute(linkAttr);
}

void HTMLDocument::setLinkColor(const String& value)
{
    HTMLElement* bodyElement = body();
    if (!bodyElement || !isHTMLBodyElement(bodyElement))
        return;
    return bodyElement->setAttribute(linkAttr, value);
}

const AtomicString& HTMLDocument::vlinkColor() const
{
    HTMLElement* bodyElement = body();
    if (!bodyElement || !isHTMLBodyElement(bodyElement))
        return emptyAtom;
    return bodyElement->fastGetAttribute(vlinkAttr);
}

void HTMLDocument::setVlinkColor(const String& value)
{
    HTMLElement* bodyElement = body();
    if (!bodyElement || !isHTMLBodyElement(bodyElement))
        return;
    return bodyElement->setAttribute(vlinkAttr, value);
}

void HTMLDocument::captureEvents()
{
}

void HTMLDocument::releaseEvents()
{
}

PassRefPtr<DocumentParser> HTMLDocument::createParser()
{
    return HTMLDocumentParser::create(*this);
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
    return HTMLElementFactory::createElement(QualifiedName(nullAtom, name.lower(), xhtmlNamespaceURI), *this);
}

static void addLocalNameToSet(HashSet<AtomicStringImpl*>* set, const QualifiedName& qName)
{
    set->add(qName.localName().impl());
}

static HashSet<AtomicStringImpl*>* createHtmlCaseInsensitiveAttributesSet()
{
    // This is the list of attributes in HTML 4.01 with values marked as "[CI]" or case-insensitive
    // Mozilla treats all other values as case-sensitive, thus so do we.
    HashSet<AtomicStringImpl*>* attrSet = new HashSet<AtomicStringImpl*>;

    addLocalNameToSet(attrSet, accept_charsetAttr);
    addLocalNameToSet(attrSet, acceptAttr);
    addLocalNameToSet(attrSet, alignAttr);
    addLocalNameToSet(attrSet, alinkAttr);
    addLocalNameToSet(attrSet, axisAttr);
    addLocalNameToSet(attrSet, bgcolorAttr);
    addLocalNameToSet(attrSet, charsetAttr);
    addLocalNameToSet(attrSet, checkedAttr);
    addLocalNameToSet(attrSet, clearAttr);
    addLocalNameToSet(attrSet, codetypeAttr);
    addLocalNameToSet(attrSet, colorAttr);
    addLocalNameToSet(attrSet, compactAttr);
    addLocalNameToSet(attrSet, declareAttr);
    addLocalNameToSet(attrSet, deferAttr);
    addLocalNameToSet(attrSet, dirAttr);
    addLocalNameToSet(attrSet, disabledAttr);
    addLocalNameToSet(attrSet, enctypeAttr);
    addLocalNameToSet(attrSet, faceAttr);
    addLocalNameToSet(attrSet, frameAttr);
    addLocalNameToSet(attrSet, hreflangAttr);
    addLocalNameToSet(attrSet, http_equivAttr);
    addLocalNameToSet(attrSet, langAttr);
    addLocalNameToSet(attrSet, languageAttr);
    addLocalNameToSet(attrSet, linkAttr);
    addLocalNameToSet(attrSet, mediaAttr);
    addLocalNameToSet(attrSet, methodAttr);
    addLocalNameToSet(attrSet, multipleAttr);
    addLocalNameToSet(attrSet, nohrefAttr);
    addLocalNameToSet(attrSet, noresizeAttr);
    addLocalNameToSet(attrSet, noshadeAttr);
    addLocalNameToSet(attrSet, nowrapAttr);
    addLocalNameToSet(attrSet, readonlyAttr);
    addLocalNameToSet(attrSet, relAttr);
    addLocalNameToSet(attrSet, revAttr);
    addLocalNameToSet(attrSet, rulesAttr);
    addLocalNameToSet(attrSet, scopeAttr);
    addLocalNameToSet(attrSet, scrollingAttr);
    addLocalNameToSet(attrSet, selectedAttr);
    addLocalNameToSet(attrSet, shapeAttr);
    addLocalNameToSet(attrSet, targetAttr);
    addLocalNameToSet(attrSet, textAttr);
    addLocalNameToSet(attrSet, typeAttr);
    addLocalNameToSet(attrSet, valignAttr);
    addLocalNameToSet(attrSet, valuetypeAttr);
    addLocalNameToSet(attrSet, vlinkAttr);

    return attrSet;
}

void HTMLDocument::addDocumentNamedItem(const AtomicStringImpl& name, Element& item)
{
    m_documentNamedItem.add(name, item, *this);
    addImpureProperty(AtomicString(const_cast<AtomicStringImpl*>(&name)));
}

void HTMLDocument::removeDocumentNamedItem(const AtomicStringImpl& name, Element& item)
{
    m_documentNamedItem.remove(name, item);
}

void HTMLDocument::addWindowNamedItem(const AtomicStringImpl& name, Element& item)
{
    m_windowNamedItem.add(name, item, *this);
}

void HTMLDocument::removeWindowNamedItem(const AtomicStringImpl& name, Element& item)
{
    m_windowNamedItem.remove(name, item);
}

bool HTMLDocument::isCaseSensitiveAttribute(const QualifiedName& attributeName)
{
    static HashSet<AtomicStringImpl*>* htmlCaseInsensitiveAttributesSet = createHtmlCaseInsensitiveAttributesSet();
    bool isPossibleHTMLAttr = !attributeName.hasPrefix() && (attributeName.namespaceURI() == nullAtom);
    return !isPossibleHTMLAttr || !htmlCaseInsensitiveAttributesSet->contains(attributeName.localName().impl());
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
    return bodyElement && isHTMLFrameSetElement(bodyElement);
}

PassRefPtr<Document> HTMLDocument::cloneDocumentWithoutChildren() const
{
    return create(nullptr, url());
}

}
