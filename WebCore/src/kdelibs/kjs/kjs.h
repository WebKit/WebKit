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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */

#ifndef _KJS_H_
#define _KJS_H_

class QChar;

namespace KJS {
  class UString;
  class Lexer;
  class Context;
  class Global;
  class KJSO;
  class Object;
  class Imp;
  class List;
  class KJScriptImp;
#ifdef KJS_DEBUGGER
  class Debugger;
#endif
};

/**
 * @libdoc ECMAScript interpreter
 *
 * This library provides an interpreter for ECMAScript aka JavaScript.
 * It currently aims at compliance with Edition 3 of the standard.
 */

/**
 * @short ECMAScript interpreter
 */
class KJScript {
  friend class KJS::KJScriptImp;
  friend class KJS::KJSO;
  friend class KJS::Context;
  friend class KJS::Lexer;
  friend class KJS::Global;
public:
  /**
   * Create a new ECMAScript interpreter. You can later ask it to interprete
   * code by passing it via @ref #evaluate.
   */
  KJScript();
  /**
   *  Destructor
   */
  ~KJScript();
  /**
   * Force a "context switch". You usually do not need to do that,
   * evaluate() does it too.
   */
  void init();
  /**
   * Returns a pointer to the Global object.
   */
  KJS::Imp *globalObject() const;
  /**
   * Don't use. May disappear.
   */
  static KJScript *current();
  /**
   * Don't use. May disappear.
   */
  static void setCurrent( KJScript *newCurr );

  /**
   * Current level of recursive calls to this interpreter. 0 initially.
   */
  int recursion() const;
  /**
   * Asks the interpreter to evaluate a piece of code. If called more than
   * once the state (global variables, functions etc.) will be preserved
   * between each call.
   * @param code is a string containing the code to be executed.
   * @return True if the code was evaluated successfully, false if an error
   * occured.
   */
  bool evaluate(const char *code);
  /**
   * Same as above. Only differing in the arguments accepted.
   * @param code is a pointer to an Unicode string containing the code to
   * be executed.
   * @param length number of characters.
   */
  bool evaluate(const KJS::KJSO &thisV,
		const QChar *code, unsigned int length);
  /**
   * Added for convenience in case you have the code in available in
   * internal representation already.
   * @param code is an Unicode string containing the code to be executed.
   */
  bool evaluate(const KJS::UString &code);
  /**
   * Call the specified function directly, optionally passing args as a
   * list of arguments. Return value and treatment of errors is analog
   * to the evaluate() calls.
   */
  bool call(const KJS::UString &func, const KJS::List &args);
  bool call(const KJS::KJSO &scope, const KJS::UString &func,
	    const KJS::List &args);
  bool call(const KJS::KJSO &func, const KJS::KJSO &thisV,
	    const KJS::List &args, const KJS::List &extraScope );
  /**
   * Clear the interpreter's memory. Otherwise, function declarations
   * and global variables will be remembered after each invokation of
   * @ref KJScript::evaluate.
   */
  void clear();
  /**
   * @return Return value from the last call to @ref evaluate(). Null if there
   * hasn't been any.
   */
  KJS::Imp *returnValue() const;
  /**
   * @return Return code from last call to @ref evaluate(). 0 on success.
   */
  int errorType() const;
  /**
   * @return Return line of last error. -1 if last call didn't have an error.
   */
  int errorLine() const;
  /**
   * @return Error message from last call to @ref evaluate(). Empty string
   * if no error occured.
   */
  const char *errorMsg() const;
  /**
   * Check the syntax of a piece of code. Return true if the code could be
   * parsed without errors, false otherwise. @ref errorLine() will tell you
   * approximately where the syntax error happened.
   */
  bool checkSyntax(const KJS::UString &code);
  /**
   * Adds a debug() function to the set of pre-defined properties.
   * debug(arg) tries to convert 'arg' to a string and prints the result
   * to stderr. If you want to debug self defined Host Objects this way
   * you should provide them with a toString() method that returns a string.
   */
  void enableDebug();
private:
  KJS::KJScriptImp *rep;
  // not implemented
  KJScript(const KJScript&);
  KJScript operator=(const KJScript&);

#ifdef KJS_DEBUGGER
  friend class KJS::Debugger;
#endif
};

#endif
