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

#ifndef _KJS_VIEWS_H_
#define _KJS_VIEWS_H_

#include "kjs_dom.h"

namespace DOM {
    class AbstractViewImpl;
}

namespace KJS {

  class DOMAbstractView : public DOMObject {
  public:
    DOMAbstractView(ExecState *, DOM::AbstractViewImpl *av);
    ~DOMAbstractView();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    ValueImp *getValueProperty(ExecState *exec, int token);
    // no put - all read-only
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    DOM::AbstractViewImpl *impl() const { return m_impl.get(); }
    enum { Document, GetComputedStyle, GetMatchedCSSRules };
  private:
    SharedPtr<DOM::AbstractViewImpl> m_impl;
  };

  ValueImp *getDOMAbstractView(ExecState *exec, DOM::AbstractViewImpl *av);

  DOM::AbstractViewImpl *toAbstractView(ValueImp *);

} // namespace

#endif
