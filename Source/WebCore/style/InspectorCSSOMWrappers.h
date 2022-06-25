/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 */

#pragma once

#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSStyleRule;
class CSSStyleSheet;
class ExtensionStyleSheets;
class StyleRule;
class StyleSheetContents;

namespace Style {

class Scope;

class InspectorCSSOMWrappers {
public:
    // WARNING. This will construct CSSOM wrappers for all style rules and cache them in a map for significant memory cost.
    // It is here to support inspector. Don't use for any regular engine functions.
    CSSStyleRule* getWrapperForRuleInSheets(const StyleRule*);
    void collectFromStyleSheetIfNeeded(CSSStyleSheet*);
    void collectDocumentWrappers(ExtensionStyleSheets&);
    void collectScopeWrappers(Scope&);

private:
    template <class ListType>
    void collect(ListType*);

    void collectFromStyleSheetContents(StyleSheetContents*);
    void collectFromStyleSheets(const Vector<RefPtr<CSSStyleSheet>>&);
    void maybeCollectFromStyleSheets(const Vector<RefPtr<CSSStyleSheet>>&);

    HashMap<const StyleRule*, RefPtr<CSSStyleRule>> m_styleRuleToCSSOMWrapperMap;
    HashSet<RefPtr<CSSStyleSheet>> m_styleSheetCSSOMWrapperSet;
};

} // namespace Style
} // namespace WebCore
