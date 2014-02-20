/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef DFGFlushFormat_h
#define DFGFlushFormat_h

#if ENABLE(DFG_JIT)

#include "DFGNodeFlags.h"
#include "DFGUseKind.h"
#include "DataFormat.h"
#include "DumpContext.h"
#include <wtf/PrintStream.h>

namespace JSC { namespace DFG {

enum FlushFormat {
    DeadFlush,
    FlushedInt32,
    FlushedInt52,
    FlushedDouble,
    FlushedCell,
    FlushedBoolean,
    FlushedJSValue,
    FlushedArguments,
    ConflictingFlush
};

inline NodeFlags resultFor(FlushFormat format)
{
    switch (format) {
    case DeadFlush:
    case FlushedJSValue:
    case FlushedCell:
    case ConflictingFlush:
    case FlushedArguments:
        return NodeResultJS;
    case FlushedInt32:
        return NodeResultInt32;
    case FlushedInt52:
        return NodeResultInt52;
    case FlushedDouble:
        return NodeResultNumber;
    case FlushedBoolean:
        return NodeResultBoolean;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

inline UseKind useKindFor(FlushFormat format)
{
    switch (format) {
    case DeadFlush:
    case FlushedJSValue:
    case ConflictingFlush:
    case FlushedArguments:
        return UntypedUse;
    case FlushedCell:
        return CellUse;
    case FlushedInt32:
        return Int32Use;
    case FlushedInt52:
        return MachineIntUse;
    case FlushedDouble:
        return NumberUse;
    case FlushedBoolean:
        return BooleanUse;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return UntypedUse;
}

inline DataFormat dataFormatFor(FlushFormat format)
{
    switch (format) {
    case DeadFlush:
    case ConflictingFlush:
        return DataFormatDead;
    case FlushedJSValue:
        return DataFormatJS;
    case FlushedDouble:
        return DataFormatDouble;
    case FlushedInt32:
        return DataFormatInt32;
    case FlushedInt52:
        return DataFormatInt52;
    case FlushedCell:
        return DataFormatCell;
    case FlushedBoolean:
        return DataFormatBoolean;
    case FlushedArguments:
        return DataFormatArguments;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return DataFormatDead;
}

} } // namespace JSC::DFG

namespace WTF {

void printInternal(PrintStream&, JSC::DFG::FlushFormat);

inline JSC::DFG::FlushFormat inContext(JSC::DFG::FlushFormat format, JSC::DumpContext*)
{
    return format;
}

} // namespace WTF

#endif // ENABLE(DFG_JIT)

#endif // DFGFlushFormat_h

