/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
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

#include <stdio.h>

#include <kjs/operations.h>
#include <dom_string.h>
#include <dom_exception.h>

#include "kjs_dom.h"
#include "kjs_text.h"

using namespace KJS;

const TypeInfo DOMCharacterData::info = { "CharacterImp", HostType,
					  &DOMNode::info, 0, 0 };

KJSO DOMCharacterData::tryGet(const UString &p) const
{
  DOM::CharacterData data = static_cast<DOM::CharacterData>(node);
  if (p == "data")
    return String(data.data());
  else if (p == "length")
    return Number(data.length());
  else if (p == "substringData")
    return new DOMCharacterDataFunction(data, DOMCharacterDataFunction::SubstringData);
  else if (p == "appendData")
    return new DOMCharacterDataFunction(data, DOMCharacterDataFunction::AppendData);
  else if (p == "insertData")
    return new DOMCharacterDataFunction(data, DOMCharacterDataFunction::InsertData);
  else if (p == "deleteData")
    return new DOMCharacterDataFunction(data, DOMCharacterDataFunction::DeleteData);
  else if (p == "replaceData")
    return new DOMCharacterDataFunction(data, DOMCharacterDataFunction::ReplaceData);
  else
    return DOMNode::tryGet(p);
}

void DOMCharacterData::tryPut(const UString &p, const KJSO& v)
{
  if (p == "data")
    static_cast<DOM::CharacterData>(node).setData(v.toString().value().string());
  else
    DOMNode::tryPut(p,v);
}

DOMCharacterDataFunction::DOMCharacterDataFunction(DOM::CharacterData d, int i)
  : data(d), id(i)
{
}

Completion DOMCharacterDataFunction::tryExecute(const List &args)
{
  KJSO result;

  switch(id) {
    case SubstringData:
      result = getString(data.substringData(args[0].toNumber().intValue(),args[1].toNumber().intValue()));
      break;
    case AppendData:
      data.appendData(args[0].toString().value().string());
      result = Undefined();
      break;
    case InsertData:
      data.insertData(args[0].toNumber().intValue(),args[1].toString().value().string());
      result = Undefined();
      break;
    case DeleteData:
      data.deleteData(args[0].toNumber().intValue(),args[1].toNumber().intValue());
      result = Undefined();
      break;
    case ReplaceData:
      data.replaceData(args[0].toNumber().intValue(),args[1].toNumber().intValue(),args[2].toString().value().string());
      result = Undefined();
      break;
    default:
      result = Undefined();
  }

  return Completion(ReturnValue, result);
}

// -------------------------------------------------------------------------

const TypeInfo DOMText::info = { "Text", HostType,
				 &DOMCharacterData::info, 0, 0 };

KJSO DOMText::tryGet(const UString &p) const
{
  if (p == "")
    return Undefined(); // ### TODO
  else if (p == "splitText")
    return new DOMTextFunction(static_cast<DOM::Text>(node), DOMTextFunction::SplitText);
  else
    return DOMCharacterData::tryGet(p);
}

DOMTextFunction::DOMTextFunction(DOM::Text t, int i)
  : text(t), id(i)
{
}

Completion DOMTextFunction::tryExecute(const List &args)
{
  KJSO result;

  switch(id) {
    case SplitText:
      result = getDOMNode(text.splitText(args[0].toNumber().intValue()));
      break;
    default:
      result = Undefined();
  }

  return Completion(ReturnValue, result);
}

