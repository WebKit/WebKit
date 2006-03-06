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

#include "config.h"
#include "xmlserializer.h"
#include "xmlserializer.lut.h"

#include "kjs_dom.h"
#include "markup.h"

using DOM::NodeImpl;

namespace KJS {

////////////////////// XMLSerializer Object ////////////////////////

/* Source for XMLSerializerProtoTable.
@begin XMLSerializerProtoTable 1
  serializeToString XMLSerializer::SerializeToString DontDelete|Function 1
@end
*/
KJS_DEFINE_PROTOTYPE(XMLSerializerProto)
KJS_IMPLEMENT_PROTOFUNC(XMLSerializerProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("XMLSerializer",XMLSerializerProto,XMLSerializerProtoFunc)

XMLSerializerConstructorImp::XMLSerializerConstructorImp(ExecState *exec)
    : JSObject()
{
    putDirect(prototypePropertyName, XMLSerializerProto::self(exec), DontEnum|DontDelete|ReadOnly);
}

bool XMLSerializerConstructorImp::implementsConstruct() const
{
  return true;
}

JSObject *XMLSerializerConstructorImp::construct(ExecState *exec, const List &)
{
  return new XMLSerializer(exec);
}

const ClassInfo XMLSerializer::info = { "XMLSerializer", 0, &XMLSerializerTable, 0 };

/* Source for XMLSerializerTable.
@begin XMLSerializerTable 0
@end
*/

XMLSerializer::XMLSerializer(ExecState *exec)
{
  setPrototype(XMLSerializerProto::self(exec));
}

JSValue *XMLSerializerProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&XMLSerializer::info))
    return throwError(exec, TypeError);

  switch (id) {
  case XMLSerializer::SerializeToString:
    {
      if (args.size() != 1) {
        return jsUndefined();
      }

      if (!args[0]->toObject(exec)->inherits(&DOMNode::info)) {
        return jsUndefined();
      }

      NodeImpl *node = static_cast<NodeImpl *>(static_cast<DOMNode *>(args[0]->toObject(exec))->impl());
      return jsStringOrNull(khtml::createMarkup(node));
    }
  }

  return jsUndefined();
}

} // end namespace
