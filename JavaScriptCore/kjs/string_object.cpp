// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "string_object.h"
#include "string_object.lut.h"

#include "JSWrapperObject.h"
#include "PropertyNameArray.h"
#include "array_object.h"
#include "error_object.h"
#include "operations.h"
#include "regexp_object.h"
#include <wtf/MathExtras.h>
#include <wtf/unicode/Unicode.h>

#if PLATFORM(CF)
#include <CoreFoundation/CoreFoundation.h>
#elif PLATFORM(WIN_OS)
#include <windows.h>
#endif

using namespace WTF;

namespace KJS {

// ------------------------------ StringInstance ----------------------------

const ClassInfo StringInstance::info = { "String", 0, 0 };

StringInstance::StringInstance(JSObject *proto)
  : JSWrapperObject(proto)
{
  setInternalValue(jsString(""));
}

StringInstance::StringInstance(JSObject *proto, StringImp* string)
  : JSWrapperObject(proto)
{
  setInternalValue(string);
}

StringInstance::StringInstance(JSObject *proto, const UString &string)
  : JSWrapperObject(proto)
{
  setInternalValue(jsString(string));
}

JSValue *StringInstance::lengthGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot &slot)
{
    return jsNumber(static_cast<StringInstance*>(slot.slotBase())->internalValue()->value().size());
}

JSValue* StringInstance::indexGetter(ExecState*, JSObject*, const Identifier&, const PropertySlot& slot)
{
    return jsString(static_cast<StringInstance*>(slot.slotBase())->internalValue()->value().substr(slot.index(), 1));
}

static JSValue* stringInstanceNumericPropertyGetter(ExecState*, JSObject*, unsigned index, const PropertySlot& slot)
{
    return jsString(static_cast<StringInstance*>(slot.slotBase())->internalValue()->value().substr(index, 1));
}

bool StringInstance::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (propertyName == exec->propertyNames().length) {
        slot.setCustom(this, lengthGetter);
        return true;
    }

    bool isStrictUInt32;
    unsigned i = propertyName.toStrictUInt32(&isStrictUInt32);
    unsigned length = internalValue()->value().size();
    if (isStrictUInt32 && i < length) {
        slot.setCustomIndex(this, i, indexGetter);
        return true;
    }
    
    return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}
    
bool StringInstance::getOwnPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    unsigned length = internalValue()->value().size();
    if (propertyName < length) {
        slot.setCustomNumeric(this, stringInstanceNumericPropertyGetter);
        return true;
    }
    
    return JSObject::getOwnPropertySlot(exec, Identifier::from(propertyName), slot);
}

void StringInstance::put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr)
{
  if (propertyName == exec->propertyNames().length)
    return;
  JSObject::put(exec, propertyName, value, attr);
}

bool StringInstance::deleteProperty(ExecState *exec, const Identifier &propertyName)
{
  if (propertyName == exec->propertyNames().length)
    return false;
  return JSObject::deleteProperty(exec, propertyName);
}

void StringInstance::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
  int size = internalValue()->getString().size();
  for (int i = 0; i < size; i++)
    propertyNames.add(Identifier(UString::from(i)));
  return JSObject::getPropertyNames(exec, propertyNames);
}

// ------------------------------ StringPrototype ---------------------------
const ClassInfo StringPrototype::info = { "String", &StringInstance::info, &stringTable };
/* Source for string_object.lut.h
@begin stringTable 26
  toString              &stringProtoFuncToString          DontEnum|Function       0
  valueOf               &stringProtoFuncValueOf           DontEnum|Function       0
  charAt                &stringProtoFuncCharAt            DontEnum|Function       1
  charCodeAt            &stringProtoFuncCharCodeAt        DontEnum|Function       1
  concat                &stringProtoFuncConcat            DontEnum|Function       1
  indexOf               &stringProtoFuncIndexOf           DontEnum|Function       1
  lastIndexOf           &stringProtoFuncLastIndexOf       DontEnum|Function       1
  match                 &stringProtoFuncMatch             DontEnum|Function       1
  replace               &stringProtoFuncReplace           DontEnum|Function       2
  search                &stringProtoFuncSearch            DontEnum|Function       1
  slice                 &stringProtoFuncSlice             DontEnum|Function       2
  split                 &stringProtoFuncSplit             DontEnum|Function       2
  substr                &stringProtoFuncSubstr            DontEnum|Function       2
  substring             &stringProtoFuncSubstring         DontEnum|Function       2
  toLowerCase           &stringProtoFuncToLowerCase       DontEnum|Function       0
  toUpperCase           &stringProtoFuncToUpperCase       DontEnum|Function       0
  toLocaleLowerCase     &stringProtoFuncToLocaleLowerCase DontEnum|Function       0
  toLocaleUpperCase     &stringProtoFuncToLocaleUpperCase DontEnum|Function       0
  localeCompare         &stringProtoFuncLocaleCompare     DontEnum|Function       1

  big                   &stringProtoFuncBig               DontEnum|Function       0
  small                 &stringProtoFuncSmall             DontEnum|Function       0
  blink                 &stringProtoFuncBlink             DontEnum|Function       0
  bold                  &stringProtoFuncBold              DontEnum|Function       0
  fixed                 &stringProtoFuncFixed             DontEnum|Function       0
  italics               &stringProtoFuncItalics           DontEnum|Function       0
  strike                &stringProtoFuncStrike            DontEnum|Function       0
  sub                   &stringProtoFuncSub               DontEnum|Function       0
  sup                   &stringProtoFuncSup               DontEnum|Function       0
  fontcolor             &stringProtoFuncFontcolor         DontEnum|Function       1
  fontsize              &stringProtoFuncFontsize          DontEnum|Function       1
  anchor                &stringProtoFuncAnchor            DontEnum|Function       1
  link                  &stringProtoFuncLink              DontEnum|Function       1
@end
*/
// ECMA 15.5.4
StringPrototype::StringPrototype(ExecState* exec, ObjectPrototype* objProto)
  : StringInstance(objProto)
{
  // The constructor will be added later, after StringObjectImp has been built
  putDirect(exec->propertyNames().length, jsNumber(0), DontDelete | ReadOnly | DontEnum);
}

bool StringPrototype::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot &slot)
{
  return getStaticFunctionSlot<StringInstance>(exec, &stringTable, this, propertyName, slot);
}

// ------------------------------ Functions --------------------------

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

  int i = -1;
  while ((i = substitutedReplacement.find(UString("$"), i + 1)) != -1) {
    if (i+1 == substitutedReplacement.size())
        break;

    unsigned short ref = substitutedReplacement[i+1].unicode();
    int backrefStart = 0;
    int backrefLength = 0;
    int advance = 0;

    if (ref == '$') {  // "$$" -> "$"
        substitutedReplacement = substitutedReplacement.substr(0, i + 1) + substitutedReplacement.substr(i + 2);
        continue;
    } else if (ref == '&') {
        backrefStart = ovector[0];
        backrefLength = ovector[1] - backrefStart;
    } else if (ref == '`') {
        backrefStart = 0;
        backrefLength = ovector[0];
    } else if (ref == '\'') {
        backrefStart = ovector[1];
        backrefLength = source.size() - backrefStart;
    } else if (ref >= '0' && ref <= '9') {
        // 1- and 2-digit back references are allowed
        unsigned backrefIndex = ref - '0';
        if (backrefIndex > reg->numSubpatterns())
            continue;
        if (substitutedReplacement.size() > i + 2) {
            ref = substitutedReplacement[i+2].unicode();
            if (ref >= '0' && ref <= '9') {
                backrefIndex = 10 * backrefIndex + ref - '0';
                if (backrefIndex > reg->numSubpatterns())
                    backrefIndex = backrefIndex / 10;   // Fall back to the 1-digit reference
                else
                    advance = 1;
            }
        }
        backrefStart = ovector[2 * backrefIndex];
        backrefLength = ovector[2 * backrefIndex + 1] - backrefStart;
    } else
        continue;

    substitutedReplacement = substitutedReplacement.substr(0, i) + source.substr(backrefStart, backrefLength) + substitutedReplacement.substr(i + 2 + advance);
    i += backrefLength - 1; // - 1 offsets 'i + 1'
  }

  return substitutedReplacement;
}
static inline int localeCompare(const UString& a, const UString& b)
{
#if PLATFORM(WIN_OS)
    int retval = CompareStringW(LOCALE_USER_DEFAULT, 0,
                                reinterpret_cast<LPCWSTR>(a.data()), a.size(),
                                reinterpret_cast<LPCWSTR>(b.data()), b.size());
    return !retval ? retval : retval - 2;
#elif PLATFORM(CF)
    CFStringRef sa = CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(a.data()), a.size(), kCFAllocatorNull);
    CFStringRef sb = CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(b.data()), b.size(), kCFAllocatorNull);

    int retval = CFStringCompare(sa, sb, kCFCompareLocalized);

    CFRelease(sa);
    CFRelease(sb);

    return retval;
#else
    return compare(a, b);
#endif
}

static JSValue *replace(ExecState *exec, StringImp* sourceVal, JSValue *pattern, JSValue *replacement)
{
  UString source = sourceVal->value();
  JSObject *replacementFunction = 0;
  UString replacementString;

  if (replacement->isObject() && replacement->toObject(exec)->implementsCall())
    replacementFunction = replacement->toObject(exec);
  else
    replacementString = replacement->toString(exec);

  if (pattern->isObject() && static_cast<JSObject *>(pattern)->inherits(&RegExpImp::info)) {
    RegExp *reg = static_cast<RegExpImp *>(pattern)->regExp();
    bool global = reg->global();

    RegExpObjectImp* regExpObj = static_cast<RegExpObjectImp*>(exec->lexicalGlobalObject()->regExpConstructor());

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
      int matchIndex;
      int matchLen;
      int* ovector;
      regExpObj->performMatch(reg, source, startPosition, matchIndex, matchLen, &ovector);
      if (matchIndex < 0)
        break;

      pushSourceRange(sourceRanges, sourceRangeCount, sourceRangeCapacity, UString::Range(lastIndex, matchIndex - lastIndex));

      UString substitutedReplacement;
      if (replacementFunction) {
          int completeMatchStart = ovector[0];
          List args;

          for (unsigned i = 0; i < reg->numSubpatterns() + 1; i++) {
              int matchStart = ovector[i * 2];
              int matchLen = ovector[i * 2 + 1] - matchStart;

              if (matchStart < 0)
                args.append(jsUndefined());
              else
                args.append(jsString(source.substr(matchStart, matchLen)));
          }
          
          args.append(jsNumber(completeMatchStart));
          args.append(sourceVal);

          substitutedReplacement = replacementFunction->call(exec, exec->dynamicGlobalObject(), 
                                                             args)->toString(exec);
      } else
          substitutedReplacement = substituteBackreferences(replacementString, source, ovector, reg);

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

    UString result;

    if (sourceRanges)
        result = source.spliceSubstringsWithSeparators(sourceRanges, sourceRangeCount, replacements, replacementCount);

    delete [] sourceRanges;
    delete [] replacements;

    if (result == source)
      return sourceVal;

    return jsString(result);
  }
  
  // First arg is a string
  UString patternString = pattern->toString(exec);
  int matchPos = source.find(patternString);
  int matchLen = patternString.size();
  // Do the replacement
  if (matchPos == -1)
    return sourceVal;
  
  if (replacementFunction) {
      List args;
      
      args.append(jsString(source.substr(matchPos, matchLen)));
      args.append(jsNumber(matchPos));
      args.append(sourceVal);
      
      replacementString = replacementFunction->call(exec, exec->dynamicGlobalObject(), 
                                                    args)->toString(exec);
  }

  return jsString(source.substr(0, matchPos) + replacementString + source.substr(matchPos + matchLen));
}

JSValue* stringProtoFuncToString(ExecState* exec, JSObject* thisObj, const List&)
{
    if (!thisObj->inherits(&StringInstance::info))
        return throwError(exec, TypeError);

    return static_cast<StringInstance*>(thisObj)->internalValue();
}

JSValue* stringProtoFuncValueOf(ExecState* exec, JSObject* thisObj, const List&)
{
    if (!thisObj->inherits(&StringInstance::info))
        return throwError(exec, TypeError);

    return static_cast<StringInstance*>(thisObj)->internalValue();
}

JSValue* stringProtoFuncCharAt(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    int len = s.size();

    UString u;
    JSValue* a0 = args[0];
    double dpos = a0->toInteger(exec);
    if (dpos >= 0 && dpos < len)
      u = s.substr(static_cast<int>(dpos), 1);
    else
      u = "";
    return jsString(u);
}

JSValue* stringProtoFuncCharCodeAt(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    int len = s.size();

    JSValue* result = 0;

    JSValue* a0 = args[0];
    double dpos = a0->toInteger(exec);
    if (dpos >= 0 && dpos < len)
      result = jsNumber(s[static_cast<int>(dpos)].unicode());
    else
      result = jsNaN();
    return result;
}

JSValue* stringProtoFuncConcat(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);

    List::const_iterator end = args.end();
    for (List::const_iterator it = args.begin(); it != end; ++it) {
        s += (*it)->toString(exec);
    }
    return jsString(s);
}

JSValue* stringProtoFuncIndexOf(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    int len = s.size();

    JSValue* a0 = args[0];
    JSValue* a1 = args[1];
    UString u2 = a0->toString(exec);
    double dpos = a1->toInteger(exec);
    if (dpos < 0)
        dpos = 0;
    else if (dpos > len)
        dpos = len;
    return jsNumber(s.find(u2, static_cast<int>(dpos)));
}

JSValue* stringProtoFuncLastIndexOf(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    int len = s.size();

    JSValue* a0 = args[0];
    JSValue* a1 = args[1];
    
    UString u2 = a0->toString(exec);
    double dpos = a1->toIntegerPreserveNaN(exec);
    if (dpos < 0)
        dpos = 0;
    else if (!(dpos <= len)) // true for NaN
        dpos = len;
    return jsNumber(s.rfind(u2, static_cast<int>(dpos)));
}

JSValue* stringProtoFuncMatch(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);

    JSValue* a0 = args[0];

    UString u = s;
    RegExp* reg;
    RegExp* tmpReg = 0;
    RegExpImp* imp = 0;
    if (a0->isObject() && static_cast<JSObject *>(a0)->inherits(&RegExpImp::info)) {
      reg = static_cast<RegExpImp *>(a0)->regExp();
    } else { 
      /*
       *  ECMA 15.5.4.12 String.prototype.search (regexp)
       *  If regexp is not an object whose [[Class]] property is "RegExp", it is
       *  replaced with the result of the expression new RegExp(regexp).
       */
      reg = tmpReg = new RegExp(a0->toString(exec));
    }
    RegExpObjectImp* regExpObj = static_cast<RegExpObjectImp*>(exec->lexicalGlobalObject()->regExpConstructor());
    int pos;
    int matchLength;
    regExpObj->performMatch(reg, u, 0, pos, matchLength);
    JSValue* result;
    if (!(reg->global())) {
      // case without 'g' flag is handled like RegExp.prototype.exec
      if (pos < 0)
        result = jsNull();
      else
        result = regExpObj->arrayOfMatches(exec);
    } else {
      // return array of matches
      List list;
      int lastIndex = 0;
      while (pos >= 0) {
        list.append(jsString(u.substr(pos, matchLength)));
        lastIndex = pos;
        pos += matchLength == 0 ? 1 : matchLength;
        regExpObj->performMatch(reg, u, pos, pos, matchLength);
      }
      if (imp)
        imp->setLastIndex(lastIndex);
      if (list.isEmpty()) {
        // if there are no matches at all, it's important to return
        // Null instead of an empty array, because this matches
        // other browsers and because Null is a false value.
        result = jsNull();
      } else {
        result = exec->lexicalGlobalObject()->arrayConstructor()->construct(exec, list);
      }
    }
    delete tmpReg;
    return result;
}

JSValue* stringProtoFuncSearch(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);

    JSValue* a0 = args[0];

    UString u = s;
    RegExp* reg;
    RegExp* tmpReg = 0;
    if (a0->isObject() && static_cast<JSObject *>(a0)->inherits(&RegExpImp::info)) {
      reg = static_cast<RegExpImp *>(a0)->regExp();
    } else { 
      /*
       *  ECMA 15.5.4.12 String.prototype.search (regexp)
       *  If regexp is not an object whose [[Class]] property is "RegExp", it is
       *  replaced with the result of the expression new RegExp(regexp).
       */
      reg = tmpReg = new RegExp(a0->toString(exec));
    }
    RegExpObjectImp* regExpObj = static_cast<RegExpObjectImp*>(exec->lexicalGlobalObject()->regExpConstructor());
    int pos;
    int matchLength;
    regExpObj->performMatch(reg, u, 0, pos, matchLength);
    delete tmpReg;
    return jsNumber(pos);
}

JSValue* stringProtoFuncReplace(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);

    StringImp* sVal = thisObj->inherits(&StringInstance::info) ?
      static_cast<StringInstance*>(thisObj)->internalValue() :
      static_cast<StringImp*>(jsString(s));

    JSValue* a0 = args[0];
    JSValue* a1 = args[1];

    return replace(exec, sVal, a0, a1);
}

JSValue* stringProtoFuncSlice(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    int len = s.size();

    JSValue* a0 = args[0];
    JSValue* a1 = args[1];

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
        return jsString(s.substr(static_cast<int>(from), static_cast<int>(to - from)));
    }

    return jsString("");
}

JSValue* stringProtoFuncSplit(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);

    JSValue* a0 = args[0];
    JSValue* a1 = args[1];

    JSObject *constructor = exec->lexicalGlobalObject()->arrayConstructor();
    JSObject* res = static_cast<JSObject*>(constructor->construct(exec, exec->emptyList()));
    JSValue* result = res;
    UString u = s;
    int pos;
    int i = 0;
    int p0 = 0;
    uint32_t limit = a1->isUndefined() ? 0xFFFFFFFFU : a1->toUInt32(exec);
    if (a0->isObject() && static_cast<JSObject *>(a0)->inherits(&RegExpImp::info)) {
      RegExp *reg = static_cast<RegExpImp *>(a0)->regExp();
      if (u.isEmpty() && reg->match(u, 0) >= 0) {
        // empty string matched by regexp -> empty array
        res->put(exec, exec->propertyNames().length, jsNumber(0));
        return result;
      }
      pos = 0;
      while (static_cast<uint32_t>(i) != limit && pos < u.size()) {
        OwnArrayPtr<int> ovector;
        int mpos = reg->match(u, pos, &ovector);
        if (mpos < 0)
          break;
        int mlen = ovector[1] - ovector[0];
        pos = mpos + (mlen == 0 ? 1 : mlen);
        if (mpos != p0 || mlen) {
          res->put(exec,i, jsString(u.substr(p0, mpos-p0)));
          p0 = mpos + mlen;
          i++;
        }
        for (unsigned si = 1; si <= reg->numSubpatterns(); ++si) {
          int spos = ovector[si * 2];
          if (spos < 0)
            res->put(exec, i++, jsUndefined());
          else
            res->put(exec, i++, jsString(u.substr(spos, ovector[si * 2 + 1] - spos)));
        }
      }
    } else {
      UString u2 = a0->toString(exec);
      if (u2.isEmpty()) {
        if (u.isEmpty()) {
          // empty separator matches empty string -> empty array
          res->put(exec, exec->propertyNames().length, jsNumber(0));
          return result;
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
    res->put(exec, exec->propertyNames().length, jsNumber(i));
    return result;
}

JSValue* stringProtoFuncSubstr(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    int len = s.size();

    JSValue* a0 = args[0];
    JSValue* a1 = args[1];

    double start = a0->toInteger(exec);
    double length = a1->isUndefined() ? len : a1->toInteger(exec);
    if (start >= len)
      return jsString("");
    if (length < 0)
      return jsString("");
    if (start < 0) {
      start += len;
      if (start < 0)
        start = 0;
    }
    if (length > len)
      length = len;
    return jsString(s.substr(static_cast<int>(start), static_cast<int>(length)));
}

JSValue* stringProtoFuncSubstring(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    int len = s.size();

    JSValue* a0 = args[0];
    JSValue* a1 = args[1];

    double start = a0->toNumber(exec);
    double end = a1->toNumber(exec);
    if (isnan(start))
      start = 0;
    if (isnan(end))
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
    return jsString(s.substr((int)start, (int)end-(int)start));
}

JSValue* stringProtoFuncToLowerCase(ExecState* exec, JSObject* thisObj, const List&)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    
    StringImp* sVal = thisObj->inherits(&StringInstance::info)
        ? static_cast<StringInstance*>(thisObj)->internalValue()
        : static_cast<StringImp*>(jsString(s));
    int ssize = s.size();
    if (!ssize)
        return sVal;
    Vector< ::UChar> buffer(ssize);
    bool error;
    int length = Unicode::toLower(buffer.data(), ssize, reinterpret_cast<const ::UChar*>(s.data()), ssize, &error);
    if (error) {
        buffer.resize(length);
        length = Unicode::toLower(buffer.data(), length, reinterpret_cast<const ::UChar*>(s.data()), ssize, &error);
        if (error)
            return sVal;
    }
    if (length == ssize && memcmp(buffer.data(), s.data(), length * sizeof(UChar)) == 0)
        return sVal;
    return jsString(UString(reinterpret_cast<UChar*>(buffer.releaseBuffer()), length, false));
}

JSValue* stringProtoFuncToUpperCase(ExecState* exec, JSObject* thisObj, const List&)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);

    StringImp* sVal = thisObj->inherits(&StringInstance::info)
        ? static_cast<StringInstance*>(thisObj)->internalValue()
        : static_cast<StringImp*>(jsString(s));
    int ssize = s.size();
    if (!ssize)
        return sVal;
    Vector< ::UChar> buffer(ssize);
    bool error;
    int length = Unicode::toUpper(buffer.data(), ssize, reinterpret_cast<const ::UChar*>(s.data()), ssize, &error);
    if (error) {
        buffer.resize(length);
        length = Unicode::toUpper(buffer.data(), length, reinterpret_cast<const ::UChar*>(s.data()), ssize, &error);
        if (error)
            return sVal;
    }
    if (length == ssize && memcmp(buffer.data(), s.data(), length * sizeof(UChar)) == 0)
        return sVal;
    return jsString(UString(reinterpret_cast<UChar*>(buffer.releaseBuffer()), length, false));
}

JSValue* stringProtoFuncToLocaleLowerCase(ExecState* exec, JSObject* thisObj, const List&)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    
    // FIXME: See http://www.unicode.org/Public/UNIDATA/SpecialCasing.txt for locale-sensitive mappings that aren't implemented.
    StringImp* sVal = thisObj->inherits(&StringInstance::info)
        ? static_cast<StringInstance*>(thisObj)->internalValue()
        : static_cast<StringImp*>(jsString(s));
    int ssize = s.size();
    if (!ssize)
        return sVal;
    Vector< ::UChar> buffer(ssize);
    bool error;
    int length = Unicode::toLower(buffer.data(), ssize, reinterpret_cast<const ::UChar*>(s.data()), ssize, &error);
    if (error) {
        buffer.resize(length);
        length = Unicode::toLower(buffer.data(), length, reinterpret_cast<const ::UChar*>(s.data()), ssize, &error);
        if (error)
            return sVal;
    }
    if (length == ssize && memcmp(buffer.data(), s.data(), length * sizeof(UChar)) == 0)
        return sVal;
    return jsString(UString(reinterpret_cast<UChar*>(buffer.releaseBuffer()), length, false));
}

JSValue* stringProtoFuncToLocaleUpperCase(ExecState* exec, JSObject* thisObj, const List&)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);

    StringImp* sVal = thisObj->inherits(&StringInstance::info)
        ? static_cast<StringInstance*>(thisObj)->internalValue()
        : static_cast<StringImp*>(jsString(s));
    int ssize = s.size();
    if (!ssize)
        return sVal;
    Vector< ::UChar> buffer(ssize);
    bool error;
    int length = Unicode::toUpper(buffer.data(), ssize, reinterpret_cast<const ::UChar*>(s.data()), ssize, &error);
    if (error) {
        buffer.resize(length);
        length = Unicode::toUpper(buffer.data(), length, reinterpret_cast<const ::UChar*>(s.data()), ssize, &error);
        if (error)
            return sVal;
    }
    if (length == ssize && memcmp(buffer.data(), s.data(), length * sizeof(UChar)) == 0)
        return sVal;
    return jsString(UString(reinterpret_cast<UChar*>(buffer.releaseBuffer()), length, false));
}

JSValue* stringProtoFuncLocaleCompare(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (args.size() < 1)
      return jsNumber(0);

    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    JSValue* a0 = args[0];
    return jsNumber(localeCompare(s, a0->toString(exec)));
}

JSValue* stringProtoFuncBig(ExecState* exec, JSObject* thisObj, const List&)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    return jsString("<big>" + s + "</big>");
}

JSValue* stringProtoFuncSmall(ExecState* exec, JSObject* thisObj, const List&)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    return jsString("<small>" + s + "</small>");
}

JSValue* stringProtoFuncBlink(ExecState* exec, JSObject* thisObj, const List&)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    return jsString("<blink>" + s + "</blink>");
}

JSValue* stringProtoFuncBold(ExecState* exec, JSObject* thisObj, const List&)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    return jsString("<b>" + s + "</b>");
}

JSValue* stringProtoFuncFixed(ExecState* exec, JSObject* thisObj, const List&)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    return jsString("<tt>" + s + "</tt>");
}

JSValue* stringProtoFuncItalics(ExecState* exec, JSObject* thisObj, const List&)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    return jsString("<i>" + s + "</i>");
}

JSValue* stringProtoFuncStrike(ExecState* exec, JSObject* thisObj, const List&)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    return jsString("<strike>" + s + "</strike>");
}

JSValue* stringProtoFuncSub(ExecState* exec, JSObject* thisObj, const List&)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    return jsString("<sub>" + s + "</sub>");
}

JSValue* stringProtoFuncSup(ExecState* exec, JSObject* thisObj, const List&)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    return jsString("<sup>" + s + "</sup>");
}

JSValue* stringProtoFuncFontcolor(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    JSValue* a0 = args[0];
    return jsString("<font color=\"" + a0->toString(exec) + "\">" + s + "</font>");
}

JSValue* stringProtoFuncFontsize(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    JSValue* a0 = args[0];
    return jsString("<font size=\"" + a0->toString(exec) + "\">" + s + "</font>");
}

JSValue* stringProtoFuncAnchor(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    JSValue* a0 = args[0];
    return jsString("<a name=\"" + a0->toString(exec) + "\">" + s + "</a>");
}

JSValue* stringProtoFuncLink(ExecState* exec, JSObject* thisObj, const List& args)
{
    // This optimizes the common case that thisObj is a StringInstance
    UString s = thisObj->inherits(&StringInstance::info) ? static_cast<StringInstance*>(thisObj)->internalValue()->value() : thisObj->toString(exec);
    JSValue* a0 = args[0];
    return jsString("<a href=\"" + a0->toString(exec) + "\">" + s + "</a>");
}

// ------------------------------ StringObjectImp ------------------------------

StringObjectImp::StringObjectImp(ExecState* exec, FunctionPrototype* funcProto, StringPrototype* stringProto)
  : InternalFunctionImp(funcProto, stringProto->classInfo()->className)
{
  // ECMA 15.5.3.1 String.prototype
  putDirect(exec->propertyNames().prototype, stringProto, DontEnum|DontDelete|ReadOnly);

  putDirectFunction(new StringObjectFuncImp(exec, funcProto, exec->propertyNames().fromCharCode), DontEnum);

  // no. of arguments for constructor
  putDirect(exec->propertyNames().length, jsNumber(1), ReadOnly|DontDelete|DontEnum);
}


bool StringObjectImp::implementsConstruct() const
{
  return true;
}

// ECMA 15.5.2
JSObject *StringObjectImp::construct(ExecState *exec, const List &args)
{
  JSObject *proto = exec->lexicalGlobalObject()->stringPrototype();
  if (args.size() == 0)
    return new StringInstance(proto);
  return new StringInstance(proto, args[0]->toString(exec));
}

// ECMA 15.5.1
JSValue *StringObjectImp::callAsFunction(ExecState *exec, JSObject* /*thisObj*/, const List &args)
{
  if (args.isEmpty())
    return jsString("");
  else {
    JSValue *v = args[0];
    return jsString(v->toString(exec));
  }
}

// ------------------------------ StringObjectFuncImp --------------------------

// ECMA 15.5.3.2 fromCharCode()
StringObjectFuncImp::StringObjectFuncImp(ExecState* exec, FunctionPrototype* funcProto, const Identifier& name)
  : InternalFunctionImp(funcProto, name)
{
  putDirect(exec->propertyNames().length, jsNumber(1), DontDelete|ReadOnly|DontEnum);
}

JSValue *StringObjectFuncImp::callAsFunction(ExecState *exec, JSObject* /*thisObj*/, const List &args)
{
  UString s;
  if (args.size()) {
    UChar *buf = static_cast<UChar *>(fastMalloc(args.size() * sizeof(UChar)));
    UChar *p = buf;
    List::const_iterator end = args.end();
    for (List::const_iterator it = args.begin(); it != end; ++it) {
      unsigned short u = static_cast<unsigned short>((*it)->toUInt32(exec));
      *p++ = UChar(u);
    }
    s = UString(buf, args.size(), false);
  } else
    s = "";

  return jsString(s);
}

} // namespace KJS
