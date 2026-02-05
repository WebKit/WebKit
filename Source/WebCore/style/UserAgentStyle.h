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

namespace WebCore {

class Element;
class StyleSheetContents;

namespace Style {

class RuleSet;

class UserAgentStyle {
public:
    static RefPtr<RuleSet>& defaultStyle();
    static RefPtr<RuleSet>& defaultQuirksStyle();
    static RefPtr<RuleSet>& defaultPrintStyle();
    static unsigned defaultStyleVersion;

    static RefPtr<StyleSheetContents>& defaultStyleSheet();
    static RefPtr<StyleSheetContents>& quirksStyleSheet();
    static RefPtr<StyleSheetContents>& svgStyleSheet();
    static RefPtr<StyleSheetContents>& mathMLStyleSheet();
    static RefPtr<StyleSheetContents>& mathMLCoreExtrasStyleSheet();
    static RefPtr<StyleSheetContents>& mathMLFontSizeMathStyleSheet();
    static RefPtr<StyleSheetContents>& mathMLLegacyFontSizeMathStyleSheet();
    static RefPtr<StyleSheetContents>& mediaQueryStyleSheet();
    static RefPtr<StyleSheetContents>& horizontalFormControlsStyleSheet();
    static RefPtr<StyleSheetContents>& htmlSwitchControlStyleSheet();
    static RefPtr<StyleSheetContents>& popoverStyleSheet();
    static RefPtr<StyleSheetContents>& counterStylesStyleSheet();
    static RefPtr<StyleSheetContents>& viewTransitionsStyleSheet();
#if ENABLE(FULLSCREEN_API)
    static RefPtr<StyleSheetContents>& fullscreenStyleSheet();
#endif
#if ENABLE(SERVICE_CONTROLS)
    static RefPtr<StyleSheetContents>& imageControlsStyleSheet();
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
    static RefPtr<StyleSheetContents>& attachmentStyleSheet();
#endif

    static void initDefaultStyleSheet();
    static void ensureDefaultStyleSheetsForElement(const Element&);

private:
    static void addToDefaultStyle(StyleSheetContents&);
};

} // namespace Style
} // namespace WebCore
