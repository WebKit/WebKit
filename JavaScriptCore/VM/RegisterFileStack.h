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

#ifndef RegisterFileStack_h
#define RegisterFileStack_h

#include "RegisterFile.h"
#include <wtf/Vector.h>

namespace KJS {

    class RegisterFileStack {
    public:
        RegisterFileStack()
            : m_globalBase(0)
        {
            allocateRegisterFile(RegisterFile::DefaultRegisterFileSize, this);
        }

        ~RegisterFileStack();

        RegisterFile* pushGlobalRegisterFile();
        void popGlobalRegisterFile();
        RegisterFile* pushFunctionRegisterFile();
        void popFunctionRegisterFile();

        RegisterFile* current() { return m_stack.last(); }

        void mark(Heap* heap)
        {
            Stack::iterator end = m_stack.end();
            for (Stack::iterator it = m_stack.begin(); it != end; ++it)
                (*it)->mark(heap);
        }

        // Pointer to a value that holds the base of the top-most global register file.
        Register** globalBasePointer() { return &m_globalBase; }

        void baseChanged(RegisterFile* registerFile)
        {
            ASSERT(registerFile == lastGlobal());
            m_globalBase = *registerFile->basePointer();
        }

        bool inImplicitCall()
        {
            for (size_t i = 0; i < m_stack.size(); ++i) {
                if (!m_stack[i]->safeForReentry())
                    return true;
            }
            return false;
        }

    private:
        typedef Vector<RegisterFile*, 4> Stack;

        RegisterFile* lastGlobal()
        {
            ASSERT(m_stack.size());
            for (size_t i = m_stack.size() - 1; i > 0; --i) {
                if (m_stack[i]->isGlobal())
                    return m_stack[i];
            }
            ASSERT(m_stack[0]->isGlobal());
            return m_stack[0];
        }

        RegisterFile* allocateRegisterFile(size_t maxSize, RegisterFileStack* = 0);

        RegisterFile* previous() { return m_stack[m_stack.size() - 2]; }
        bool hasPrevious() { return m_stack.size() > 1; }

        Stack m_stack;
        Register* m_globalBase;
    };

} // namespace KJS

#endif // RegisterFileStack_h
