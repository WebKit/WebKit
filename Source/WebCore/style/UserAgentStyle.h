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

namespace WebCore {

class Element;
class StyleSheetContents;

namespace Style {

class RuleSet;

class UserAgentStyle {
public:
    static RuleSet* defaultStyle;
    static RuleSet* defaultQuirksStyle;
    static RuleSet* defaultPrintStyle;
    static unsigned defaultStyleVersion;

    static StyleSheetContents* simpleDefaultStyleSheet;
    static StyleSheetContents* defaultStyleSheet;
    static StyleSheetContents* quirksStyleSheet;
    static StyleSheetContents* dialogStyleSheet;
    static StyleSheetContents* svgStyleSheet;
    static StyleSheetContents* mathMLStyleSheet;
    static StyleSheetContents* mediaControlsStyleSheet;
    static StyleSheetContents* fullscreenStyleSheet;
    static StyleSheetContents* plugInsStyleSheet;
    static StyleSheetContents* imageControlsStyleSheet;
#if ENABLE(DATALIST_ELEMENT)
    static StyleSheetContents* dataListStyleSheet;
#endif
#if ENABLE(INPUT_TYPE_COLOR)
    static StyleSheetContents* colorInputStyleSheet;
#endif

    static StyleSheetContents* mediaQueryStyleSheet;

    static void initDefaultStyle(const Element*);
    static void ensureDefaultStyleSheetsForElement(const Element&);
    static void loadFullDefaultStyle();

private:
    static void loadSimpleDefaultStyle();
    static void addToDefaultStyle(StyleSheetContents&);
};

} // namespace Style
} // namespace WebCore
