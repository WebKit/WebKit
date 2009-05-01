/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "Protect.h"

namespace JSC {

    class DebuggerCallFrame;
    class ExecState;
    class JSGlobalObject;
    class SourceCode;
    class UString;

    class Debugger {
    public:
        Debugger();
        virtual ~Debugger();

        void attach(JSGlobalObject*);
        void detach(JSGlobalObject*);

        virtual void sourceParsed(ExecState*, const SourceCode&, int errorLine, const UString& errorMsg) = 0;
        virtual void exception(const DebuggerCallFrame&, intptr_t sourceID, int lineno) = 0;
        virtual void atStatement(const DebuggerCallFrame&, intptr_t sourceID, int lineno) = 0;
        virtual void callEvent(const DebuggerCallFrame&, intptr_t sourceID, int lineno) = 0;
        virtual void returnEvent(const DebuggerCallFrame&, intptr_t sourceID, int lineno) = 0;

        virtual void willExecuteProgram(const DebuggerCallFrame&, intptr_t sourceID, int lineno) = 0;
        virtual void didExecuteProgram(const DebuggerCallFrame&, intptr_t sourceID, int lineno) = 0;
        virtual void didReachBreakpoint(const DebuggerCallFrame&, intptr_t sourceID, int lineno) = 0;

    private:
        HashSet<JSGlobalObject*> m_globalObjects;
    };

    // This method exists only for backwards compatibility with existing
    // WebScriptDebugger clients
    JSValue evaluateInGlobalCallFrame(const UString&, JSValue& exception, JSGlobalObject*);

} // namespace JSC

#endif // Debugger_h
