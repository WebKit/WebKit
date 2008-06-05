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

#include "config.h"
#include "RegisterFileStack.h"

using namespace std;

namespace KJS {

RegisterFileStack::~RegisterFileStack()
{
    ASSERT(m_stack.size() == 1 && m_stack[0]->size() == 0); // The stack shouldn't be destroyed during execution.
    delete m_stack[0];
}

RegisterFile* RegisterFileStack::pushGlobalRegisterFile()
{
    RegisterFile* current = this->current();

    // Common case: Existing register file is not in use: re-use it.
    if (!current->size()) {
        current->setSafeForReentry(true);
        return current;
    }

    // Slow case: Existing register file is in use: Create a nested
    // register file with a copy of this register file's globals.
    RegisterFile* lastGlobal = this->lastGlobal();

    current = allocateRegisterFile(current->maxSize() - current->size(), this);
    current->addGlobalSlots(lastGlobal->numGlobalSlots());
    current->copyGlobals(lastGlobal);
    m_globalBase = *current->basePointer();

    return current;
}

void RegisterFileStack::popGlobalRegisterFile()
{
    // Common case: This is the only register file: clear its call frames.
    if (!hasPrevious()) {
        current()->shrink(0);
        return;
    }

    // Slow case: This is a nested register file: pop this register file and
    // copy its globals to the previous register file.
    RegisterFile* tmp = m_stack.last();
    m_stack.removeLast();

    ASSERT(tmp->isGlobal() || tmp->numGlobalSlots() == lastGlobal()->numGlobalSlots());
    if (tmp->isGlobal()) {
        RegisterFile* lastGlobal = this->lastGlobal();
        lastGlobal->addGlobalSlots(tmp->numGlobalSlots() - lastGlobal->numGlobalSlots());
        lastGlobal->copyGlobals(tmp);

        m_globalBase = *lastGlobal->basePointer();
    }

    delete tmp;
}

RegisterFile* RegisterFileStack::pushFunctionRegisterFile()
{
    return allocateRegisterFile(current()->maxSize() - current()->size());
}

void RegisterFileStack::popFunctionRegisterFile()
{
    delete m_stack.last();
    m_stack.removeLast();
}

RegisterFile* RegisterFileStack::allocateRegisterFile(size_t maxSize, RegisterFileStack* registerFileStack)
{
    RegisterFile* registerFile = new RegisterFile(maxSize, registerFileStack);
    m_stack.append(registerFile);
    return registerFile;
}

} // namespace KJS
