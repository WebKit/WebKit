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

  class ExecState;
  class JSValue;

  bool equal(ExecState*, JSValue*, JSValue*);
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
              return static_cast<JSCell*>(v2)->isNumber() && static_cast<JSNumberCell*>(v2)->value() == 0;
          return static_cast<JSCell*>(v1)->isNumber() && static_cast<JSNumberCell*>(v1)->value() == 0;
      }
      
      if (static_cast<JSCell*>(v1)->isNumber()) {
          return static_cast<JSCell*>(v2)->isNumber()
              && static_cast<JSNumberCell*>(v1)->value() == static_cast<JSNumberCell*>(v2)->value();
      }

      if (static_cast<JSCell*>(v1)->isString()) {
          return static_cast<JSCell*>(v2)->isString()
              && static_cast<JSString*>(v1)->value() == static_cast<JSString*>(v2)->value();
      }

      return v1 == v2;
  }

  JSValue* throwOutOfMemoryError(ExecState*);
}

#endif
