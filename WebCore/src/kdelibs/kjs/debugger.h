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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  $Id$
 */

#ifndef _KJSDEBUGGER_H_
#define _KJSDEBUGGER_H_

namespace KJS {

  class DebuggerImp;
  class Interpreter;
  class ExecState;
  class Object;
  class UString;
  class List;

  /**
   * @internal
   *
   * Provides an interface which receives notification about various
   * script-execution related events such as statement execution and function
   * calls.
   *
   * WARNING: This interface is still a work in progress and is not yet
   * offically publicly available. It is likely to change in binary incompatible
   * (and possibly source incompatible) ways in future versions. It is
   * anticipated that at some stage the interface will be frozen and made
   * available for general use.
   */
  class Debugger {
  public:

    /**
     * Creates a new debugger
     */
    Debugger();

    /**
     * Destroys the debugger. If the debugger is attached to any interpreters,
     * it is automatically detached.
     */
    virtual ~Debugger();

    DebuggerImp *imp() const { return rep; }

    /**
     * Attaches the debugger to specified interpreter. This will cause this
     * object to receive notification of events from the interpreter.
     *
     * If the interpreter is deleted, the debugger will automatically be
     * detached.
     *
     * Note: only one debugger can be attached to an interpreter at a time.
     * Attaching another debugger to the same interpreter will cause the
     * original debugger to be detached from that interpreter.
     *
     * @param interp The interpreter to attach to
     *
     * @see detach()
     */
    void attach(Interpreter *interp);

    /**
     * Detach the debugger from an interpreter
     *
     * @param interp The interpreter to detach from. If 0, the debugger will be
     * detached from all interpreters to which it is attached.
     *
     * @see attach()
     */
    void detach(Interpreter *interp);

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
     * sourceId supplied in other functions such as @ref atStatement()
     * @param source The source code that was parsed
     * @param errorLine The line number at which parsing encountered an
     * error, or -1 if the source code was valid and parsed succesfully
     * @return true if execution should be continue, false if it should
     * be aborted
     */
    virtual bool sourceParsed(ExecState *exec, int sourceId,
			      const UString &source, int errorLine);

    /**
     * Called when all functions/programs associated with a particular
     * sourceId have been deleted. After this function has been called for
     * a particular sourceId, that sourceId will not be used again.
     *
     * The default implementation does nothing. Override this method if
     * you want to process this event.
     *
     * @param exec The current execution state
     * @param sourceId The ID of the source code (corresponds to the
     * sourceId supplied in other functions such as atLine()
     * @return true if execution should be continue, false if it should
     * be aborted
     */
    virtual bool sourceUnused(ExecState *exec, int sourceId);

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
     * @return true if execution should be continue, false if it should
     * be aborted
     */
    virtual bool exception(ExecState *exec, int sourceId, int lineno,
                           Object &exceptionObj);

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
     * @param firstLine The ending line of the statement  that is about to be
     * executed (usually the same as firstLine)
     * @return true if execution should be continue, false if it should
     * be aborted
     */
    virtual bool atStatement(ExecState *exec, int sourceId, int firstLine,
                             int lastLine);
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
     * @param function The function being called
     * @param args The arguments that were passed to the function
     * line is being executed
     * @return true if execution should be continue, false if it should
     * be aborted
     */
    virtual bool callEvent(ExecState *exec, int sourceId, int lineno,
			   Object &function, const List &args);

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
     * @param function The function being called
     * @return true if execution should be continue, false if it should
     * be aborted
     */
    virtual bool returnEvent(ExecState *exec, int sourceId, int lineno,
                             Object &function);

  private:
    DebuggerImp *rep;
  };

};

#endif
