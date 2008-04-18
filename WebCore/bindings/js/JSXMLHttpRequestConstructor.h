/*
 *  Copyright (C) 2003, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@nypop.com>
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

#ifndef JSXMLHttpRequestConstructor_h
#define JSXMLHttpRequestConstructor_h

#include "kjs_binding.h"

namespace WebCore {

class Document;

class JSXMLHttpRequestConstructor : public DOMObject {
public:
    JSXMLHttpRequestConstructor(KJS::ExecState*, Document*);

    virtual bool implementsConstruct() const;
    virtual KJS::JSObject* construct(KJS::ExecState*, const KJS::List&);

    virtual const KJS::ClassInfo* classInfo() const { return &s_info; }
    static const KJS::ClassInfo s_info;

private:
    RefPtr<Document> m_document;
};

} // namespace WebCore

#endif // JSXMLHttpRequestConstructor_h
