/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 1999 Waldo Bastian (bastian@kde.org)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmial.com)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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

#ifndef StyleBase_h
#define StyleBase_h

#include <wtf/RefCounted.h>

namespace WebCore {

    class String;
    class StyleSheet;

    // a style class which has a parent (almost all have)
    class StyleBase : public RefCounted<StyleBase> {
    public:
        StyleBase(StyleBase* parent)
            : m_parent(parent)
            , m_strictParsing(!parent || parent->useStrictParsing())
        { }
        virtual ~StyleBase() { }

        StyleBase* parent() const { return m_parent; }
        void setParent(StyleBase* parent) { m_parent = parent; }

        // returns the url of the style sheet this object belongs to
        String baseURL();

        virtual bool isStyleSheet() const { return false; }
        virtual bool isCSSStyleSheet() const { return false; }
        virtual bool isXSLStyleSheet() const { return false; }
        virtual bool isStyleSheetList() const { return false; }
        virtual bool isMediaList() { return false; }
        virtual bool isRuleList() { return false; }
        virtual bool isRule() { return false; }
        virtual bool isStyleRule() { return false; }
        virtual bool isCharsetRule() { return false; }
        virtual bool isImportRule() { return false; }
        virtual bool isMediaRule() { return false; }
        virtual bool isFontFaceRule() { return false; }
        virtual bool isPageRule() { return false; }
        virtual bool isUnknownRule() { return false; }
        virtual bool isStyleDeclaration() { return false; }
        virtual bool isValue() { return false; }
        virtual bool isPrimitiveValue() const { return false; }
        virtual bool isValueList() { return false; }
        virtual bool isValueCustom() { return false; }
#if ENABLE(SVG)
        virtual bool isSVGColor() const { return false; }
        virtual bool isSVGPaint() const { return false; }
#endif

        virtual bool parseString(const String&, bool /*strict*/ = false) { return false; }
        virtual void checkLoaded();

        void setStrictParsing(bool b) { m_strictParsing = b; }
        bool useStrictParsing() const { return m_strictParsing; }

        virtual void insertedIntoParent() { }

        StyleSheet* stylesheet();

    private:
        StyleBase* m_parent;
        bool m_strictParsing;
    };
}

#endif
