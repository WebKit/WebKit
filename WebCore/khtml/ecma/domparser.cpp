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
#include "domparser.h"
#include "domparser.lut.h"

#include "html/html_documentimpl.h"

using DOM::DocumentImpl;
using DOM::DOMImplementationImpl;

////////////////////// DOMParser Object ////////////////////////

/* Source for DOMParserProtoTable.
@begin DOMParserProtoTable 1
  parseFromString DOMParser::ParseFromString DontDelete|Function 2
@end
*/
namespace KJS {

DEFINE_PROTOTYPE("DOMParser",DOMParserProto)
IMPLEMENT_PROTOFUNC(DOMParserProtoFunc)
IMPLEMENT_PROTOTYPE(DOMParserProto,DOMParserProtoFunc)

DOMParserConstructorImp::DOMParserConstructorImp(ExecState *, DOM::DocumentImpl *d)
    : doc(d)
{
}

bool DOMParserConstructorImp::implementsConstruct() const
{
  return true;
}

ObjectImp *DOMParserConstructorImp::construct(ExecState *exec, const List &)
{
  return new DOMParser(exec, doc.get());
}

const ClassInfo DOMParser::info = { "DOMParser", 0, &DOMParserTable, 0 };

/* Source for DOMParserTable
@begin DOMParserTable 0
@end
*/

DOMParser::DOMParser(ExecState *exec, DOM::DocumentImpl *d)
  : doc(d)
{
  setPrototype(DOMParserProto::self(exec));
}


ValueImp *DOMParserProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  if (!thisObj->inherits(&DOMParser::info))
    return throwError(exec, TypeError);
  
  DOMParser *parser = static_cast<DOMParser *>(thisObj);

  switch (id) {
  case DOMParser::ParseFromString:
    {
        if (args.size() != 2)
            return jsUndefined();

      QString str = args[0]->toString(exec).qstring();
      QString contentType = args[1]->toString(exec).qstring().stripWhiteSpace();

      if (DOMImplementationImpl::isXMLMIMEType(contentType)) {
        DocumentImpl *docImpl = parser->doc->implementation()->createDocument();

        docImpl->open();
        docImpl->write(str);
        docImpl->finishParsing();
        docImpl->close();

        return getDOMNode(exec, docImpl);
      }
    }
  }
		
  return jsUndefined();
}

} // end namespace


