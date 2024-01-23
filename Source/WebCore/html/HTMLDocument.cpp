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
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentType.h"
#include "ElementChildIteratorInlines.h"
#include "FocusController.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "HTMLBodyElement.h"
#include "HTMLCollection.h"
#include "HTMLDocumentParser.h"
#include "HTMLElementFactory.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Quirks.h"
#include "ScriptController.h"
#include "StyleResolver.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/text/CString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLDocument);

using namespace HTMLNames;

Ref<HTMLDocument> HTMLDocument::createSynthesizedDocument(LocalFrame& frame, const URL& url)
{
    auto document = adoptRef(*new HTMLDocument(&frame, frame.settings(), url, { }, { DocumentClass::HTML }, { ConstructionFlag::Synthesized }));
    document->addToContextsMap();
    return document;
}

HTMLDocument::HTMLDocument(LocalFrame* frame, const Settings& settings, const URL& url, ScriptExecutionContextIdentifier documentIdentifier, DocumentClasses documentClasses, OptionSet<ConstructionFlag> constructionFlags)
    : Document(frame, settings, url, documentClasses | DocumentClasses(DocumentClass::HTML), constructionFlags, documentIdentifier)
{
    clearXMLVersion();
}

HTMLDocument::~HTMLDocument() = default;

int HTMLDocument::width()
{
    updateLayoutIgnorePendingStylesheets();
    RefPtr frameView = view();
    return frameView ? frameView->contentsWidth() : 0;
}

int HTMLDocument::height()
{
    updateLayoutIgnorePendingStylesheets();
    RefPtr frameView = view();
    return frameView ? frameView->contentsHeight() : 0;
}

Ref<DocumentParser> HTMLDocument::createParser()
{
    return HTMLDocumentParser::create(*this, parserContentPolicy());
}

// https://html.spec.whatwg.org/multipage/dom.html#dom-document-nameditem
std::optional<std::variant<RefPtr<WindowProxy>, RefPtr<Element>, RefPtr<HTMLCollection>>> HTMLDocument::namedItem(const AtomString& name)
{
    if (name.isNull() || !hasDocumentNamedItem(name))
        return std::nullopt;

    if (UNLIKELY(documentNamedItemContainsMultipleElements(name))) {
        auto collection = documentNamedItems(name);
        ASSERT(collection->length() > 1);
        return std::variant<RefPtr<WindowProxy>, RefPtr<Element>, RefPtr<HTMLCollection>> { RefPtr<HTMLCollection> { WTFMove(collection) } };
    }

    auto& element = *documentNamedItem(name);
    if (auto* iframe = dynamicDowncast<HTMLIFrameElement>(element); UNLIKELY(iframe)) {
        if (RefPtr domWindow = iframe->contentWindow())
            return std::variant<RefPtr<WindowProxy>, RefPtr<Element>, RefPtr<HTMLCollection>> { WTFMove(domWindow) };
    }

    return std::variant<RefPtr<WindowProxy>, RefPtr<Element>, RefPtr<HTMLCollection>> { RefPtr<Element> { &element } };
}

bool HTMLDocument::isSupportedPropertyName(const AtomString& name) const
{
    return !name.isNull() && hasDocumentNamedItem(name);
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

void HTMLDocument::addDocumentNamedItem(const AtomString& name, Element& item)
{
    m_documentNamedItem.add(name, item, *this);
    addImpureProperty(name);
}

void HTMLDocument::removeDocumentNamedItem(const AtomString& name, Element& item)
{
    m_documentNamedItem.remove(name, item);
}

void HTMLDocument::addWindowNamedItem(const AtomString& name, Element& item)
{
    m_windowNamedItem.add(name, item, *this);
}

void HTMLDocument::removeWindowNamedItem(const AtomString& name, Element& item)
{
    m_windowNamedItem.remove(name, item);
}

bool HTMLDocument::isCaseSensitiveAttribute(const QualifiedName& attributeName)
{
    static NeverDestroyed set = [] {
        // https://html.spec.whatwg.org/multipage/semantics-other.html#case-sensitivity-of-selectors
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
            &directionAttr,
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
    auto isPossibleHTMLAttr = !attributeName.hasPrefix() && attributeName.namespaceURI().isNull();
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
