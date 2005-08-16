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

#include "xmlserializer.h"
#include "xmlserializer.lut.h"

#include "kjs_dom.h"

#include "dom/dom_exception.h"
#include "xml/dom_docimpl.h"

#include <kdebug.h>

using DOM::NodeImpl;

namespace KJS {

////////////////////// XMLSerializer Object ////////////////////////

/* Source for XMLSerializerProtoTable.
@begin XMLSerializerProtoTable 1
  serializeToString XMLSerializer::SerializeToString DontDelete|Function 1
@end
*/
DEFINE_PROTOTYPE("XMLSerializer",XMLSerializerProto)
IMPLEMENT_PROTOFUNC(XMLSerializerProtoFunc)
IMPLEMENT_PROTOTYPE(XMLSerializerProto,XMLSerializerProtoFunc)

XMLSerializerConstructorImp::XMLSerializerConstructorImp(ExecState *)
    : ObjectImp()
{
}

bool XMLSerializerConstructorImp::implementsConstruct() const
{
  return true;
}

ObjectImp *XMLSerializerConstructorImp::construct(ExecState *exec, const List &)
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

ValueImp *XMLSerializerProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&XMLSerializer::info))
    return throwError(exec, TypeError);

  switch (id) {
  case XMLSerializer::SerializeToString:
    {
      if (args.size() != 1) {
	return Undefined();
      }

      if (!args[0]->toObject(exec)->inherits(&DOMNode::info)) {
	return Undefined();
      }

      NodeImpl *node = static_cast<NodeImpl *>(static_cast<DOMNode *>(args[0]->toObject(exec))->impl());
      return getStringOrNull(node->toString().string());
    }
  }

  return Undefined();
}

} // end namespace
