/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
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

#include <wtf/NeverDestroyed.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Element;
class StyleSheetContents;

namespace Style {

class RuleSet;

class UserAgentStyle {
public:
    struct Sheets {
        Sheets(); // Defined out of line so includers don't need complete RuleSet / StyleSheetContents types.

        const RefPtr<RuleSet> defaultStyle;
        const RefPtr<RuleSet> defaultQuirksStyle;
        const RefPtr<RuleSet> defaultPrintStyle;

        const RefPtr<StyleSheetContents> defaultStyleSheet;
        const RefPtr<StyleSheetContents> quirksStyleSheet;
        const RefPtr<StyleSheetContents> svgStyleSheet;
        const RefPtr<StyleSheetContents> mathMLStyleSheet;
        const RefPtr<StyleSheetContents> mathMLCoreExtrasStyleSheet;
        const RefPtr<StyleSheetContents> mathMLCoreMathvariantStyleSheet;
        const RefPtr<StyleSheetContents> mathMLFontSizeMathStyleSheet;
        const RefPtr<StyleSheetContents> mathMLLegacyFontSizeMathStyleSheet;
        const RefPtr<StyleSheetContents> mediaQueryStyleSheet;
        const RefPtr<StyleSheetContents> horizontalFormControlsStyleSheet;
        const RefPtr<StyleSheetContents> htmlSwitchControlStyleSheet;
        const RefPtr<StyleSheetContents> selectMultipleAndListBoxStyleSheet;
        const RefPtr<StyleSheetContents> popoverStyleSheet;
        const RefPtr<StyleSheetContents> counterStylesStyleSheet;
        const RefPtr<StyleSheetContents> viewTransitionsStyleSheet;
#if ENABLE(FULLSCREEN_API)
        const RefPtr<StyleSheetContents> fullscreenStyleSheet;
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
        const RefPtr<StyleSheetContents> attachmentStyleSheet;
#endif
    };

    static Sheets& sheets()
    {
        static NeverDestroyed<Sheets> sheets;
        return sheets;
    }

    static unsigned defaultStyleVersion;

    static void initDefaultStyleSheet();
    static void ensureDefaultStyleSheetsForElement(const Element&);

private:
    static void addToDefaultStyle(StyleSheetContents&);
};

} // namespace Style
} // namespace WebCore
