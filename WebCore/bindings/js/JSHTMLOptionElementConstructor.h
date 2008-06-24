/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef JSHTMLOptionElementConstructor_h
#define JSHTMLOptionElementConstructor_h

#include "JSDOMBinding.h"
#include <wtf/RefPtr.h>

namespace WebCore {

    class JSHTMLOptionElementConstructor : public DOMObject {
    public:
        JSHTMLOptionElementConstructor(KJS::ExecState*, Document*);
        Document* document() const { return m_document.get(); }

        static const KJS::ClassInfo s_info;

    private:
        virtual KJS::ConstructType getConstructData(KJS::ConstructData&);
        virtual const KJS::ClassInfo* classInfo() const { return &s_info; }

        RefPtr<Document> m_document;
    };

} // namespace WebCore

#endif // JSHTMLOptionElementConstructor_h
