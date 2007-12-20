// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007 Apple Inc. All rights reserved.
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

#ifndef KJS_COMPLETION_H
#define KJS_COMPLETION_H

namespace KJS {

  class JSValue;

  enum ComplType { Normal, Break, Continue, ReturnValue, Throw, Interrupted };

  /**
   * Completion objects are used to convey the return status and value
   * from functions.
   *
   * See FunctionImp::execute()
   *
   * @see FunctionImp
   *
   * @short Handle for a Completion type.
   */
  class Completion {
  public:
    Completion(ComplType type = Normal, JSValue* value = 0)
        : m_type(type), m_value(value) { }

    ComplType complType() const { return m_type; }
    JSValue* value() const { return m_value; }
    void setValue(JSValue* v) { m_value = v; }
    bool isValueCompletion() const { return !!m_value; }

  private:
    ComplType m_type;
    JSValue* m_value;
  };

}

#endif 
