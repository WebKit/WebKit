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
#include "Quirks.h"
#include "ScriptController.h"
#include "StyleResolver.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/text/CString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLDocument);

using namespace HTMLNames;

Ref<HTMLDocument> HTMLDocument::createSynthesizedDocument(Frame& frame, const URL& url)
{
    auto document = adoptRef(*new HTMLDocument(&frame, frame.settings(), url, { }, { DocumentClass::HTML }, Synthesized));
    document->addToContextsMap();
    return document;
}

HTMLDocument::HTMLDocument(Frame* frame, const Settings& settings, const URL& url, ScriptExecutionContextIdentifier documentIdentifier, DocumentClasses documentClasses, unsigned constructionFlags)
    : Document(frame, settings, url, documentClasses | DocumentClasses(DocumentClass::HTML), constructionFlags, documentIdentifier)
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
    return HTMLDocumentParser::create(*this, parserContentPolicy());
}

// https://html.spec.whatwg.org/multipage/dom.html#dom-document-nameditem
std::optional<std::variant<RefPtr<WindowProxy>, RefPtr<Element>, RefPtr<HTMLCollection>>> HTMLDocument::namedItem(const AtomString& name)
{
    if (name.isNull() || !hasDocumentNamedItem(*name.impl()))
        return std::nullopt;

    if (UNLIKELY(documentNamedItemContainsMultipleElements(*name.impl()))) {
        auto collection = documentNamedItems(name);
        ASSERT(collection->length() > 1);
        return std::variant<RefPtr<WindowProxy>, RefPtr<Element>, RefPtr<HTMLCollection>> { RefPtr<HTMLCollection> { WTFMove(collection) } };
    }

    auto& element = *documentNamedItem(*name.impl());
    if (UNLIKELY(is<HTMLIFrameElement>(element))) {
        if (RefPtr domWindow = downcast<HTMLIFrameElement>(element).contentWindow())
            return std::variant<RefPtr<WindowProxy>, RefPtr<Element>, RefPtr<HTMLCollection>> { WTFMove(domWindow) };
    }

    return std::variant<RefPtr<WindowProxy>, RefPtr<Element>, RefPtr<HTMLCollection>> { RefPtr<Element> { &element } };
}

Vector<AtomString> HTMLDocument::supportedPropertyNames() const
{
    if (Quirks::shouldOmitHTMLDocumentSupportedPropertyNames())
        return { };

    auto properties = m_documentNamedItem.keys();
    // The specification says these should be sorted in document order but this would be expensive
    // and other browser engines do not comply with this part of the specification. For now, just
    // do an alphabetical sort to get consistent results.
    std::sort(properties.begin(), properties.end(), WTF::codePointCompareLessThan);
    return properties;
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
    static NeverDestroyed set = [] {
        // This is the list of attributes in HTML 4.01 with values marked as "[CI]" or case-insensitive
        // Mozilla treats all other values as case-sensitive, thus so do we.
        static constexpr std::array names {
            &accept_charsetAttr,
            &acceptAttr,
            &alignAttr,
            &alinkAttr,
            &axisAttr,
            &bgcolorAttr,
            &charsetAttr,
            &checkedAttr,
            &clearAttr,
            &codetypeAttr,
            &colorAttr,
            &compactAttr,
            &declareAttr,
            &deferAttr,
            &dirAttr,
            &disabledAttr,
            &enctypeAttr,
            &faceAttr,
            &frameAttr,
            &hreflangAttr,
            &http_equivAttr,
            &langAttr,
            &languageAttr,
            &linkAttr,
            &mediaAttr,
            &methodAttr,
            &multipleAttr,
            &nohrefAttr,
            &noresizeAttr,
            &noshadeAttr,
            &nowrapAttr,
            &readonlyAttr,
            &relAttr,
            &revAttr,
            &rulesAttr,
            &scopeAttr,
            &scrollingAttr,
            &selectedAttr,
            &shapeAttr,
            &targetAttr,
            &textAttr,
            &typeAttr,
            &valignAttr,
            &valuetypeAttr,
            &vlinkAttr,
        };
        MemoryCompactLookupOnlyRobinHoodHashSet<AtomString> set;
        set.reserveInitialCapacity(std::size(names));
        for (auto* name : names)
            set.add(name->get().localName());
        return set;
    }();
    bool isPossibleHTMLAttr = !attributeName.hasPrefix() && attributeName.namespaceURI().isNull();
    return !isPossibleHTMLAttr || !set.get().contains(attributeName.localName());
}

bool HTMLDocument::isFrameSet() const
{
    if (!documentElement())
        return false;
    return !!childrenOfType<HTMLFrameSetElement>(*documentElement()).first();
}

Ref<Document> HTMLDocument::cloneDocumentWithoutChildren() const
{
    return create(nullptr, settings(), url());
}

}
