// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2005 Anders Carlsson (andersca@mac.com)
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
#include "JSDOMParser.h"
#include "JSDOMParser.lut.h"

#include "HTMLDocument.h"
#include "DOMImplementation.h"

using namespace WebCore;

////////////////////// JSDOMParser Object ////////////////////////

/* Source for DOMParserProtoTable.
@begin DOMParserProtoTable 1
  parseFromString JSDOMParser::ParseFromString DontDelete|Function 2
@end
*/
namespace KJS {

KJS_DEFINE_PROTOTYPE(DOMParserProto)
KJS_IMPLEMENT_PROTOFUNC(DOMParserProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("DOMParser",DOMParserProto,DOMParserProtoFunc)

DOMParserConstructorImp::DOMParserConstructorImp(ExecState *exec, WebCore::Document *d)
    : doc(d)
{
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
    putDirect(prototypePropertyName, DOMParserProto::self(exec), None);
}

bool DOMParserConstructorImp::implementsConstruct() const
{
  return true;
}

JSObject *DOMParserConstructorImp::construct(ExecState *exec, const List &)
{
  return new JSDOMParser(exec, doc.get());
}

const ClassInfo JSDOMParser::info = { "DOMParser", 0, &DOMParserTable, 0 };

/* Source for DOMParserTable
@begin DOMParserTable 0
@end
*/

JSDOMParser::JSDOMParser(ExecState *exec, WebCore::Document *d)
  : doc(d)
{
  setPrototype(DOMParserProto::self(exec));
}


JSValue *DOMParserProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&JSDOMParser::info))
    return throwError(exec, TypeError);
  
  JSDOMParser *parser = static_cast<JSDOMParser *>(thisObj);

  switch (id) {
  case JSDOMParser::ParseFromString:
    {
        if (args.size() != 2)
            return jsUndefined();

      DeprecatedString str = args[0]->toString(exec);
      DeprecatedString contentType = DeprecatedString(args[1]->toString(exec)).stripWhiteSpace();

      if (DOMImplementation::isXMLMIMEType(contentType)) {
        RefPtr<Document> doc = parser->doc->implementation()->createDocument();

        doc->open();
        doc->write(str);
        doc->finishParsing();
        doc->close();

        return toJS(exec, doc.get());
      }
    }
  }

  return jsUndefined();
}

} // end namespace


