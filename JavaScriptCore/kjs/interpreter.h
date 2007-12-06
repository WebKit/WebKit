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

#ifndef KJS_Interpreter_h
#define KJS_Interpreter_h

namespace KJS {

  class Completion;
  class ExecState;
  class JSValue;
  class UString;

  struct UChar;
  
  class Interpreter {
  public:
    /**
     * Parses the supplied ECMAScript code and checks for syntax errors.
     *
     * @param code The code to check
     * @return A normal completion if there were no syntax errors in the code, 
     * otherwise a throw completion with the syntax error as its value.
     */
    static Completion checkSyntax(ExecState*, const UString& sourceURL, int startingLineNumber, const UString& code);
    static Completion checkSyntax(ExecState*, const UString& sourceURL, int startingLineNumber, const UChar* code, int codeLength);

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
    static Completion evaluate(ExecState*, const UString& sourceURL, int startingLineNumber, const UString& code, JSValue* thisV = 0);
    static Completion evaluate(ExecState*, const UString& sourceURL, int startingLineNumber, const UChar* code, int codeLength, JSValue* thisV = 0);
    
    static bool shouldPrintExceptions();
    static void setShouldPrintExceptions(bool);
  };

} // namespace KJS

#endif // KJS_Interpreter_h
