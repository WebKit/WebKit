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
#include "JSXMLSerializer.h"
#include "JSXMLSerializer.lut.h"

#include "DeprecatedString.h"
#include "PlatformString.h"
#include "kjs_dom.h"
#include "markup.h"

using namespace KJS;

namespace WebCore {

/* Source for JSXMLSerializerProtoTable.
@begin JSXMLSerializerProtoTable 1
  serializeToString WebCore::JSXMLSerializer::SerializeToString DontDelete|Function 1
@end
*/

KJS_DEFINE_PROTOTYPE(JSXMLSerializerProto)
KJS_IMPLEMENT_PROTOFUNC(JSXMLSerializerProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("XMLSerializer", JSXMLSerializerProto, JSXMLSerializerProtoFunc)

JSXMLSerializerConstructorImp::JSXMLSerializerConstructorImp(ExecState* exec)
{
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
    putDirect(prototypePropertyName, JSXMLSerializerProto::self(exec), None);
}

bool JSXMLSerializerConstructorImp::implementsConstruct() const
{
    return true;
}

JSObject* JSXMLSerializerConstructorImp::construct(ExecState* exec, const List&)
{
    return new JSXMLSerializer(exec);
}

const ClassInfo JSXMLSerializer::info = { "XMLSerializer", 0, &JSXMLSerializerTable, 0 };

/* Source for JSXMLSerializerTable.
@begin JSXMLSerializerTable 0
@end
*/

JSXMLSerializer::JSXMLSerializer(ExecState* exec)
{
    setPrototype(JSXMLSerializerProto::self(exec));
}

JSValue* JSXMLSerializerProtoFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXMLSerializer::info))
        return throwError(exec, TypeError);

    if (id == JSXMLSerializer::SerializeToString) {
        if (args.size() != 1)
            return jsUndefined();

        if (!args[0]->toObject(exec)->inherits(&DOMNode::info))
            return jsUndefined();

        Node* node = static_cast<Node*>(static_cast<DOMNode*>(args[0]->toObject(exec))->impl());
        return jsStringOrNull(createMarkup(node));
    }

    return jsUndefined();
}

} // end namespace
