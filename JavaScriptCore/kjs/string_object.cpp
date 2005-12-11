// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004 Apple Computer, Inc.
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
 *  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "string_object.h"

#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "operations.h"
#include "regexp.h"
#include "regexp_object.h"
#include "error_object.h"
#include <stdio.h>
#include "string_object.lut.h"

using namespace KJS;

// ------------------------------ StringInstanceImp ----------------------------

const ClassInfo StringInstanceImp::info = {"String", 0, 0, 0};

StringInstanceImp::StringInstanceImp(ObjectImp *proto)
  : ObjectImp(proto)
{
  setInternalValue(jsString(""));
}

StringInstanceImp::StringInstanceImp(ObjectImp *proto, const UString &string)
  : ObjectImp(proto)
{
  setInternalValue(jsString(string));
}

ValueImp *StringInstanceImp::lengthGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot &slot)
{
    return jsNumber(static_cast<StringInstanceImp *>(slot.slotBase())->internalValue()->toString(exec).size());
}

ValueImp *StringInstanceImp::indexGetter(ExecState *exec, const Identifier& propertyName, const PropertySlot &slot)
{
    const UChar c = static_cast<StringInstanceImp *>(slot.slotBase())->internalValue()->toString(exec)[slot.index()];
    return jsString(UString(&c, 1));
}

bool StringInstanceImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot &slot)
{
  if (propertyName == lengthPropertyName) {
    slot.setCustom(this, lengthGetter);
    return true;
  }

  bool ok;
  const unsigned index = propertyName.toArrayIndex(&ok);
  if (ok) {
    const UString s = internalValue()->toString(exec);
    const unsigned length = s.size();
    if (index >= length)
      return false;
    slot.setCustomIndex(this, index, indexGetter);
    return true;
  }

  return ObjectImp::getOwnPropertySlot(exec, propertyName, slot);
}

void StringInstanceImp::put(ExecState *exec, const Identifier &propertyName, ValueImp *value, int attr)
{
  if (propertyName == lengthPropertyName)
    return;
  ObjectImp::put(exec, propertyName, value, attr);
}

bool StringInstanceImp::deleteProperty(ExecState *exec, const Identifier &propertyName)
{
  if (propertyName == lengthPropertyName)
    return false;
  return ObjectImp::deleteProperty(exec, propertyName);
}

// ------------------------------ StringPrototypeImp ---------------------------
const ClassInfo StringPrototypeImp::info = {"String", &StringInstanceImp::info, &stringTable, 0};
/* Source for string_object.lut.h
@begin stringTable 26
  toString		StringProtoFuncImp::ToString	DontEnum|Function	0
  valueOf		StringProtoFuncImp::ValueOf	DontEnum|Function	0
  charAt		StringProtoFuncImp::CharAt	DontEnum|Function	1
  charCodeAt		StringProtoFuncImp::CharCodeAt	DontEnum|Function	1
  concat		StringProtoFuncImp::Concat	DontEnum|Function	1
  indexOf		StringProtoFuncImp::IndexOf	DontEnum|Function	1
  lastIndexOf		StringProtoFuncImp::LastIndexOf	DontEnum|Function	1
  match			StringProtoFuncImp::Match	DontEnum|Function	1
  replace		StringProtoFuncImp::Replace	DontEnum|Function	2
  search		StringProtoFuncImp::Search	DontEnum|Function	1
  slice			StringProtoFuncImp::Slice	DontEnum|Function	2
  split			StringProtoFuncImp::Split	DontEnum|Function	2
  substr		StringProtoFuncImp::Substr	DontEnum|Function	2
  substring		StringProtoFuncImp::Substring	DontEnum|Function	2
  toLowerCase		StringProtoFuncImp::ToLowerCase	DontEnum|Function	0
  toUpperCase		StringProtoFuncImp::ToUpperCase	DontEnum|Function	0
  toLocaleLowerCase	StringProtoFuncImp::ToLocaleLowerCase DontEnum|Function	0
  toLocaleUpperCase     StringProtoFuncImp::ToLocaleUpperCase DontEnum|Function	0
#
# Under here: html extension, should only exist if KJS_PURE_ECMA is not defined
# I guess we need to generate two hashtables in the .lut.h file, and use #ifdef
# to select the right one... TODO. #####
  big			StringProtoFuncImp::Big		DontEnum|Function	0
  small			StringProtoFuncImp::Small	DontEnum|Function	0
  blink			StringProtoFuncImp::Blink	DontEnum|Function	0
  bold			StringProtoFuncImp::Bold	DontEnum|Function	0
  fixed			StringProtoFuncImp::Fixed	DontEnum|Function	0
  italics		StringProtoFuncImp::Italics	DontEnum|Function	0
  strike		StringProtoFuncImp::Strike	DontEnum|Function	0
  sub			StringProtoFuncImp::Sub		DontEnum|Function	0
  sup			StringProtoFuncImp::Sup		DontEnum|Function	0
  fontcolor		StringProtoFuncImp::Fontcolor	DontEnum|Function	1
  fontsize		StringProtoFuncImp::Fontsize	DontEnum|Function	1
  anchor		StringProtoFuncImp::Anchor	DontEnum|Function	1
  link			StringProtoFuncImp::Link	DontEnum|Function	1
@end
*/
// ECMA 15.5.4
StringPrototypeImp::StringPrototypeImp(ExecState *exec,
                                       ObjectPrototypeImp *objProto)
  : StringInstanceImp(objProto)
{
  // The constructor will be added later, after StringObjectImp has been built
  putDirect(lengthPropertyName, jsNumber(0), DontDelete|ReadOnly|DontEnum);
}

bool StringPrototypeImp::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot &slot)
{
  return getStaticFunctionSlot<StringProtoFuncImp, StringInstanceImp>(exec, &stringTable, this, propertyName, slot);
}

// ------------------------------ StringProtoFuncImp ---------------------------

StringProtoFuncImp::StringProtoFuncImp(ExecState *exec, int i, int len)
  : InternalFunctionImp(
    static_cast<FunctionPrototypeImp*>(exec->lexicalInterpreter()->builtinFunctionPrototype())
    ), id(i)
{
  putDirect(lengthPropertyName, len, DontDelete|ReadOnly|DontEnum);
}

bool StringProtoFuncImp::implementsCall() const
{
  return true;
}

static inline bool regExpIsGlobal(RegExpImp *regExp, ExecState *exec)
{
    ValueImp *globalProperty = regExp->get(exec,"global");
    return !globalProperty->isUndefined() && globalProperty->toBoolean(exec);
}

static inline void expandSourceRanges(UString::Range * & array, int& count, int& capacity)
{
  int newCapacity;
  if (capacity == 0) {
    newCapacity = 16;
  } else {
    newCapacity = capacity * 2;
  }

  UString::Range *newArray = new UString::Range[newCapacity];
  for (int i = 0; i < count; i++) {
    newArray[i] = array[i];
  }

  delete [] array;

  capacity = newCapacity;
  array = newArray;
}

static void pushSourceRange(UString::Range * & array, int& count, int& capacity, UString::Range range)
{
  if (count + 1 > capacity)
    expandSourceRanges(array, count, capacity);

  array[count] = range;
  count++;
}

static inline void expandReplacements(UString * & array, int& count, int& capacity)
{
  int newCapacity;
  if (capacity == 0) {
    newCapacity = 16;
  } else {
    newCapacity = capacity * 2;
  }

  UString *newArray = new UString[newCapacity];
  for (int i = 0; i < count; i++) {
    newArray[i] = array[i];
  }
  
  delete [] array;

  capacity = newCapacity;
  array = newArray;
}

static void pushReplacement(UString * & array, int& count, int& capacity, UString replacement)
{
  if (count + 1 > capacity)
    expandReplacements(array, count, capacity);

  array[count] = replacement;
  count++;
}

static inline UString substituteBackreferences(const UString &replacement, const UString &source, int *ovector, RegExp *reg)
{
  UString substitutedReplacement = replacement;

  bool converted;

  for (int i = 0; (i = substitutedReplacement.find(UString("$"), i)) != -1; i++) {
    if (i+1 < substitutedReplacement.size() && substitutedReplacement[i+1] == '$') {  // "$$" -> "$"
      substitutedReplacement = substitutedReplacement.substr(0,i) + "$" + substitutedReplacement.substr(i+2);
      continue;
    }
    // Assume number part is one char exactly
    unsigned backrefIndex = substitutedReplacement.substr(i+1,1).toUInt32(&converted, false /* tolerate empty string */);
    if (converted && backrefIndex <= (unsigned)reg->subPatterns()) {
      int backrefStart = ovector[2*backrefIndex];
      int backrefLength = ovector[2*backrefIndex+1] - backrefStart;
      substitutedReplacement = substitutedReplacement.substr(0,i)
        + source.substr(backrefStart, backrefLength)
        + substitutedReplacement.substr(i+2);
      i += backrefLength - 1; // -1 offsets i++
    }
  }

  return substitutedReplacement;
}

static ValueImp *replace(ExecState *exec, const UString &source, ValueImp *pattern, ValueImp *replacement)
{
  ObjectImp *replacementFunction = 0;
  UString replacementString;

  if (replacement->isObject() && replacement->toObject(exec)->implementsCall())
    replacementFunction = replacement->toObject(exec);
  else
    replacementString = replacement->toString(exec);

  if (pattern->isObject() && pattern->toObject(exec)->inherits(&RegExpImp::info)) {
    RegExpImp* imp = static_cast<RegExpImp *>( pattern->toObject(exec) );
    RegExp *reg = imp->regExp();
    bool global = regExpIsGlobal(imp, exec);

    RegExpObjectImp* regExpObj = static_cast<RegExpObjectImp*>(exec->lexicalInterpreter()->builtinRegExp());

    int matchIndex = 0;
    int lastIndex = 0;
    int startPosition = 0;

    UString::Range *sourceRanges = 0;
    int sourceRangeCount = 0;
    int sourceRangeCapacity = 0;
    UString *replacements = 0;
    int replacementCount = 0;
    int replacementCapacity = 0;

    // This is either a loop (if global is set) or a one-way (if not).
    do {
      int *ovector;
      UString matchString = regExpObj->performMatch(reg, source, startPosition, &matchIndex, &ovector);
      if (matchIndex == -1)
        break;
      int matchLen = matchString.size();

      pushSourceRange(sourceRanges, sourceRangeCount, sourceRangeCapacity, UString::Range(lastIndex, matchIndex - lastIndex));

      if (replacementFunction) {
          int completeMatchStart = ovector[0];
          List args;

          args.append(jsString(matchString));
          
          for (unsigned i = 0; i < reg->subPatterns(); i++) {
              int matchStart = ovector[(i + 1) * 2];
              int matchLen = ovector[(i + 1) * 2 + 1] - matchStart;
              
              args.append(jsString(source.substr(matchStart, matchLen)));
          }
          
          args.append(jsNumber(completeMatchStart));
          args.append(jsString(source));

          replacementString = replacementFunction->call(exec, exec->dynamicInterpreter()->globalObject(), 
                                                        args)->toString(exec);
      }
      
      UString substitutedReplacement = substituteBackreferences(replacementString, source, ovector, reg);
      pushReplacement(replacements, replacementCount, replacementCapacity, substitutedReplacement);

      lastIndex = matchIndex + matchLen;
      startPosition = lastIndex;

      // special case of empty match
      if (matchLen == 0) {
        startPosition++;
        if (startPosition > source.size())
          break;
      }
    } while (global);

    if (lastIndex < source.size())
      pushSourceRange(sourceRanges, sourceRangeCount, sourceRangeCapacity, UString::Range(lastIndex, source.size() - lastIndex));

    UString result = source.spliceSubstringsWithSeparators(sourceRanges, sourceRangeCount, replacements, replacementCount);

    delete [] sourceRanges;
    delete [] replacements;

    return jsString(result);
  }
  
  // First arg is a string
  UString patternString = pattern->toString(exec);
  int matchPos = source.find(patternString);
  int matchLen = patternString.size();
  // Do the replacement
  if (matchPos == -1)
    return jsString(source);
  
  if (replacementFunction) {
      List args;
      
      args.append(jsString(source.substr(matchPos, matchLen)));
      args.append(jsNumber(matchPos));
      args.append(jsString(source));
      
      replacementString = replacementFunction->call(exec, exec->dynamicInterpreter()->globalObject(), 
                                                    args)->toString(exec);
  }

  return jsString(source.substr(0, matchPos) + replacementString + source.substr(matchPos + matchLen));
}

// ECMA 15.5.4.2 - 15.5.4.20
ValueImp *StringProtoFuncImp::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
  ValueImp *result = NULL;

  // toString and valueOf are no generic function.
  if (id == ToString || id == ValueOf) {
    if (!thisObj || !thisObj->inherits(&StringInstanceImp::info))
      return throwError(exec, TypeError);

    return jsString(thisObj->internalValue()->toString(exec));
  }

  UString u, u2, u3;
  int pos, p0, i;
  double dpos;
  double d = 0.0;

  UString s = thisObj->toString(exec);

  int len = s.size();
  ValueImp *a0 = args[0];
  ValueImp *a1 = args[1];

  switch (id) {
  case ToString:
  case ValueOf:
    // handled above
    break;
  case CharAt:
    // Other browsers treat an omitted parameter as 0 rather than NaN.
    // That doesn't match the ECMA standard, but is needed for site compatibility.
    dpos = a0->isUndefined() ? 0 : a0->toInteger(exec);
    if (dpos >= 0 && dpos < len) // false for NaN
      u = s.substr(static_cast<int>(dpos), 1);
    else
      u = "";
    result = jsString(u);
    break;
  case CharCodeAt:
    // Other browsers treat an omitted parameter as 0 rather than NaN.
    // That doesn't match the ECMA standard, but is needed for site compatibility.
    dpos = a0->isUndefined() ? 0 : a0->toInteger(exec);
    if (dpos >= 0 && dpos < len) // false for NaN
      result = jsNumber(s[static_cast<int>(dpos)].unicode());
    else
      result = jsNaN();
    break;
  case Concat: {
    ListIterator it = args.begin();
    for ( ; it != args.end() ; ++it) {
        s += it->toString(exec);
    }
    result = jsString(s);
    break;
  }
  case IndexOf:
    u2 = a0->toString(exec);
    if (a1->isUndefined())
      dpos = 0;
    else {
      dpos = a1->toInteger(exec);
      if (dpos >= 0) { // false for NaN
        if (dpos > len)
          dpos = len;
      } else
        dpos = 0;
    }
    result = jsNumber(s.find(u2, static_cast<int>(dpos)));
    break;
  case LastIndexOf:
    u2 = a0->toString(exec);
    d = a1->toNumber(exec);
    if (a1->isUndefined() || KJS::isNaN(d))
      dpos = len;
    else {
      dpos = a1->toInteger(exec);
      if (dpos >= 0) { // false for NaN
        if (dpos > len)
          dpos = len;
      } else
        dpos = 0;
    }
    result = jsNumber(s.rfind(u2, static_cast<int>(dpos)));
    break;
  case Match:
  case Search: {
    u = s;
    RegExp *reg, *tmpReg = 0;
    RegExpImp *imp = 0;
    if (a0->isObject() && a0->getObject()->inherits(&RegExpImp::info)) {
      imp = static_cast<RegExpImp *>(a0);
      reg = imp->regExp();
    } else { 
      /*
       *  ECMA 15.5.4.12 String.prototype.search (regexp)
       *  If regexp is not an object whose [[Class]] property is "RegExp", it is
       *  replaced with the result of the expression new RegExp(regexp).
       */
      reg = tmpReg = new RegExp(a0->toString(exec), RegExp::None);
    }
    RegExpObjectImp* regExpObj = static_cast<RegExpObjectImp*>(exec->lexicalInterpreter()->builtinRegExp());
    UString mstr = regExpObj->performMatch(reg, u, 0, &pos);
    if (id == Search) {
      result = jsNumber(pos);
    } else {
      // Exec
      if ((reg->flags() & RegExp::Global) == 0) {
	// case without 'g' flag is handled like RegExp.prototype.exec
	if (mstr.isNull()) {
	  result = jsNull();
	} else {
	  result = regExpObj->arrayOfMatches(exec,mstr);
	}
      } else {
	// return array of matches
	List list;
	int lastIndex = 0;
	while (pos >= 0) {
          if (mstr.isNull())
            list.append(jsUndefined());
          else
	    list.append(jsString(mstr));
	  lastIndex = pos;
	  pos += mstr.isEmpty() ? 1 : mstr.size();
	  mstr = regExpObj->performMatch(reg, u, pos, &pos);
	}
	if (imp)
	  imp->put(exec, "lastIndex", jsNumber(lastIndex), DontDelete|DontEnum);
	if (list.isEmpty()) {
	  // if there are no matches at all, it's important to return
	  // Null instead of an empty array, because this matches
	  // other browsers and because Null is a false value.
	  result = jsNull(); 
	} else {
	  result = exec->lexicalInterpreter()->builtinArray()->construct(exec, list);
	}
      }
    }
    delete tmpReg;
    break;
  }
  case Replace:
    result = replace(exec, s, a0, a1);
    break;
  case Slice:
    {
      // The arg processing is very much like ArrayProtoFunc::Slice
      double start = a0->toInteger(exec);
      double end = a1->isUndefined() ? len : a1->toInteger(exec);
      double from = start < 0 ? len + start : start;
      double to = end < 0 ? len + end : end;
      if (to > from && to > 0 && from < len) {
        if (from < 0)
          from = 0;
        if (to > len)
          to = len;
        result = jsString(s.substr(static_cast<int>(from), static_cast<int>(to - from)));
      } else {
        result = jsString("");
      }
      break;
    }
    case Split: {
    ObjectImp *constructor = exec->lexicalInterpreter()->builtinArray();
    ObjectImp *res = static_cast<ObjectImp *>(constructor->construct(exec,List::empty()));
    result = res;
    u = s;
    i = p0 = 0;
    uint32_t limit = a1->isUndefined() ? 0xFFFFFFFFU : a1->toUInt32(exec);
    if (a0->isObject() && static_cast<ObjectImp *>(a0)->inherits(&RegExpImp::info)) {
      ObjectImp *obj0 = static_cast<ObjectImp *>(a0);
      RegExp reg(obj0->get(exec,"source")->toString(exec));
      if (u.isEmpty() && !reg.match(u, 0).isNull()) {
	// empty string matched by regexp -> empty array
	res->put(exec,lengthPropertyName, jsNumber(0));
	break;
      }
      pos = 0;
      while (static_cast<uint32_t>(i) != limit && pos < u.size()) {
	// TODO: back references
        int mpos;
        int *ovector = 0L;
	UString mstr = reg.match(u, pos, &mpos, &ovector);
        delete [] ovector; ovector = 0L;
	if (mpos < 0)
	  break;
	pos = mpos + (mstr.isEmpty() ? 1 : mstr.size());
	if (mpos != p0 || !mstr.isEmpty()) {
	  res->put(exec,i, jsString(u.substr(p0, mpos-p0)));
	  p0 = mpos + mstr.size();
	  i++;
	}
      }
    } else {
      u2 = a0->toString(exec);
      if (u2.isEmpty()) {
	if (u.isEmpty()) {
	  // empty separator matches empty string -> empty array
	  put(exec,lengthPropertyName, jsNumber(0));
	  break;
	} else {
	  while (static_cast<uint32_t>(i) != limit && i < u.size()-1)
	    res->put(exec, i++, jsString(u.substr(p0++, 1)));
	}
      } else {
	while (static_cast<uint32_t>(i) != limit && (pos = u.find(u2, p0)) >= 0) {
	  res->put(exec, i, jsString(u.substr(p0, pos-p0)));
	  p0 = pos + u2.size();
	  i++;
	}
      }
    }
    // add remaining string, if any
    if (static_cast<uint32_t>(i) != limit)
      res->put(exec, i++, jsString(u.substr(p0)));
    res->put(exec,lengthPropertyName, jsNumber(i));
    }
    break;
  case Substr: {
    double d = a0->toInteger(exec);
    double d2 = a1->toInteger(exec);
    if (!(d >= 0)) { // true for NaN
      d += len;
      if (!(d >= 0)) // true for NaN
        d = 0;
    }
    if (isNaN(d2))
      d2 = len - d;
    else {
      if (d2 < 0)
        d2 = 0;
      if (d2 > len - d)
        d2 = len - d;
    }
    result = jsString(s.substr(static_cast<int>(d), static_cast<int>(d2)));
    break;
  }
  case Substring: {
    double start = a0->toNumber(exec);
    double end = a1->toNumber(exec);
    if (isNaN(start))
      start = 0;
    if (isNaN(end))
      end = 0;
    if (start < 0)
      start = 0;
    if (end < 0)
      end = 0;
    if (start > len)
      start = len;
    if (end > len)
      end = len;
    if (a1->isUndefined())
      end = len;
    if (start > end) {
      double temp = end;
      end = start;
      start = temp;
    }
    result = jsString(s.substr((int)start, (int)end-(int)start));
    }
    break;
  case ToLowerCase:
  case ToLocaleLowerCase: // FIXME: To get this 100% right we need to detect Turkish and change I to lowercase i without a dot.
    u = s;
    for (i = 0; i < len; i++)
      u[i] = u[i].toLower();
    result = jsString(u);
    break;
  case ToUpperCase:
  case ToLocaleUpperCase: // FIXME: To get this 100% right we need to detect Turkish and change i to uppercase I with a dot.
    u = s;
    for (i = 0; i < len; i++)
      u[i] = u[i].toUpper();
    result = jsString(u);
    break;
#ifndef KJS_PURE_ECMA
  case Big:
    result = jsString("<big>" + s + "</big>");
    break;
  case Small:
    result = jsString("<small>" + s + "</small>");
    break;
  case Blink:
    result = jsString("<blink>" + s + "</blink>");
    break;
  case Bold:
    result = jsString("<b>" + s + "</b>");
    break;
  case Fixed:
    result = jsString("<tt>" + s + "</tt>");
    break;
  case Italics:
    result = jsString("<i>" + s + "</i>");
    break;
  case Strike:
    result = jsString("<strike>" + s + "</strike>");
    break;
  case Sub:
    result = jsString("<sub>" + s + "</sub>");
    break;
  case Sup:
    result = jsString("<sup>" + s + "</sup>");
    break;
  case Fontcolor:
    result = jsString("<font color=\"" + a0->toString(exec) + "\">" + s + "</font>");
    break;
  case Fontsize:
    result = jsString("<font size=\"" + a0->toString(exec) + "\">" + s + "</font>");
    break;
  case Anchor:
    result = jsString("<a name=\"" + a0->toString(exec) + "\">" + s + "</a>");
    break;
  case Link:
    result = jsString("<a href=\"" + a0->toString(exec) + "\">" + s + "</a>");
    break;
#endif
  }

  return result;
}

// ------------------------------ StringObjectImp ------------------------------

StringObjectImp::StringObjectImp(ExecState *exec,
                                 FunctionPrototypeImp *funcProto,
                                 StringPrototypeImp *stringProto)
  : InternalFunctionImp(funcProto)
{
  // ECMA 15.5.3.1 String.prototype
  putDirect(prototypePropertyName, stringProto, DontEnum|DontDelete|ReadOnly);

  putDirect(fromCharCodePropertyName, new StringObjectFuncImp(exec, funcProto), DontEnum);

  // no. of arguments for constructor
  putDirect(lengthPropertyName, jsNumber(1), ReadOnly|DontDelete|DontEnum);
}


bool StringObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.5.2
ObjectImp *StringObjectImp::construct(ExecState *exec, const List &args)
{
  ObjectImp *proto = exec->lexicalInterpreter()->builtinStringPrototype();
  if (args.size() == 0)
    return new StringInstanceImp(proto);
  return new StringInstanceImp(proto, args.begin()->toString(exec));
}

bool StringObjectImp::implementsCall() const
{
  return true;
}

// ECMA 15.5.1
ValueImp *StringObjectImp::callAsFunction(ExecState *exec, ObjectImp */*thisObj*/, const List &args)
{
  if (args.isEmpty())
    return jsString("");
  else {
    ValueImp *v = args[0];
    return jsString(v->toString(exec));
  }
}

// ------------------------------ StringObjectFuncImp --------------------------

// ECMA 15.5.3.2 fromCharCode()
StringObjectFuncImp::StringObjectFuncImp(ExecState *exec, FunctionPrototypeImp *funcProto)
  : InternalFunctionImp(funcProto)
{
  putDirect(lengthPropertyName, jsNumber(1), DontDelete|ReadOnly|DontEnum);
}

bool StringObjectFuncImp::implementsCall() const
{
  return true;
}

ValueImp *StringObjectFuncImp::callAsFunction(ExecState *exec, ObjectImp */*thisObj*/, const List &args)
{
  UString s;
  if (args.size()) {
    UChar *buf = static_cast<UChar *>(fastMalloc(args.size() * sizeof(UChar)));
    UChar *p = buf;
    ListIterator it = args.begin();
    while (it != args.end()) {
      unsigned short u = it->toUInt16(exec);
      *p++ = UChar(u);
      it++;
    }
    s = UString(buf, args.size(), false);
  } else
    s = "";

  return jsString(s);
}
