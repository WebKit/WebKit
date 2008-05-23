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
 
#ifndef RegisterFile_h
#define RegisterFile_h

#include "Register.h"
#include "collector.h"
#include <wtf/Noncopyable.h>

namespace KJS {

/*
    A register file is a stack of register frames. We represent a register
    frame by its offset from "base", the logical first entry in the register
    file. The bottom-most register frame's offset from base is 0.
    
    In a program where function "a" calls function "b" (global code -> a -> b),
    the register file might look like this:

    |       global frame     |        call frame      |        call frame      |     spare capacity     |
    -----------------------------------------------------------------------------------------------------
    |  0 |  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8 |  9 | 10 | 11 | 12 | 13 | 14 |    |    |    |    |    | <-- index in buffer
    -----------------------------------------------------------------------------------------------------
    | -3 | -2 | -1 |  0 |  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8 |  9 | 10 | 11 |    |    |    |    |    | <-- index relative to base
    -----------------------------------------------------------------------------------------------------
    |    <-globals | temps-> |  <-vars | temps->      |                 <-vars |
       ^              ^                   ^                                       ^
       |              |                   |                                       |
     buffer    base (frame 0)          frame 1                                 frame 2
     
    Since all variables, including globals, are accessed by negative offsets
    from their register frame pointers, to keep old global offsets correct, new
    globals must appear at the beginning of the register file, shifting base
    to the right.
    
    If we added one global variable to the register file depicted above, it
    would look like this:

    |         global frame        |<                                                                    >
    ------------------------------->                                                                    <
    |  0 |  1 |  2 |  3 |  4 |  5 |<                             >snip<                                 > <-- index in buffer
    ------------------------------->                                                                    <
    | -4 | -3 | -2 | -1 |  0 |  1 |<                                                                    > <-- index relative to base
    ------------------------------->                                                                    <
    |         <-globals | temps-> |
       ^                   ^       
       |                   |       
     buffer         base (frame 0)

    As you can see, global offsets relative to base have stayed constant,
    but base itself has moved. To keep up with possible changes to base,
    clients keep an indirect pointer, so their calculations update
    automatically when base changes.
    
    For client simplicity, the RegisterFile measures size and capacity from
    "base", not "buffer".
*/

    class RegisterFileStack;
    
    class RegisterFile : Noncopyable {
    public:
        enum { DefaultRegisterFileSize = 2 * 1024 * 1024 };
        RegisterFile(size_t maxSize, RegisterFileStack* m_baseObserver)
            : m_safeForReentry(true)
            , m_size(0)
            , m_capacity(0)
            , m_maxSize(maxSize)
            , m_base(0)
            , m_buffer(0)
            , m_baseObserver(m_baseObserver)
        {
        }
        
        ~RegisterFile()
        {
            setBuffer(0);
        }
        
        // Pointer to a value that holds the base of this register file.
        Register** basePointer() { return &m_base; }
        
        void shrink(size_t size)
        {
            if (size < m_size)
                m_size = size;
        }

        bool grow(size_t size)
        {
            if (size > m_size) {
                if (size > m_capacity) {
                    if (size > m_maxSize)
                        return false;
                    growBuffer(size, m_maxSize);
                }
                m_size = size;
            }
            return true;
        }

        size_t size() { return m_size; }
        size_t maxSize() { return m_maxSize; }
        
        void clear();

        void addGlobalSlots(size_t count);
        int numGlobalSlots() { return static_cast<int>(m_base - m_buffer); }

        void copyGlobals(RegisterFile* src);

        void mark()
        {
            Collector::markStackObjectsConservatively(m_buffer, m_base + m_size);
        }

        bool isGlobal() { return !!m_baseObserver; }

        bool safeForReentry() { return m_safeForReentry; }
        void setSafeForReentry(bool safeForReentry) { m_safeForReentry = safeForReentry; }
    private:
        size_t newBuffer(size_t size, size_t capacity, size_t minCapacity, size_t maxSize, size_t offset);
        bool growBuffer(size_t minCapacity, size_t maxSize);
        void setBuffer(Register* buffer)
        {
            if (m_buffer)
                fastFree(m_buffer);

            m_buffer = buffer;
        }
        
        void setBase(Register*);
        bool m_safeForReentry;
        size_t m_size;
        size_t m_capacity;
        size_t m_maxSize;
        Register* m_base;
        Register* m_buffer;
        RegisterFileStack* m_baseObserver;
    };
    
} // namespace KJS

#endif // RegisterFile_h
