/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
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

#ifndef _KJSDEBUGGER_H_
#define _KJSDEBUGGER_H_

#include "internal.h"

namespace KJS {

  //
  // NOTE: this interface is not ready, yet. Do not use unless you
  // don't mind source and binary incompatible changes that may arise
  // before the final version is released.
  //

#ifdef KJS_DEBUGGER
  class Debugger {
    friend class KJScriptImp;
    friend class StatementNode;
    friend class DeclaredFunctionImp;
    friend class FunctionImp;
  public:
    /**
     * Available modes of the debugger.
     */
    enum Mode { Disabled = 0, Next, Step, Continue, Stop };
    /**
     * Construct a debugger and attach it to the scripting engine s.
     */
    Debugger(KJScript *s);
    /**
     * Destruct the debugger and detach from the scripting engine we
     * might have been attached to.
     */
    virtual ~Debugger();
    /**
     * Attaches the debugger to specified scripting engine.
     */
    void attach(KJScript *e);
    /**
     * Returns the engine the interpreter is currently attached to. Null
     * if there isn't any.
     */
    KJScript* engine() const;
    /**
     * Detach the debugger from any scripting engine.
     */
    void detach();
    /**
     * Set debugger into specified mode. This will influence further behaviour
     * if execution of the programm is started or continued.
     */
    virtual void setMode(Mode m);
    /**
     * Returns the current operation mode.
     */
    Mode mode() const;
    /**
     * Returns the line number the debugger currently has stopped at.
     * -1 if the debugger is not in a break status.
     */
    int lineNumber() const { return l; }
    /**
     * Returns the source id the debugger currently has stopped at.
     * -1 if the debugger is not in a break status.
     */
    int sourceId() const { return sid; }
    /**
     * Sets a breakpoint in the first statement where line lies in between
     * the statements range. Returns true if sucessfull, false if no
     * matching statement could be found.
     */
    bool setBreakpoint(int id, int line);
    bool deleteBreakpoint(int id, int line);
    void clearAllBreakpoints(int id=-1);
    /**
     * Returns the value of ident out of the current context in string form
     */
    UString varInfo(const UString &ident);
    /**
     * Set variable ident to value. Returns true if successful, false if
     * the specified variable doesn't exist or isn't writable.
     */
    bool setVar(const UString &ident, const KJSO &value);

  protected:
    /**
     * Invoked in case a breakpoint or the next statement is reached in step
     * mode. The return value decides whether execution will continue. True
     * denotes continuation, false an abortion, respectively.
     *
     * The default implementation does nothing. Overload this method if
     * you want to process this event.
     */
    virtual bool stopEvent();
    /**
     * Returns an integer that will be assigned to the code passed
     * next to one of the KJScript::evaluate() methods. It's basically
     * a counter to will only be reset to 0 on KJScript::clear().
     *
     * This information is useful in case you evaluate multiple blocks of
     * code containing some function declarations. Keep a map of source id/
     * code pairs, query sourceId() in case of a stopEvent() and update
     * your debugger window with the matching source code.
     */
    int freeSourceId() const;
    /**
     * Invoked on each function call. Use together with @ref returnEvent
     * if you want to keep track of the call stack.
     */
    virtual void callEvent(const UString &fn = UString::null,
				    const UString &s = UString::null);
    /**
     * Invoked on each function exit.
     */
    virtual void returnEvent();

  private:
    void reset();
    bool hit(int line, bool breakPoint);
    void setSourceId(int i) { sid = i; }
    UString objInfo(const KJSO &obj) const;

    KJScript *eng;
    Mode dmode;
    int l;
    int sid;
  };
#endif

};

#endif
