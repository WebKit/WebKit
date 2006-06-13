/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003 Apple Computer, Inc.
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

#ifndef ExecState_H
#define ExecState_H

#include "value.h"
#include "types.h"

namespace KJS {
    class Context;
    
  /**
   * Represents the current state of script execution. This object allows you
   * obtain a handle the interpreter that is currently executing the script,
   * and also the current execution context.
   */
  class ExecState {
    friend class Interpreter;
    friend class FunctionImp;
    friend class RuntimeMethodImp;
    friend class GlobalFuncImp;
  public:
    /**
     * Returns the interpreter associated with this execution state
     *
     * @return The interpreter executing the script
     */
    Interpreter* dynamicInterpreter() const { return m_interpreter; }

    /**
     * Returns the interpreter associated with the current scope's
     * global object
     *
     * @return The interpreter currently in scope
     */
    Interpreter* lexicalInterpreter() const;

    /**
     * Returns the execution context associated with this execution state
     *
     * @return The current execution state context
     */
    Context* context() const { return m_context; }

    void setException(JSValue* e) { m_exception = e; }
    void clearException() { m_exception = 0; }
    JSValue* exception() const { return m_exception; }
    bool hadException() const { return m_exception; }

  private:
    ExecState(Interpreter* interp, Context* con)
        : m_interpreter(interp)
        , m_context(con)
        , m_exception(0)
    { 
    }
    Interpreter* m_interpreter;
    Context* m_context;
    JSValue* m_exception;
  };

} // namespace KJS

#endif // ExecState_H
