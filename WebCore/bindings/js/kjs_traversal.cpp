/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"
#include "kjs_traversal.h"

#include "Document.h"
#include "Frame.h"
#include "JSNodeFilter.h"
#include "kjs_proxy.h"

#include "kjs_traversal.lut.h"

using namespace WebCore;

namespace KJS {

// -------------------------------------------------------------------------

const ClassInfo DOMNodeFilter::info = { "NodeFilter", 0, 0, 0 };
/*
@begin DOMNodeFilterPrototypeTable 1
  acceptNode    DOMNodeFilter::AcceptNode       DontDelete|Function 0
@end
*/
KJS_DEFINE_PROTOTYPE(DOMNodeFilterPrototype)
KJS_IMPLEMENT_PROTOTYPE_FUNCTION(DOMNodeFilterPrototypeFunction)
KJS_IMPLEMENT_PROTOTYPE("DOMNodeFilter",DOMNodeFilterPrototype,DOMNodeFilterPrototypeFunction)

DOMNodeFilter::DOMNodeFilter(ExecState *exec, NodeFilter *nf)
  : m_impl(nf) 
{
  setPrototype(DOMNodeFilterPrototype::self(exec));
}

DOMNodeFilter::~DOMNodeFilter()
{
  ScriptInterpreter::forgetDOMObject(m_impl.get());
}

void DOMNodeFilter::mark()
{
    m_impl->mark();
    DOMObject::mark();
}

JSValue *DOMNodeFilterPrototypeFunction::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&KJS::DOMNodeFilter::info))
    return throwError(exec, TypeError);
  NodeFilter &nodeFilter = *static_cast<DOMNodeFilter *>(thisObj)->impl();
  switch (id) {
    case DOMNodeFilter::AcceptNode:
      return jsNumber(nodeFilter.acceptNode(toNode(args[0])));
  }
  return jsUndefined();
}

JSValue *toJS(ExecState* exec, NodeFilter* nf)
{
    return cacheDOMObject<NodeFilter, JSNodeFilter>(exec, nf);
}

PassRefPtr<NodeFilter> toNodeFilter(JSValue* val)
{
    if (!val)
        return 0;
    if (!val->isObject())
        return 0;

    if (val->isObject(&DOMNodeFilter::info))
        return static_cast<DOMNodeFilter *>(val)->impl();

    JSObject* o = static_cast<JSObject*>(val);
    if (o->implementsCall())
        return new NodeFilter(new JSNodeFilterCondition(o));

    return 0;
}

// -------------------------------------------------------------------------

JSNodeFilterCondition::JSNodeFilterCondition(JSObject * _filter)
    : filter( _filter )
{
}

void JSNodeFilterCondition::mark()
{
    filter->mark();
}

short JSNodeFilterCondition::acceptNode(WebCore::Node* filterNode) const
{
    WebCore::Node *node = filterNode;
    Frame *frame = node->document()->frame();
    KJSProxy *proxy = frame->scriptProxy();
    if (proxy && filter->implementsCall()) {
        JSLock lock;
        ExecState *exec = proxy->interpreter()->globalExec();
        List args;
        args.append(toJS(exec, node));
        JSObject *obj = filter;
        JSValue *result = obj->call(exec, obj, args);
        return result->toInt32(exec);
    }

    return NodeFilter::FILTER_REJECT;
}

} // namespace
