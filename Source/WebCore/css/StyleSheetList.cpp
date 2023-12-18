/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
#include "StyleSheetList.h"

#include "CSSStyleSheet.h"
#include "Document.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "ShadowRoot.h"
#include "StyleScope.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace HTMLNames;

StyleSheetList::StyleSheetList(Document& document)
    : m_document(document)
{
}

StyleSheetList::StyleSheetList(ShadowRoot& shadowRoot)
    : m_shadowRoot(&shadowRoot)
{
}

StyleSheetList::~StyleSheetList() = default;

inline const Vector<RefPtr<StyleSheet>>& StyleSheetList::styleSheets() const
{
    if (m_document)
        return m_document->styleScope().styleSheetsForStyleSheetList();
    if (m_shadowRoot)
        return m_shadowRoot->styleScope().styleSheetsForStyleSheetList();
    return m_detachedStyleSheets;
}

Node* StyleSheetList::ownerNode() const
{
    if (m_document)
        return m_document.get();
    return m_shadowRoot;
}

void StyleSheetList::detach()
{
    if (m_document) {
        ASSERT(!m_shadowRoot);
        m_detachedStyleSheets = m_document->styleScope().styleSheetsForStyleSheetList();
        m_document = nullptr;
    } else if (m_shadowRoot) {
        ASSERT(!m_document);
        m_detachedStyleSheets = m_shadowRoot->styleScope().styleSheetsForStyleSheetList();
        m_shadowRoot = nullptr;
    } else
        ASSERT_NOT_REACHED();
}

unsigned StyleSheetList::length() const
{
    return styleSheets().size();
}

StyleSheet* StyleSheetList::item(unsigned index)
{
    const Vector<RefPtr<StyleSheet>>& sheets = styleSheets();
    return index < sheets.size() ? sheets[index].get() : 0;
}

CSSStyleSheet* StyleSheetList::namedItem(const AtomString& name) const
{
    // Support the named getter on document for backwards compatibility.
    if (!m_document)
        return nullptr;

    // IE also supports retrieving a stylesheet by name, using the name/id of the <style> tag
    // (this is consistent with all the other collections)
    // ### Bad implementation because returns a single element (are IDs always unique?)
    // and doesn't look for name attribute.
    // But unicity of stylesheet ids is good practice anyway ;)
    if (RefPtr element = dynamicDowncast<HTMLStyleElement>(m_document->getElementById(name)))
        return element->sheet();
    return nullptr;
}

bool StyleSheetList::isSupportedPropertyName(const AtomString& name) const
{
    return namedItem(name);
}

Vector<AtomString> StyleSheetList::supportedPropertyNames()
{
    // FIXME: Should be implemented.
    return Vector<AtomString>();
}

} // namespace WebCore
