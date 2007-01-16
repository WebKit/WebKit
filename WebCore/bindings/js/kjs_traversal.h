// -*- c-basic-offset: 2 -*-
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

#ifndef kjs_traversal_h
#define kjs_traversal_h

#include "NodeFilter.h"
#include "NodeFilterCondition.h"
#include "kjs_dom.h"

namespace WebCore {
    class NodeFilter;
}

namespace KJS {

  class DOMNodeFilter : public DOMObject {
  public:
    DOMNodeFilter(ExecState*, WebCore::NodeFilter*);
    ~DOMNodeFilter();
    virtual void mark();
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    WebCore::NodeFilter* impl() const { return m_impl.get(); }
    enum { AcceptNode };
  private:
    RefPtr<WebCore::NodeFilter> m_impl;
  };

  JSValue* toJS(ExecState*, WebCore::NodeFilter*);

  PassRefPtr<WebCore::NodeFilter> toNodeFilter(JSValue*); // returns 0 if value is not a DOMNodeFilter or JS function 

  class JSNodeFilterCondition : public WebCore::NodeFilterCondition {
  public:
    JSNodeFilterCondition(JSObject* filter);
    virtual short acceptNode(WebCore::Node*) const;
    virtual void mark();
  protected:
    JSObject *filter;
  };

} // namespace

#endif
