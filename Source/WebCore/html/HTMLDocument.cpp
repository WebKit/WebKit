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
#include "CommonVM.h"
#include "CookieJar.h"
#include "CustomHeaderFields.h"
#include "DOMWindow.h"
#include "DocumentLoader.h"
#include "DocumentType.h"
#include "ElementChildIterator.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLBodyElement.h"
#include "HTMLCollection.h"
#include "HTMLDocumentParser.h"
#include "HTMLElementFactory.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HashTools.h"
#include "ScriptController.h"
#include "StyleResolver.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/CString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLDocument);

using namespace HTMLNames;

Ref<HTMLDocument> HTMLDocument::createSynthesizedDocument(Frame& frame, const URL& url)
{
    return adoptRef(*new HTMLDocument(&frame, url, HTMLDocumentClass, Synthesized));
}

HTMLDocument::HTMLDocument(Frame* frame, const URL& url, DocumentClassFlags documentClasses, unsigned constructionFlags)
    : Document(frame, url, documentClasses | HTMLDocumentClass, constructionFlags)
{
    clearXMLVersion();
}

HTMLDocument::~HTMLDocument() = default;

int HTMLDocument::width()
{
    updateLayoutIgnorePendingStylesheets();
    RefPtr<FrameView> frameView = view();
    return frameView ? frameView->contentsWidth() : 0;
}

int HTMLDocument::height()
{
    updateLayoutIgnorePendingStylesheets();
    RefPtr<FrameView> frameView = view();
    return frameView ? frameView->contentsHeight() : 0;
}

Ref<DocumentParser> HTMLDocument::createParser()
{
    return HTMLDocumentParser::create(*this);
}

// https://html.spec.whatwg.org/multipage/dom.html#dom-document-nameditem
Optional<Variant<RefPtr<WindowProxy>, RefPtr<Element>, RefPtr<HTMLCollection>>> HTMLDocument::namedItem(const AtomString& name)
{
    if (name.isNull() || !hasDocumentNamedItem(*name.impl()))
        return WTF::nullopt;

    if (UNLIKELY(documentNamedItemContainsMultipleElements(*name.impl()))) {
        auto collection = documentNamedItems(name);
        ASSERT(collection->length() > 1);
        return Variant<RefPtr<WindowProxy>, RefPtr<Element>, RefPtr<HTMLCollection>> { RefPtr<HTMLCollection> { WTFMove(collection) } };
    }

    auto& element = *documentNamedItem(*name.impl());
    if (UNLIKELY(is<HTMLIFrameElement>(element))) {
        if (auto domWindow = makeRefPtr(downcast<HTMLIFrameElement>(element).contentWindow()))
            return Variant<RefPtr<WindowProxy>, RefPtr<Element>, RefPtr<HTMLCollection>> { WTFMove(domWindow) };
    }

    return Variant<RefPtr<WindowProxy>, RefPtr<Element>, RefPtr<HTMLCollection>> { RefPtr<Element> { &element } };
}

Vector<AtomString> HTMLDocument::supportedPropertyNames() const
{
    // https://html.spec.whatwg.org/multipage/dom.html#dom-document-namedItem-which
    //
    // ... The supported property names of a Document object document at any moment consist of the following, in
    // tree order according to the element that contributed them, ignoring later duplicates, and with values from
    // id attributes coming before values from name attributes when the same element contributes both:
    //
    // - the value of the name content attribute for all applet, exposed embed, form, iframe, img, and exposed
    //   object elements that have a non-empty name content attribute and are in a document tree with document
    //   as their root;
    // - the value of the id content attribute for all applet and exposed object elements that have a non-empty
    //   id content attribute and are in a document tree with document as their root; and
    // - the value of the id content attribute for all img elements that have both a non-empty id content attribute
    //   and a non-empty name content attribute, and are in a document tree with document as their root.

    // FIXME: Implement.
    return { };
}

void HTMLDocument::addDocumentNamedItem(const AtomStringImpl& name, Element& item)
{
    m_documentNamedItem.add(name, item, *this);
    addImpureProperty(AtomString(const_cast<AtomStringImpl*>(&name)));
}

void HTMLDocument::removeDocumentNamedItem(const AtomStringImpl& name, Element& item)
{
    m_documentNamedItem.remove(name, item);
}

void HTMLDocument::addWindowNamedItem(const AtomStringImpl& name, Element& item)
{
    m_windowNamedItem.add(name, item, *this);
}

void HTMLDocument::removeWindowNamedItem(const AtomStringImpl& name, Element& item)
{
    m_windowNamedItem.remove(name, item);
}

bool HTMLDocument::isCaseSensitiveAttribute(const QualifiedName& attributeName)
{
    static const auto caseInsensitiveAttributeSet = makeNeverDestroyed([] {
        // This is the list of attributes in HTML 4.01 with values marked as "[CI]" or case-insensitive
        // Mozilla treats all other values as case-sensitive, thus so do we.
        static const QualifiedName* const names[] = {
            &accept_charsetAttr.get(),
            &acceptAttr.get(),
            &alignAttr.get(),
            &alinkAttr.get(),
            &axisAttr.get(),
            &bgcolorAttr.get(),
            &charsetAttr.get(),
            &checkedAttr.get(),
            &clearAttr.get(),
            &codetypeAttr.get(),
            &colorAttr.get(),
            &compactAttr.get(),
            &declareAttr.get(),
            &deferAttr.get(),
            &dirAttr.get(),
            &disabledAttr.get(),
            &enctypeAttr.get(),
            &faceAttr.get(),
            &frameAttr.get(),
            &hreflangAttr.get(),
            &http_equivAttr.get(),
            &langAttr.get(),
            &languageAttr.get(),
            &linkAttr.get(),
            &mediaAttr.get(),
            &methodAttr.get(),
            &multipleAttr.get(),
            &nohrefAttr.get(),
            &noresizeAttr.get(),
            &noshadeAttr.get(),
            &nowrapAttr.get(),
            &readonlyAttr.get(),
            &relAttr.get(),
            &revAttr.get(),
            &rulesAttr.get(),
            &scopeAttr.get(),
            &scrollingAttr.get(),
            &selectedAttr.get(),
            &shapeAttr.get(),
            &targetAttr.get(),
            &textAttr.get(),
            &typeAttr.get(),
            &valignAttr.get(),
            &valuetypeAttr.get(),
            &vlinkAttr.get(),
        };
        HashSet<AtomString> set;
        for (auto* name : names)
            set.add(name->localName());
        return set;
    }());

    bool isPossibleHTMLAttr = !attributeName.hasPrefix() && attributeName.namespaceURI().isNull();
    return !isPossibleHTMLAttr || !caseInsensitiveAttributeSet.get().contains(attributeName.localName());
}

bool HTMLDocument::isFrameSet() const
{
    if (!documentElement())
        return false;
    return !!childrenOfType<HTMLFrameSetElement>(*documentElement()).first();
}

Ref<Document> HTMLDocument::cloneDocumentWithoutChildren() const
{
    return create(nullptr, url());
}

}
