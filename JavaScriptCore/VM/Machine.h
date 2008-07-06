/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Machine_h
#define Machine_h

#include "ArgList.h"
#include "JSCell.h"
#include "JSValue.h"
#include "Opcode.h"
#include "RegisterFile.h"
#include <wtf/HashMap.h>

namespace KJS {

    class CodeBlock;
    class EvalNode;
    class ExecState;
    class FunctionBodyNode;
    class Instruction;
    class JSFunction;
    class JSGlobalObject;
    class ProgramNode;
    class Register;
    class ScopeChainNode;

    enum DebugHookID {
        WillExecuteProgram,
        DidExecuteProgram,
        DidEnterCallFrame,
        DidReachBreakpoint,
        WillLeaveCallFrame,
        WillExecuteStatement
    };

    enum { MaxReentryDepth = 128 };

    class Machine {
    public:
        Machine();
        
        RegisterFile& registerFile() { return m_registerFile; }
        
        Opcode getOpcode(OpcodeID id)
        {
            #if HAVE(COMPUTED_GOTO)
                return m_opcodeTable[id];
            #else
                return id;
            #endif
        }

        OpcodeID getOpcodeID(Opcode opcode)
        {
            #if HAVE(COMPUTED_GOTO)
                ASSERT(isOpcode(opcode));
                return m_opcodeIDTable.get(opcode);
            #else
                return opcode;
            #endif
        }

        bool isOpcode(Opcode opcode);
        
        JSValue* execute(ProgramNode*, ExecState*, ScopeChainNode*, JSObject* thisObj, JSValue** exception);
        JSValue* execute(FunctionBodyNode*, ExecState*, JSFunction*, JSObject* thisObj, const ArgList& args, ScopeChainNode*, JSValue** exception);
        JSValue* execute(EvalNode* evalNode, ExecState* exec, JSObject* thisObj, ScopeChainNode* scopeChain, JSValue** exception)
        {
            return execute(evalNode, exec, thisObj, m_registerFile.size(), scopeChain, exception);
        }

        JSValue* retrieveArguments(ExecState*, JSFunction*) const;
        JSValue* retrieveCaller(ExecState*, JSFunction*) const;

        void getArgumentsData(Register* callFrame, JSFunction*&, Register*& argv, int& argc);
        void setTimeoutTime(unsigned timeoutTime) { m_timeoutTime = timeoutTime; }
        
        void startTimeoutCheck()
        {
            if (!m_timeoutCheckCount)
                resetTimeoutCheck();
            
            ++m_timeoutCheckCount;
        }
        
        void stopTimeoutCheck()
        {
            --m_timeoutCheckCount;
        }

        inline void initTimeout()
        {
            resetTimeoutCheck();
            m_timeoutTime = 0;
            m_timeoutCheckCount = 0;
        }

    private:
        enum ExecutionFlag { Normal, InitializeAndReturn };

        friend NEVER_INLINE JSValue* callEval(ExecState* exec, JSObject* thisObj, ScopeChainNode* scopeChain, RegisterFile*, Register* r, int argv, int argc, JSValue*& exceptionValue);
        JSValue* execute(EvalNode*, ExecState*, JSObject* thisObj, int registerOffset, ScopeChainNode*, JSValue** exception);

        ALWAYS_INLINE void setScopeChain(ExecState* exec, ScopeChainNode*&, ScopeChainNode*);
        NEVER_INLINE void debug(ExecState*, const Instruction*, const CodeBlock*, ScopeChainNode*, Register*);

        NEVER_INLINE bool unwindCallFrame(ExecState*, JSValue*, const Instruction*&, CodeBlock*&, JSValue**&, ScopeChainNode*&, Register*&);
        NEVER_INLINE Instruction* throwException(ExecState*, JSValue*, const Instruction*, CodeBlock*&, JSValue**&, ScopeChainNode*&, Register*&);

        Register* callFrame(ExecState*, JSFunction*) const;

        JSValue* privateExecute(ExecutionFlag, ExecState* = 0, RegisterFile* = 0, Register* = 0, ScopeChainNode* = 0, CodeBlock* = 0, JSValue** exception = 0);

        void dumpCallFrame(const CodeBlock*, ScopeChainNode*, RegisterFile*, const Register*);
        void dumpRegisters(const CodeBlock*, RegisterFile*, const Register*);

        JSValue* checkTimeout(JSGlobalObject*);
        void resetTimeoutCheck();

        bool isJSArray(JSValue* v) { return !JSImmediate::isImmediate(v) && v->asCell()->vptr() == m_jsArrayVptr; }
        bool isJSString(JSValue* v) { return !JSImmediate::isImmediate(v) && v->asCell()->vptr() == m_jsStringVptr; }
        
        int m_reentryDepth;
        unsigned m_timeoutTime;
        unsigned m_timeAtLastCheckTimeout;
        unsigned m_timeExecuting;
        unsigned m_timeoutCheckCount;
        unsigned m_ticksUntilNextTimeoutCheck;

        RegisterFile m_registerFile;
        
        void* m_jsArrayVptr;
        void* m_jsStringVptr;

#if HAVE(COMPUTED_GOTO)
        Opcode m_opcodeTable[numOpcodeIDs]; // Maps OpcodeID => Opcode for compiling
        HashMap<Opcode, OpcodeID> m_opcodeIDTable; // Maps Opcode => OpcodeID for decompiling
#endif
    };

} // namespace KJS

#endif // Machine_h
