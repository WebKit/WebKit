 // -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef kjs_css_h
#define kjs_css_h

#include "Color.h"
#include "JSStyleSheet.h"
#include "kjs_binding.h"

namespace WebCore {

    class Counter;
    class CSSStyleSheet;
    class StyleSheet;
    class StyleSheetList;

    class JSStyleSheetList : public KJS::DOMObject {
    public:
        JSStyleSheetList(KJS::ExecState*, StyleSheetList*, Document*);
        virtual ~JSStyleSheetList();

        virtual bool getOwnPropertySlot(KJS::ExecState*, const KJS::Identifier&, KJS::PropertySlot&);
        KJS::JSValue* getValueProperty(KJS::ExecState*, int token) const;
        // no put - all read-only
        virtual const KJS::ClassInfo* classInfo() const { return &info; }
        static const KJS::ClassInfo info;

        enum { Item, Length };

        StyleSheetList* impl() const { return m_impl.get(); }

    private:
        static KJS::JSValue* indexGetter(KJS::ExecState*, KJS::JSObject*, const KJS::Identifier&, const KJS::PropertySlot&);
        static KJS::JSValue* nameGetter(KJS::ExecState*, KJS::JSObject*, const KJS::Identifier&, const KJS::PropertySlot&);

        RefPtr<StyleSheetList> m_impl;
        RefPtr<Document> m_doc;
    };

    // The document is only used for get-stylesheet-by-name (make optional if necessary)
    KJS::JSValue* toJS(KJS::ExecState*, StyleSheetList*, Document*);

    class JSRGBColor : public KJS::DOMObject {
    public:
        JSRGBColor(KJS::ExecState*, unsigned color);
        ~JSRGBColor();

        virtual bool getOwnPropertySlot(KJS::ExecState*, const KJS::Identifier&, KJS::PropertySlot&);
        KJS::JSValue* getValueProperty(KJS::ExecState*, int token) const;
        // no put - all read-only

        virtual const KJS::ClassInfo* classInfo() const { return &info; }
        static const KJS::ClassInfo info;

        enum { Red, Green, Blue };

        unsigned impl() const { return m_color; }

    private:
        unsigned m_color;
    };

    KJS::JSValue* getJSRGBColor(KJS::ExecState*, unsigned color);

} // namespace WebCore

#endif // kjs_css_h
