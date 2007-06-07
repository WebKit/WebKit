/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef kjs_dom_h
#define kjs_dom_h

#include "JSNode.h"
#include "Node.h"
#include "kjs_binding.h"
#include <wtf/Vector.h>

namespace WebCore {
    class Attr;
    class EventTarget;
}

namespace KJS {

  WebCore::Attr* toAttr(JSValue*, bool& ok);

  bool checkNodeSecurity(ExecState*, WebCore::Node*);
  JSValue* getRuntimeObject(ExecState*, WebCore::Node*);
  JSValue* toJS(ExecState*, WebCore::EventTarget*);
  JSObject* getNodeConstructor(ExecState*);

} // namespace

#endif
