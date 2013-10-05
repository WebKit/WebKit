/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

namespace JSC {

class DebuggerCallFrame;
class ExecState;
class VM;
class JSGlobalObject;
class JSValue;
class SourceProvider;

typedef ExecState CallFrame;

class JS_EXPORT_PRIVATE Debugger {
public:
    virtual ~Debugger();

    void attach(JSGlobalObject*);
    virtual void detach(JSGlobalObject*);

    virtual void sourceParsed(ExecState*, SourceProvider*, int errorLineNumber, const WTF::String& errorMessage) = 0;

    virtual void exception(CallFrame*, JSValue exceptionValue, bool hasHandler) = 0;
    virtual void atStatement(CallFrame*) = 0;
    virtual void callEvent(CallFrame*) = 0;
    virtual void returnEvent(CallFrame*) = 0;

    virtual void willExecuteProgram(CallFrame*) = 0;
    virtual void didExecuteProgram(CallFrame*) = 0;
    virtual void didReachBreakpoint(CallFrame*) = 0;

    void recompileAllJSFunctions(VM*);

private:
    HashSet<JSGlobalObject*> m_globalObjects;
};

} // namespace JSC

#endif // Debugger_h
