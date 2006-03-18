/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004 Apple Computer, Inc.
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
#include "kjs_views.h"

#include "kjs_css.h"

#include "dom2_viewsimpl.h"
#include "DocumentImpl.h"
#include "css/css_stylesheetimpl.h"
#include "css/css_ruleimpl.h"

using DOM::AbstractViewImpl;
using DOM::DocumentImpl;
using DOM::ElementImpl;
using DOM::NodeImpl;
using DOM::CSSRuleListImpl;

#include "kjs_views.lut.h"

namespace KJS {

// -------------------------------------------------------------------------

const ClassInfo DOMAbstractView::info = { "AbstractView", 0, &DOMAbstractViewTable, 0 };
/*
@begin DOMAbstractViewTable 2
  document              DOMAbstractView::Document               DontDelete|ReadOnly
@end
@begin DOMAbstractViewProtoTable 1
  getComputedStyle      DOMAbstractView::GetComputedStyle       DontDelete|Function 2
  getMatchedCSSRules    DOMAbstractView::GetMatchedCSSRules     DontDelete|Function 2
@end
*/

KJS_DEFINE_PROTOTYPE(DOMAbstractViewProto)
KJS_IMPLEMENT_PROTOFUNC(DOMAbstractViewProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("DOMAbstractView",DOMAbstractViewProto,DOMAbstractViewProtoFunc)

DOMAbstractView::DOMAbstractView(ExecState *exec, AbstractViewImpl *av)
  : m_impl(av)
{
  setPrototype(DOMAbstractViewProto::self(exec));
}

DOMAbstractView::~DOMAbstractView()
{
    ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue *DOMAbstractView::getValueProperty(ExecState *exec, int token)
{
    assert(token == Document);
    return toJS(exec, impl()->document());
}

bool DOMAbstractView::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<DOMAbstractView, DOMObject>(exec, &DOMAbstractViewTable, this, propertyName, slot);
}

JSValue *DOMAbstractViewProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMAbstractView::info))
    return throwError(exec, TypeError);
  AbstractViewImpl &abstractView = *static_cast<DOMAbstractView *>(thisObj)->impl();
  switch (id) {
    case DOMAbstractView::GetComputedStyle: {
        ElementImpl *arg0 = toElement(args[0]);
        if (!arg0)
          return jsUndefined(); // throw exception?
        else {
          arg0->getDocument()->updateLayoutIgnorePendingStylesheets();
          return toJS(exec, abstractView.getComputedStyle(arg0, args[1]->toString(exec).domString().impl()));
        }
      }
    case DOMAbstractView::GetMatchedCSSRules: {
        ElementImpl *arg0 = toElement(args[0]);
        if (!arg0)
            return jsUndefined(); // throw exception?
        else {
            // No need to update layout, since we just want the back-end rules.
            return toJS(exec, abstractView.getMatchedCSSRules(arg0,
                                     args[1]->toString(exec).domString().impl()).get());
        }
    }
  }
  return jsUndefined();
}

JSValue *toJS(ExecState *exec, AbstractViewImpl *av)
{
  return cacheDOMObject<AbstractViewImpl, DOMAbstractView>(exec, av);
}

AbstractViewImpl *toAbstractView(JSValue *val)
{
  if (!val || !val->isObject(&DOMAbstractView::info))
    return 0;
  return static_cast<DOMAbstractView *>(val)->impl();
}

}
