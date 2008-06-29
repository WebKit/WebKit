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
#include "StringPrototype.h"

#include "JSArray.h"
#include "ObjectPrototype.h"
#include "PropertyNameArray.h"
#include "RegExpConstructor.h"
#include "RegExpObject.h"
#include <wtf/MathExtras.h>
#include <wtf/unicode/Collator.h>

using namespace WTF;

namespace KJS {

static JSValue* stringProtoFuncToString(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncCharAt(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncCharCodeAt(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncConcat(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncIndexOf(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncLastIndexOf(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncMatch(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncReplace(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncSearch(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncSlice(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncSplit(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncSubstr(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncSubstring(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncToLowerCase(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncToUpperCase(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncToLocaleLowerCase(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncToLocaleUpperCase(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncLocaleCompare(ExecState*, JSObject*, JSValue*, const ArgList&);

static JSValue* stringProtoFuncBig(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncSmall(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncBlink(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncBold(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncFixed(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncItalics(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncStrike(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncSub(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncSup(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncFontcolor(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncFontsize(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncAnchor(ExecState*, JSObject*, JSValue*, const ArgList&);
static JSValue* stringProtoFuncLink(ExecState*, JSObject*, JSValue*, const ArgList&);

}

#include "StringPrototype.lut.h"

namespace KJS {

const ClassInfo StringPrototype::info = { "String", &StringObject::info, 0, ExecState::stringTable };

/* Source for StringPrototype.lut.h
@begin stringTable 26
  toString              stringProtoFuncToString          DontEnum|Function       0
  valueOf               stringProtoFuncToString          DontEnum|Function       0
  charAt                stringProtoFuncCharAt            DontEnum|Function       1
  charCodeAt            stringProtoFuncCharCodeAt        DontEnum|Function       1
  concat                stringProtoFuncConcat            DontEnum|Function       1
  indexOf               stringProtoFuncIndexOf           DontEnum|Function       1
  lastIndexOf           stringProtoFuncLastIndexOf       DontEnum|Function       1
  match                 stringProtoFuncMatch             DontEnum|Function       1
  replace               stringProtoFuncReplace           DontEnum|Function       2
  search                stringProtoFuncSearch            DontEnum|Function       1
  slice                 stringProtoFuncSlice             DontEnum|Function       2
  split                 stringProtoFuncSplit             DontEnum|Function       2
  substr                stringProtoFuncSubstr            DontEnum|Function       2
  substring             stringProtoFuncSubstring         DontEnum|Function       2
  toLowerCase           stringProtoFuncToLowerCase       DontEnum|Function       0
  toUpperCase           stringProtoFuncToUpperCase       DontEnum|Function       0
  toLocaleLowerCase     stringProtoFuncToLocaleLowerCase DontEnum|Function       0
  toLocaleUpperCase     stringProtoFuncToLocaleUpperCase DontEnum|Function       0
  localeCompare         stringProtoFuncLocaleCompare     DontEnum|Function       1

  big                   stringProtoFuncBig               DontEnum|Function       0
  small                 stringProtoFuncSmall             DontEnum|Function       0
  blink                 stringProtoFuncBlink             DontEnum|Function       0
  bold                  stringProtoFuncBold              DontEnum|Function       0
  fixed                 stringProtoFuncFixed             DontEnum|Function       0
  italics               stringProtoFuncItalics           DontEnum|Function       0
  strike                stringProtoFuncStrike            DontEnum|Function       0
  sub                   stringProtoFuncSub               DontEnum|Function       0
  sup                   stringProtoFuncSup               DontEnum|Function       0
  fontcolor             stringProtoFuncFontcolor         DontEnum|Function       1
  fontsize              stringProtoFuncFontsize          DontEnum|Function       1
  anchor                stringProtoFuncAnchor            DontEnum|Function       1
  link                  stringProtoFuncLink              DontEnum|Function       1
@end
*/

// ECMA 15.5.4
StringPrototype::StringPrototype(ExecState* exec, ObjectPrototype* objProto)
  : StringObject(exec, objProto)
{
  // The constructor will be added later, after StringConstructor has been built
  putDirect(exec->propertyNames().length, jsNumber(exec, 0), DontDelete | ReadOnly | DontEnum);
}

bool StringPrototype::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot &slot)
{
  return getStaticFunctionSlot<StringObject>(exec, ExecState::stringTable(exec), this, propertyName, slot);
}

// ------------------------------ Functions --------------------------

static inline UString substituteBackreferences(const UString& replacement, const UString& source, const int* ovector, RegExp* reg)
{
  UString substitutedReplacement;
  int offset = 0;
  int i = -1;
  while ((i = replacement.find('$', i + 1)) != -1) {
    if (i + 1 == replacement.size())
        break;

    unsigned short ref = replacement[i + 1];
    if (ref == '$') {
        // "$$" -> "$"
        ++i;
        substitutedReplacement.append(replacement.data() + offset, i - offset);
        offset = i + 1;
        substitutedReplacement.append('$');
        continue;
    }

    int backrefStart;
    int backrefLength;
    int advance = 0;
    if (ref == '&') {
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
        if (replacement.size() > i + 2) {
            ref = replacement[i + 2];
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

    if (i - offset)
        substitutedReplacement.append(replacement.data() + offset, i - offset);
    i += 1 + advance;
    offset = i + 1;
    substitutedReplacement.append(source.data() + backrefStart, backrefLength);
  }

  if (!offset)
    return replacement;

  if (replacement.size() - offset)
    substitutedReplacement.append(replacement.data() + offset, replacement.size() - offset);

  return substitutedReplacement;
}

static inline int localeCompare(const UString& a, const UString& b)
{
    return Collator::userDefault()->collate(reinterpret_cast<const ::UChar*>(a.data()), a.size(), reinterpret_cast<const ::UChar*>(b.data()), b.size());
}

JSValue* stringProtoFuncReplace(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
  JSString* sourceVal = thisValue->toThisJSString(exec);
  const UString& source = sourceVal->value();

  JSValue* pattern = args[0];

  JSValue* replacement = args[1];
  UString replacementString;
  CallData callData;
  CallType callType = replacement->getCallData(callData);
  if (callType == CallTypeNone)
    replacementString = replacement->toString(exec);

  if (pattern->isObject(&RegExpObject::info)) {
    RegExp* reg = static_cast<RegExpObject*>(pattern)->regExp();
    bool global = reg->global();

    RegExpConstructor* regExpObj = exec->lexicalGlobalObject()->regExpConstructor();

    int lastIndex = 0;
    int startPosition = 0;

    Vector<UString::Range, 16> sourceRanges;
    Vector<UString, 16> replacements;

    // This is either a loop (if global is set) or a one-way (if not).
    do {
      int matchIndex;
      int matchLen;
      int* ovector;
      regExpObj->performMatch(reg, source, startPosition, matchIndex, matchLen, &ovector);
      if (matchIndex < 0)
        break;

      sourceRanges.append(UString::Range(lastIndex, matchIndex - lastIndex));

      if (callType != CallTypeNone) {
          int completeMatchStart = ovector[0];
          ArgList args;

          for (unsigned i = 0; i < reg->numSubpatterns() + 1; i++) {
              int matchStart = ovector[i * 2];
              int matchLen = ovector[i * 2 + 1] - matchStart;

              if (matchStart < 0)
                args.append(jsUndefined());
              else
                args.append(jsString(exec, source.substr(matchStart, matchLen)));
          }
          
          args.append(jsNumber(exec, completeMatchStart));
          args.append(sourceVal);

          replacements.append(call(exec, replacement, callType, callData, exec->globalThisValue(), args)->toString(exec));
      } else
          replacements.append(substituteBackreferences(replacementString, source, ovector, reg));

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
      sourceRanges.append(UString::Range(lastIndex, source.size() - lastIndex));

    UString result = source.spliceSubstringsWithSeparators(sourceRanges.data(), sourceRanges.size(), replacements.data(), replacements.size());

    if (result == source)
      return sourceVal;

    return jsString(exec, result);
  }
  
  // First arg is a string
  UString patternString = pattern->toString(exec);
  int matchPos = source.find(patternString);
  int matchLen = patternString.size();
  // Do the replacement
  if (matchPos == -1)
    return sourceVal;
  
  if (callType != CallTypeNone) {
      ArgList args;
      
      args.append(jsString(exec, source.substr(matchPos, matchLen)));
      args.append(jsNumber(exec, matchPos));
      args.append(sourceVal);
      
      replacementString = call(exec, replacement, callType, callData, exec->globalThisValue(), args)->toString(exec);
  }

  return jsString(exec, source.substr(0, matchPos) + replacementString + source.substr(matchPos + matchLen));
}

JSValue* stringProtoFuncToString(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    // Also used for valueOf.

    if (thisValue->isString())
        return thisValue;

    if (thisValue->isObject(&StringObject::info))
        return static_cast<StringObject*>(thisValue)->internalValue();

    return throwError(exec, TypeError);
}

JSValue* stringProtoFuncCharAt(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);
    int len = s.size();

    UString u;
    JSValue* a0 = args[0];
    double dpos = a0->toInteger(exec);
    if (dpos >= 0 && dpos < len)
      u = s.substr(static_cast<int>(dpos), 1);
    else
      u = "";
    return jsString(exec, u);
}

JSValue* stringProtoFuncCharCodeAt(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);
    int len = s.size();

    JSValue* result = 0;

    JSValue* a0 = args[0];
    double dpos = a0->toInteger(exec);
    if (dpos >= 0 && dpos < len)
      result = jsNumber(exec, s[static_cast<int>(dpos)]);
    else
      result = jsNaN(exec);
    return result;
}

JSValue* stringProtoFuncConcat(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);

    ArgList::const_iterator end = args.end();
    for (ArgList::const_iterator it = args.begin(); it != end; ++it) {
        s += (*it)->toString(exec);
    }
    return jsString(exec, s);
}

JSValue* stringProtoFuncIndexOf(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);
    int len = s.size();

    JSValue* a0 = args[0];
    JSValue* a1 = args[1];
    UString u2 = a0->toString(exec);
    double dpos = a1->toInteger(exec);
    if (dpos < 0)
        dpos = 0;
    else if (dpos > len)
        dpos = len;
    return jsNumber(exec, s.find(u2, static_cast<int>(dpos)));
}

JSValue* stringProtoFuncLastIndexOf(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);
    int len = s.size();

    JSValue* a0 = args[0];
    JSValue* a1 = args[1];
    
    UString u2 = a0->toString(exec);
    double dpos = a1->toIntegerPreserveNaN(exec);
    if (dpos < 0)
        dpos = 0;
    else if (!(dpos <= len)) // true for NaN
        dpos = len;
    return jsNumber(exec, s.rfind(u2, static_cast<int>(dpos)));
}

JSValue* stringProtoFuncMatch(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);

    JSValue* a0 = args[0];

    UString u = s;
    RefPtr<RegExp> reg;
    RegExpObject* imp = 0;
    if (a0->isObject() && static_cast<JSObject *>(a0)->inherits(&RegExpObject::info)) {
      reg = static_cast<RegExpObject *>(a0)->regExp();
    } else { 
      /*
       *  ECMA 15.5.4.12 String.prototype.search (regexp)
       *  If regexp is not an object whose [[Class]] property is "RegExp", it is
       *  replaced with the result of the expression new RegExp(regexp).
       */
      reg = RegExp::create(a0->toString(exec));
    }
    RegExpConstructor* regExpObj = exec->lexicalGlobalObject()->regExpConstructor();
    int pos;
    int matchLength;
    regExpObj->performMatch(reg.get(), u, 0, pos, matchLength);
    JSValue* result;
    if (!(reg->global())) {
      // case without 'g' flag is handled like RegExp.prototype.exec
      if (pos < 0)
        result = jsNull();
      else
        result = regExpObj->arrayOfMatches(exec);
    } else {
      // return array of matches
      ArgList list;
      int lastIndex = 0;
      while (pos >= 0) {
        list.append(jsString(exec, u.substr(pos, matchLength)));
        lastIndex = pos;
        pos += matchLength == 0 ? 1 : matchLength;
        regExpObj->performMatch(reg.get(), u, pos, pos, matchLength);
      }
      if (imp)
        imp->setLastIndex(lastIndex);
      if (list.isEmpty()) {
        // if there are no matches at all, it's important to return
        // Null instead of an empty array, because this matches
        // other browsers and because Null is a false value.
        result = jsNull();
      } else {
        result = constructArray(exec, list);
      }
    }
    return result;
}

JSValue* stringProtoFuncSearch(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);

    JSValue* a0 = args[0];

    UString u = s;
    RefPtr<RegExp> reg;
    if (a0->isObject() && static_cast<JSObject*>(a0)->inherits(&RegExpObject::info)) {
      reg = static_cast<RegExpObject*>(a0)->regExp();
    } else { 
      /*
       *  ECMA 15.5.4.12 String.prototype.search (regexp)
       *  If regexp is not an object whose [[Class]] property is "RegExp", it is
       *  replaced with the result of the expression new RegExp(regexp).
       */
      reg = RegExp::create(a0->toString(exec));
    }
    RegExpConstructor* regExpObj = exec->lexicalGlobalObject()->regExpConstructor();
    int pos;
    int matchLength;
    regExpObj->performMatch(reg.get(), u, 0, pos, matchLength);
    return jsNumber(exec, pos);
}

JSValue* stringProtoFuncSlice(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);
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
        return jsString(exec, s.substr(static_cast<int>(from), static_cast<int>(to - from)));
    }

    return jsString(exec, "");
}

JSValue* stringProtoFuncSplit(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);

    JSValue* a0 = args[0];
    JSValue* a1 = args[1];

    JSObject* res = constructEmptyArray(exec);
    JSValue* result = res;
    UString u = s;
    int pos;
    int i = 0;
    int p0 = 0;
    uint32_t limit = a1->isUndefined() ? 0xFFFFFFFFU : a1->toUInt32(exec);
    if (a0->isObject() && static_cast<JSObject *>(a0)->inherits(&RegExpObject::info)) {
      RegExp *reg = static_cast<RegExpObject *>(a0)->regExp();
      if (u.isEmpty() && reg->match(u, 0) >= 0) {
        // empty string matched by regexp -> empty array
        res->put(exec, exec->propertyNames().length, jsNumber(exec, 0));
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
          res->put(exec,i, jsString(exec, u.substr(p0, mpos-p0)));
          p0 = mpos + mlen;
          i++;
        }
        for (unsigned si = 1; si <= reg->numSubpatterns(); ++si) {
          int spos = ovector[si * 2];
          if (spos < 0)
            res->put(exec, i++, jsUndefined());
          else
            res->put(exec, i++, jsString(exec, u.substr(spos, ovector[si * 2 + 1] - spos)));
        }
      }
    } else {
      UString u2 = a0->toString(exec);
      if (u2.isEmpty()) {
        if (u.isEmpty()) {
          // empty separator matches empty string -> empty array
          res->put(exec, exec->propertyNames().length, jsNumber(exec, 0));
          return result;
        } else {
          while (static_cast<uint32_t>(i) != limit && i < u.size()-1)
            res->put(exec, i++, jsString(exec, u.substr(p0++, 1)));
        }
      } else {
        while (static_cast<uint32_t>(i) != limit && (pos = u.find(u2, p0)) >= 0) {
          res->put(exec, i, jsString(exec, u.substr(p0, pos - p0)));
          p0 = pos + u2.size();
          i++;
        }
      }
    }
    // add remaining string, if any
    if (static_cast<uint32_t>(i) != limit)
      res->put(exec, i++, jsString(exec, u.substr(p0)));
    res->put(exec, exec->propertyNames().length, jsNumber(exec, i));
    return result;
}

JSValue* stringProtoFuncSubstr(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);
    int len = s.size();

    JSValue* a0 = args[0];
    JSValue* a1 = args[1];

    double start = a0->toInteger(exec);
    double length = a1->isUndefined() ? len : a1->toInteger(exec);
    if (start >= len)
      return jsString(exec, "");
    if (length < 0)
      return jsString(exec, "");
    if (start < 0) {
      start += len;
      if (start < 0)
        start = 0;
    }
    if (length > len)
      length = len;
    return jsString(exec, s.substr(static_cast<int>(start), static_cast<int>(length)));
}

JSValue* stringProtoFuncSubstring(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);
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
    return jsString(exec, s.substr((int)start, (int)end-(int)start));
}

JSValue* stringProtoFuncToLowerCase(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    JSString* sVal = thisValue->toThisJSString(exec);
    const UString& s = sVal->value();
    
    int ssize = s.size();
    if (!ssize)
        return sVal;
    Vector<UChar> buffer(ssize);
    bool error;
    int length = Unicode::toLower(buffer.data(), ssize, reinterpret_cast<const UChar*>(s.data()), ssize, &error);
    if (error) {
        buffer.resize(length);
        length = Unicode::toLower(buffer.data(), length, reinterpret_cast<const UChar*>(s.data()), ssize, &error);
        if (error)
            return sVal;
    }
    if (length == ssize && memcmp(buffer.data(), s.data(), length * sizeof(UChar)) == 0)
        return sVal;
    return jsString(exec, UString(buffer.releaseBuffer(), length, false));
}

JSValue* stringProtoFuncToUpperCase(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    JSString* sVal = thisValue->toThisJSString(exec);
    const UString& s = sVal->value();
    
    int ssize = s.size();
    if (!ssize)
        return sVal;
    Vector<UChar> buffer(ssize);
    bool error;
    int length = Unicode::toUpper(buffer.data(), ssize, reinterpret_cast<const UChar*>(s.data()), ssize, &error);
    if (error) {
        buffer.resize(length);
        length = Unicode::toUpper(buffer.data(), length, reinterpret_cast<const UChar*>(s.data()), ssize, &error);
        if (error)
            return sVal;
    }
    if (length == ssize && memcmp(buffer.data(), s.data(), length * sizeof(UChar)) == 0)
        return sVal;
    return jsString(exec, UString(buffer.releaseBuffer(), length, false));
}

JSValue* stringProtoFuncToLocaleLowerCase(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    // FIXME: See http://www.unicode.org/Public/UNIDATA/SpecialCasing.txt for locale-sensitive mappings that aren't implemented.

    JSString* sVal = thisValue->toThisJSString(exec);
    const UString& s = sVal->value();
    
    int ssize = s.size();
    if (!ssize)
        return sVal;
    Vector<UChar> buffer(ssize);
    bool error;
    int length = Unicode::toLower(buffer.data(), ssize, reinterpret_cast<const UChar*>(s.data()), ssize, &error);
    if (error) {
        buffer.resize(length);
        length = Unicode::toLower(buffer.data(), length, reinterpret_cast<const UChar*>(s.data()), ssize, &error);
        if (error)
            return sVal;
    }
    if (length == ssize && memcmp(buffer.data(), s.data(), length * sizeof(UChar)) == 0)
        return sVal;
    return jsString(exec, UString(buffer.releaseBuffer(), length, false));
}

JSValue* stringProtoFuncToLocaleUpperCase(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    JSString* sVal = thisValue->toThisJSString(exec);
    const UString& s = sVal->value();
    
    int ssize = s.size();
    if (!ssize)
        return sVal;
    Vector<UChar> buffer(ssize);
    bool error;
    int length = Unicode::toUpper(buffer.data(), ssize, reinterpret_cast<const UChar*>(s.data()), ssize, &error);
    if (error) {
        buffer.resize(length);
        length = Unicode::toUpper(buffer.data(), length, reinterpret_cast<const UChar*>(s.data()), ssize, &error);
        if (error)
            return sVal;
    }
    if (length == ssize && memcmp(buffer.data(), s.data(), length * sizeof(UChar)) == 0)
        return sVal;
    return jsString(exec, UString(buffer.releaseBuffer(), length, false));
}

JSValue* stringProtoFuncLocaleCompare(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    if (args.size() < 1)
      return jsNumber(exec, 0);

    UString s = thisValue->toThisString(exec);
    JSValue* a0 = args[0];
    return jsNumber(exec, localeCompare(s, a0->toString(exec)));
}

JSValue* stringProtoFuncBig(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    UString s = thisValue->toThisString(exec);
    return jsString(exec, "<big>" + s + "</big>");
}

JSValue* stringProtoFuncSmall(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    UString s = thisValue->toThisString(exec);
    return jsString(exec, "<small>" + s + "</small>");
}

JSValue* stringProtoFuncBlink(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    UString s = thisValue->toThisString(exec);
    return jsString(exec, "<blink>" + s + "</blink>");
}

JSValue* stringProtoFuncBold(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    UString s = thisValue->toThisString(exec);
    return jsString(exec, "<b>" + s + "</b>");
}

JSValue* stringProtoFuncFixed(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    UString s = thisValue->toThisString(exec);
    return jsString(exec, "<tt>" + s + "</tt>");
}

JSValue* stringProtoFuncItalics(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    UString s = thisValue->toThisString(exec);
    return jsString(exec, "<i>" + s + "</i>");
}

JSValue* stringProtoFuncStrike(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    UString s = thisValue->toThisString(exec);
    return jsString(exec, "<strike>" + s + "</strike>");
}

JSValue* stringProtoFuncSub(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    UString s = thisValue->toThisString(exec);
    return jsString(exec, "<sub>" + s + "</sub>");
}

JSValue* stringProtoFuncSup(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList&)
{
    UString s = thisValue->toThisString(exec);
    return jsString(exec, "<sup>" + s + "</sup>");
}

JSValue* stringProtoFuncFontcolor(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);
    JSValue* a0 = args[0];
    return jsString(exec, "<font color=\"" + a0->toString(exec) + "\">" + s + "</font>");
}

JSValue* stringProtoFuncFontsize(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);
    JSValue* a0 = args[0];
    return jsString(exec, "<font size=\"" + a0->toString(exec) + "\">" + s + "</font>");
}

JSValue* stringProtoFuncAnchor(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);
    JSValue* a0 = args[0];
    return jsString(exec, "<a name=\"" + a0->toString(exec) + "\">" + s + "</a>");
}

JSValue* stringProtoFuncLink(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    UString s = thisValue->toThisString(exec);
    JSValue* a0 = args[0];
    return jsString(exec, "<a href=\"" + a0->toString(exec) + "\">" + s + "</a>");
}

} // namespace KJS
