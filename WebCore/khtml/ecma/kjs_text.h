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

#ifndef _KJS_TEXT_H_
#define _KJS_TEXT_H_

#include <dom_text.h>

#include <kjs/object.h>
#include <kjs/function.h>

#include "kjs_binding.h"
#include "kjs_dom.h"

namespace KJS {

  class DOMCharacterData : public DOMNode {
  public:
    DOMCharacterData(DOM::CharacterData d) : DOMNode(d) { }
    virtual KJSO tryGet(const UString &p) const;
    virtual void tryPut(const UString &p, const KJSO& v);
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };


  class DOMCharacterDataFunction : public DOMFunction {
  public:
    DOMCharacterDataFunction(DOM::CharacterData d, int i);
    Completion tryExecute(const List &);
    enum { SubstringData, AppendData, InsertData, DeleteData, ReplaceData };
  private:
    DOM::CharacterData data;
    int id;
  };


  class DOMText : public DOMCharacterData {
  public:
    DOMText(DOM::Text t) : DOMCharacterData(t) { }
    virtual KJSO tryGet(const UString &p) const;
    virtual const TypeInfo* typeInfo() const { return &info; }
    static const TypeInfo info;
  };

  class DOMTextFunction : public DOMFunction {
  public:
    DOMTextFunction(DOM::Text t, int i);
    Completion tryExecute(const List &);
    enum { SplitText };
  private:
    DOM::Text text;
    int id;
  };


}; // namespace

#endif
