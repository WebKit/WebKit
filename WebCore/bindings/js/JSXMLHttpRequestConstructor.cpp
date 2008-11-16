/*
 *  Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSXMLHttpRequestConstructor.h"

#include "JSXMLHttpRequest.h"
#include "ScriptExecutionContext.h"
#include "XMLHttpRequest.h"

using namespace JSC;

namespace WebCore {

ASSERT_CLASS_FITS_IN_CELL(JSXMLHttpRequestConstructor)

const ClassInfo JSXMLHttpRequestConstructor::s_info = { "XMLHttpRequestConstructor", 0, 0, 0 };

JSXMLHttpRequestConstructor::JSXMLHttpRequestConstructor(ExecState* exec, ScriptExecutionContext* context)
    : DOMObject(JSXMLHttpRequestConstructor::createStructure(exec->lexicalGlobalObject()->objectPrototype()))
{
    ASSERT(context->isDocument());
    m_document = static_cast<JSDocument*>(asObject(toJS(exec, static_cast<Document*>(context))));

    putDirect(exec->propertyNames().prototype, JSXMLHttpRequestPrototype::self(exec), None);
}

static JSObject* constructXMLHttpRequest(ExecState* exec, JSObject* constructor, const ArgList&)
{
    RefPtr<XMLHttpRequest> xmlHttpRequest = XMLHttpRequest::create(static_cast<JSXMLHttpRequestConstructor*>(constructor)->document());
    return CREATE_DOM_OBJECT_WRAPPER(exec, XMLHttpRequest, xmlHttpRequest.get());
}

ConstructType JSXMLHttpRequestConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructXMLHttpRequest;
    return ConstructTypeHost;
}

void JSXMLHttpRequestConstructor::mark()
{
    DOMObject::mark();
    if (!m_document->marked())
        m_document->mark();
}

} // namespace WebCore
