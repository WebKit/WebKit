/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef kjs_navigator_h
#define kjs_navigator_h

#include "kjs_binding.h"

namespace WebCore {

    class Frame;

    class Navigator : public DOMObject {
    public:
        Navigator(KJS::JSObject* prototype, Frame*);

        virtual bool getOwnPropertySlot(KJS::ExecState*, const KJS::Identifier&, KJS::PropertySlot&);
        KJS::JSValue* getValueProperty(KJS::ExecState*, int token) const;
        virtual const KJS::ClassInfo* classInfo() const { return &info; }
        static const KJS::ClassInfo info;

        enum { AppCodeName, AppName, AppVersion, Language, UserAgent, Platform,
               _Plugins, _MimeTypes, Product, ProductSub, Vendor, VendorSub, CookieEnabled };

        Frame* frame() const { return m_frame; }

    private:
        Frame* m_frame;
    };

    KJS::JSValue* navigatorProtoFuncJavaEnabled(KJS::ExecState*, KJS::JSObject*, const KJS::List&);

} // namespace

#endif
