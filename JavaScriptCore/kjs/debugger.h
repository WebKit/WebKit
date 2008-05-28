// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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

#ifndef Debugger_h
#define Debugger_h

#include <wtf/HashSet.h>
#include "protect.h"

namespace KJS {

  class DebuggerCallFrame;
  class ExecState;
  class JSGlobalObject;
  class JSObject;
  class JSValue;
  class List;
  class SourceProvider;
  class UString;
  
  /**
   * @internal
   *
   * Provides an interface which receives notification about various
   * script-execution related events such as statement execution and function
   * calls.
   */
  class Debugger {
  public:

    /**
     * Creates a new debugger
     */
    Debugger();

    /**
     * Destroys the debugger. If the debugger is attached to any global objects,
     * it is automatically detached.
     */
    virtual ~Debugger();

    /**
     * Attaches the debugger to specified global object. This will cause this
     * object to receive notification of events during execution.
     *
     * If the global object is deleted, it will detach the debugger.
     *
     * Note: only one debugger can be attached to a global object at a time.
     * Attaching another debugger to the same global object will cause the
     * original debugger to be detached.
     *
     * @param The global object to attach to.
     */
    void attach(JSGlobalObject*);

    /**
     * Detach the debugger from a global object.
     *
     * @param The global object to detach from.
     */
    void detach(JSGlobalObject*);

    /**
     * Called to notify the debugger that some javascript source code has
     * been parsed. For calls to Interpreter::evaluate(), this will be called
     * with the supplied source code before any other code is parsed.
     * Other situations in which this may be called include creation of a
     * function using the Function() constructor, or the eval() function.
     *
     * The default implementation does nothing. Override this method if
     * you want to process this event.
     *
     * @param exec The current execution state
     * @param sourceId The ID of the source code (corresponds to the
     * sourceId supplied in other functions such as atStatement()
     * @param sourceURL Where the source code that was parsed came from
     * @param source The source code that was parsed
     * @param startingLineNumber The line number at which parsing started
     * @param errorLine The line number at which parsing encountered an
     * error, or -1 if the source code was valid and parsed successfully
     * @param errorMsg The error description, or null if the source code
       was valid and parsed successfully
     */
    virtual void sourceParsed(ExecState*, int sourceId, const UString& sourceURL,
                              const SourceProvider& source, int startingLineNumber, int errorLine, const UString& errorMsg) = 0;

    /**
     * Called when an exception is thrown during script execution.
     *
     * The default implementation does nothing. Override this method if
     * you want to process this event.
     *
     * @param exec The current execution state
     * @param sourceId The ID of the source code being executed
     * @param lineno The line at which the error occurred
     * @param exceptionObj The exception object
     */
    virtual void exception(const DebuggerCallFrame&, int sourceId, int lineno) = 0;

    /**
     * Called when a line of the script is reached (before it is executed)
     *
     * The default implementation does nothing. Override this method if
     * you want to process this event.
     *
     * @param exec The current execution state
     * @param sourceId The ID of the source code being executed
     * @param firstLine The starting line of the statement  that is about to be
     * executed
     * @param lastLine The ending line of the statement  that is about to be
     * executed (usually the same as firstLine)
     */
    virtual void atStatement(const DebuggerCallFrame&, int sourceId, int lineno) = 0;
    /**
     * Called on each function call. Use together with @ref #returnEvent
     * if you want to keep track of the call stack.
     *
     * Note: This only gets called for functions that are declared in ECMAScript
     * source code or passed to eval(), not for internal KJS or
     * application-supplied functions.
     *
     * The default implementation does nothing. Override this method if
     * you want to process this event.
     *
     * @param exec The current execution state
     * @param sourceId The ID of the source code being executed
     * @param lineno The line that is about to be executed
     */
    virtual void callEvent(const DebuggerCallFrame&, int sourceId, int lineno) = 0;

    /**
     * Called on each function exit. The function being returned from is that
     * which was supplied in the last callEvent().
     *
     * Note: This only gets called for functions that are declared in ECMAScript
     * source code or passed to eval(), not for internal KJS or
     * application-supplied functions.
     *
     * The default implementation does nothing. Override this method if
     * you want to process this event.
     *
     * @param exec The current execution state
     * @param sourceId The ID of the source code being executed
     * @param lineno The line that is about to be executed
     */
    virtual void returnEvent(const DebuggerCallFrame&, int sourceId, int lineno) = 0;

    virtual void willExecuteProgram(const DebuggerCallFrame&, int sourceId, int lineno) = 0;
    virtual void didExecuteProgram(const DebuggerCallFrame&, int sourceId, int lineno) = 0;

  private:
    HashSet<JSGlobalObject*> m_globalObjects;
  };

} // namespace KJS

#endif
