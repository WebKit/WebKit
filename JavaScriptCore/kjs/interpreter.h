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

#ifndef _KJS_INTERPRETER_H_
#define _KJS_INTERPRETER_H_

#include "ExecState.h"
#include "protect.h"
#include "value.h"
#include "types.h"

namespace KJS {

  class ArrayObjectImp;
  class ArrayPrototype;
  class BooleanObjectImp;
  class BooleanPrototype;
  class Context;
  class DateObjectImp;
  class DatePrototype;
  class Debugger;
  class ErrorObjectImp;
  class ErrorPrototype;
  class EvalError;
  class EvalErrorPrototype;
  class FunctionObjectImp;
  class FunctionPrototype;
  class NativeErrorImp;
  class NativeErrorPrototype;
  class NumberObjectImp;
  class NumberPrototype;
  class ObjectObjectImp;
  class ObjectPrototype;
  class RangeError;
  class RangeErrorPrototype;
  class ReferenceError;
  class ReferenceError;
  class ReferenceErrorPrototype;
  class RegExpObjectImp;
  class RegExpPrototype;
  class RuntimeMethod;
  class SavedBuiltins;
  class ScopeChain;
  class StringObjectImp;
  class StringPrototype;
  class SyntaxErrorPrototype;
  class TypeError;
  class TypeErrorPrototype;
  class UriError;
  class UriErrorPrototype;
  
  /**
   * Interpreter objects can be used to evaluate ECMAScript code. Each
   * interpreter has a global object which is used for the purposes of code
   * evaluation, and also provides access to built-in properties such as
   * " Object" and "Number".
   */
  class Interpreter {
      friend class Collector;
  public:
    /**
     * Creates a new interpreter. The supplied object will be used as the global
     * object for all scripts executed with this interpreter. During
     * constuction, all the standard properties such as "Object" and "Number"
     * will be added to the global object.
     *
     * Note: You should not use the same global object for multiple
     * interpreters.
     *
     * This is due do the fact that the built-in properties are set in the
     * constructor, and if these objects have been modified from another
     * interpreter (e.g. a script modifying String.prototype), the changes will
     * be overridden.
     *
     * @param global The object to use as the global object for this interpreter
     */
    Interpreter(JSObject* globalObject);
    /**
     * Creates a new interpreter. A global object will be created and
     * initialized with the standard global properties.
     */
    Interpreter();

    /**
     * Resets the global object's default properties and adds the default object 
     * prototype to its prototype chain.
     */
    void initGlobalObject();

    /**
     * Returns the object that is used as the global object during all script
     * execution performed by this interpreter
     */
    JSObject* globalObject() const;

    /**
     * Returns the execution state object which can be used to execute
     * scripts using this interpreter at a the "global" level, i.e. one
     * with a execution context that has the global object as the "this"
     * value, and who's scope chain contains only the global object.
     *
     * Note: this pointer remains constant for the life of the interpreter
     * and should not be manually deleted.
     *
     * @return The interpreter global execution state object
     */
    virtual ExecState *globalExec();

    /**
     * Parses the supplied ECMAScript code and checks for syntax errors.
     *
     * @param code The code to check
     * @return A normal completion if there were no syntax errors in the code, 
     * otherwise a throw completion with the syntax error as its value.
     */
    Completion checkSyntax(const UString& sourceURL, int startingLineNumber, const UString& code);
    Completion checkSyntax(const UString& sourceURL, int startingLineNumber, const UChar* code, int codeLength);

    /**
     * Evaluates the supplied ECMAScript code.
     *
     * Since this method returns a Completion, you should check the type of
     * completion to detect an error or before attempting to access the returned
     * value. For example, if an error occurs during script execution and is not
     * caught by the script, the completion type will be Throw.
     *
     * If the supplied code is invalid, a SyntaxError will be thrown.
     *
     * @param code The code to evaluate
     * @param thisV The value to pass in as the "this" value for the script
     * execution. This should either be jsNull() or an Object.
     * @return A completion object representing the result of the execution.
     */
    Completion evaluate(const UString& sourceURL, int startingLineNumber, const UChar* code, int codeLength, JSValue* thisV = 0);
    Completion evaluate(const UString& sourceURL, int startingLineNumber, const UString& code, JSValue* thisV = 0);

    /**
     * Returns the builtin "Object" object. This is the object that was set
     * as a property of the global object during construction; if the property
     * is replaced by script code, this method will still return the original
     * object.
     *
     * @return The builtin "Object" object
     */
    JSObject *builtinObject() const;

    /**
     * Returns the builtin "Function" object.
     */
    JSObject *builtinFunction() const;

    /**
     * Returns the builtin "Array" object.
     */
    JSObject *builtinArray() const;

    /**
     * Returns the builtin "Boolean" object.
     */
    JSObject *builtinBoolean() const;

    /**
     * Returns the builtin "String" object.
     */
    JSObject *builtinString() const;

    /**
     * Returns the builtin "Number" object.
     */
    JSObject *builtinNumber() const;

    /**
     * Returns the builtin "Date" object.
     */
    JSObject *builtinDate() const;

    /**
     * Returns the builtin "RegExp" object.
     */
    JSObject *builtinRegExp() const;

    /**
     * Returns the builtin "Error" object.
     */
    JSObject *builtinError() const;

    /**
     * Returns the builtin "Object.prototype" object.
     */
    JSObject *builtinObjectPrototype() const;

    /**
     * Returns the builtin "Function.prototype" object.
     */
    JSObject *builtinFunctionPrototype() const;

    /**
     * Returns the builtin "Array.prototype" object.
     */
    JSObject *builtinArrayPrototype() const;

    /**
     * Returns the builtin "Boolean.prototype" object.
     */
    JSObject *builtinBooleanPrototype() const;

    /**
     * Returns the builtin "String.prototype" object.
     */
    JSObject *builtinStringPrototype() const;

    /**
     * Returns the builtin "Number.prototype" object.
     */
    JSObject *builtinNumberPrototype() const;

    /**
     * Returns the builtin "Date.prototype" object.
     */
    JSObject *builtinDatePrototype() const;

    /**
     * Returns the builtin "RegExp.prototype" object.
     */
    JSObject *builtinRegExpPrototype() const;

    /**
     * Returns the builtin "Error.prototype" object.
     */
    JSObject *builtinErrorPrototype() const;

    /**
     * The initial value of "Error" global property
     */
    JSObject *builtinEvalError() const;
    JSObject *builtinRangeError() const;
    JSObject *builtinReferenceError() const;
    JSObject *builtinSyntaxError() const;
    JSObject *builtinTypeError() const;
    JSObject *builtinURIError() const;

    JSObject *builtinEvalErrorPrototype() const;
    JSObject *builtinRangeErrorPrototype() const;
    JSObject *builtinReferenceErrorPrototype() const;
    JSObject *builtinSyntaxErrorPrototype() const;
    JSObject *builtinTypeErrorPrototype() const;
    JSObject *builtinURIErrorPrototype() const;

    enum CompatMode { NativeMode, IECompat, NetscapeCompat };
    /**
     * Call this to enable a compatibility mode with another browser.
     * (by default konqueror is in "native mode").
     * Currently, in KJS, this only changes the behavior of Date::getYear()
     * which returns the full year under IE.
     */
    void setCompatMode(CompatMode mode) { m_compatMode = mode; }
    CompatMode compatMode() const { return m_compatMode; }
    
    /**
     * Run the garbage collection. Returns true when at least one object
     * was collected; false otherwise.
     */
    static bool collect();

    /**
     * Called during the mark phase of the garbage collector. Subclasses 
     * implementing custom mark methods must make sure to chain to this one.
     */
    virtual void mark();

#ifdef KJS_DEBUG_MEM
    /**
     * @internal
     */
    static void finalCheck();
#endif

    static bool shouldPrintExceptions();
    static void setShouldPrintExceptions(bool);

    void saveBuiltins (SavedBuiltins&) const;
    void restoreBuiltins (const SavedBuiltins&);

    /**
     * Determine if the value is a global object (for any interpreter).  This may
     * be difficult to determine for multiple uses of JSC in a process that are
     * logically independent of each other.  In the case of WebCore, this method
     * is used to determine if an object is the Window object so we can perform
     * security checks.
     */
    virtual bool isGlobalObject(JSValue*) { return false; }
    
    /** 
     * Find the interpreter for a particular global object.  This should really
     * be a static method, but we can't do that is C++.  Again, as with isGlobalObject()
     * implementation really need to know about all instances of Interpreter
     * created in an application to correctly implement this method.  The only
     * override of this method is currently in WebCore.
     */
    virtual Interpreter* interpreterForGlobalObject(const JSValue*) { return 0; }
    
    /**
     * Determine if the it is 'safe' to execute code in the target interpreter from an
     * object that originated in this interpreter.  This check is used to enforce WebCore
     * cross frame security rules.  In particular, attempts to access 'bound' objects are
     * not allowed unless isSafeScript returns true.
     */
    virtual bool isSafeScript(const Interpreter*) { return true; }
  
    // Chained list of interpreters (ring)
    static Interpreter* firstInterpreter() { return s_hook; }
    Interpreter* nextInterpreter() const { return next; }
    Interpreter* prevInterpreter() const { return prev; }

    Debugger* debugger() const { return m_debugger; }
    void setDebugger(Debugger* d) { m_debugger = d; }
    
    void setContext(Context* c) { m_context = c; }
    Context* context() const { return m_context; }
    
    static Interpreter* interpreterWithGlobalObject(JSObject*);
    
    void setTimeoutTime(unsigned timeoutTime) { m_timeoutTime = timeoutTime; }

    void startTimeoutCheck();
    void stopTimeoutCheck();
    
    bool timedOut();
    
    void ref() { ++m_refCount; }
    void deref() { if (--m_refCount <= 0) delete this; }
    int refCount() const { return m_refCount; }
    
protected:
    virtual ~Interpreter(); // only deref should delete us
    virtual bool shouldInterruptScript() const { return true; }

    unsigned m_timeoutTime;

private:
    void init();

    void resetTimeoutCheck();
    bool checkTimeout();

    // Uncopyable
    Interpreter(const Interpreter&);
    Interpreter operator=(const Interpreter&);
    
    int m_refCount;
    
    ExecState m_globalExec;

    // Chained list of interpreters (ring) - for collector
    static Interpreter* s_hook;
    Interpreter *next, *prev;
    
    int m_recursion;
    
    Debugger* m_debugger;
    Context* m_context;
    CompatMode m_compatMode;

    unsigned m_timeAtLastCheckTimeout;
    unsigned m_timeExecuting;
    unsigned m_timeoutCheckCount;
    
    unsigned m_tickCount;
    unsigned m_ticksUntilNextTimeoutCheck;

    JSObject* m_globalObject;

    ObjectObjectImp* m_Object;
    FunctionObjectImp* m_Function;
    ArrayObjectImp* m_Array;
    BooleanObjectImp* m_Boolean;
    StringObjectImp* m_String;
    NumberObjectImp* m_Number;
    DateObjectImp* m_Date;
    RegExpObjectImp* m_RegExp;
    ErrorObjectImp* m_Error;
    
    ObjectPrototype* m_ObjectPrototype;
    FunctionPrototype* m_FunctionPrototype;
    ArrayPrototype* m_ArrayPrototype;
    BooleanPrototype* m_BooleanPrototype;
    StringPrototype* m_StringPrototype;
    NumberPrototype* m_NumberPrototype;
    DatePrototype* m_DatePrototype;
    RegExpPrototype* m_RegExpPrototype;
    ErrorPrototype* m_ErrorPrototype;
    
    NativeErrorImp* m_EvalError;
    NativeErrorImp* m_RangeError;
    NativeErrorImp* m_ReferenceError;
    NativeErrorImp* m_SyntaxError;
    NativeErrorImp* m_TypeError;
    NativeErrorImp* m_UriError;
    
    NativeErrorPrototype* m_EvalErrorPrototype;
    NativeErrorPrototype* m_RangeErrorPrototype;
    NativeErrorPrototype* m_ReferenceErrorPrototype;
    NativeErrorPrototype* m_SyntaxErrorPrototype;
    NativeErrorPrototype* m_TypeErrorPrototype;
    NativeErrorPrototype* m_UriErrorPrototype;
  };

  inline bool Interpreter::timedOut()
  {
      m_tickCount++;
      
      if (m_tickCount != m_ticksUntilNextTimeoutCheck)
          return false;
      
      return checkTimeout();
  }
  
} // namespace

#endif // _KJS_INTERPRETER_H_
