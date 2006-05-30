// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003 Apple Computer, Inc.
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

#ifndef JSXMLSerializer_H
#define JSXMLSerializer_H

#include "kjs_binding.h"

namespace KJS {
    class JSEventListener;
}

namespace WebCore {

  class JSXMLSerializerConstructorImp : public KJS::DOMObject {
  public:
    JSXMLSerializerConstructorImp(KJS::ExecState*);
    virtual bool implementsConstruct() const;
    virtual KJS::JSObject* construct(KJS::ExecState*, const KJS::List& args);
  };

  class JSXMLSerializer : public KJS::DOMObject {
  public:
    JSXMLSerializer(KJS::ExecState*);
    virtual bool toBoolean(KJS::ExecState*) const { return true; }
    virtual const KJS::ClassInfo* classInfo() const { return &info; }
    static const KJS::ClassInfo info;
    enum { SerializeToString };
  };

} // namespace

#endif
