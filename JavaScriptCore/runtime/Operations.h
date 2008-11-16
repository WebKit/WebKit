/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef _KJS_OPERATIONS_H_
#define _KJS_OPERATIONS_H_

#include "JSImmediate.h"
#include "JSNumberCell.h"
#include "JSString.h"

namespace JSC {

  // ECMA 11.9.3
  bool equal(ExecState*, JSValue*, JSValue*);
  bool equalSlowCase(ExecState*, JSValue*, JSValue*);

  ALWAYS_INLINE bool equalSlowCaseInline(ExecState* exec, JSValue* v1, JSValue* v2)
  {
      ASSERT(!JSImmediate::areBothImmediateNumbers(v1, v2));

      do {
          if (v1->isNumber() && v2->isNumber())
              return v1->uncheckedGetNumber() == v2->uncheckedGetNumber();

          bool s1 = v1->isString();
          bool s2 = v2->isString();
          if (s1 && s2)
              return asString(v1)->value() == asString(v2)->value();

          if (v1->isUndefinedOrNull()) {
              if (v2->isUndefinedOrNull())
                  return true;
              if (JSImmediate::isImmediate(v2))
                  return false;
              return v2->asCell()->structure()->typeInfo().masqueradesAsUndefined();
          }

          if (v2->isUndefinedOrNull()) {
              if (JSImmediate::isImmediate(v1))
                  return false;
              return v1->asCell()->structure()->typeInfo().masqueradesAsUndefined();
          }

          if (v1->isObject()) {
              if (v2->isObject())
                  return v1 == v2;
              JSValue* p1 = v1->toPrimitive(exec);
              if (exec->hadException())
                  return false;
              v1 = p1;
              if (JSImmediate::areBothImmediateNumbers(v1, v2))
                  return v1 == v2;
              continue;
          }

          if (v2->isObject()) {
              JSValue* p2 = v2->toPrimitive(exec);
              if (exec->hadException())
                  return false;
              v2 = p2;
              if (JSImmediate::areBothImmediateNumbers(v1, v2))
                  return v1 == v2;
              continue;
          }

          if (s1 || s2) {
              double d1 = v1->toNumber(exec);
              double d2 = v2->toNumber(exec);
              return d1 == d2;
          }

          if (v1->isBoolean()) {
              if (v2->isNumber())
                  return static_cast<double>(v1->getBoolean()) == v2->uncheckedGetNumber();
          } else if (v2->isBoolean()) {
              if (v1->isNumber())
                  return v1->uncheckedGetNumber() == static_cast<double>(v2->getBoolean());
          }

          return v1 == v2;
      } while (true);
  }


  bool strictEqual(JSValue*, JSValue*);
  bool strictEqualSlowCase(JSValue*, JSValue*);

  inline bool strictEqualSlowCaseInline(JSValue* v1, JSValue* v2)
  {
      ASSERT(!JSImmediate::areBothImmediate(v1, v2));
      
      if (JSImmediate::isEitherImmediate(v1, v2)) {
          ASSERT(v1 == JSImmediate::zeroImmediate() || v2 == JSImmediate::zeroImmediate());
          ASSERT(v1 != v2);

          // The reason we can't just return false here is that 0 === -0,
          // and while the former is an immediate number, the latter is not.
          if (v1 == JSImmediate::zeroImmediate())
              return asCell(v2)->isNumber() && asNumberCell(v2)->value() == 0;
          return asCell(v1)->isNumber() && asNumberCell(v1)->value() == 0;
      }
      
      if (asCell(v1)->isNumber()) {
          return asCell(v2)->isNumber()
              && asNumberCell(v1)->value() == asNumberCell(v2)->value();
      }

      if (asCell(v1)->isString()) {
          return asCell(v2)->isString()
              && asString(v1)->value() == asString(v2)->value();
      }

      return v1 == v2;
  }

  JSValue* throwOutOfMemoryError(ExecState*);
}

#endif
