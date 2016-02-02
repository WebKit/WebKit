/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "B3Effects.h"

#if ENABLE(B3_JIT)

#include <wtf/CommaPrinter.h>
#include <wtf/DataLog.h>

namespace JSC { namespace B3 {

namespace {

// These helpers cascade in such a way that after the helper for terminal, we don't have to worry
// about terminal again, since the terminal case considers all ways that a terminal may interfere
// with something else. And after the exit sideways case, we don't have to worry about either
// exitsSideways or terminal. And so on...

bool interferesWithTerminal(const Effects& terminal, const Effects& other)
{
    if (!terminal.terminal)
        return false;
    return other.terminal || other.controlDependent || other.writesLocalState || other.writes;
}

bool interferesWithExitSideways(const Effects& exitsSideways, const Effects& other)
{
    if (!exitsSideways.exitsSideways)
        return false;
    return other.controlDependent || other.writes;
}

bool interferesWithWritesLocalState(const Effects& writesLocalState, const Effects& other)
{
    if (!writesLocalState.writesLocalState)
        return false;
    return other.writesLocalState || other.readsLocalState;
}

} // anonymous namespace

bool Effects::interferes(const Effects& other) const
{
    if (interferesWithTerminal(*this, other) || interferesWithTerminal(other, *this))
        return true;
    if (interferesWithExitSideways(*this, other) || interferesWithExitSideways(other, *this))
        return true;
    if (interferesWithWritesLocalState(*this, other) || interferesWithWritesLocalState(other, *this))
        return true;
    return writes.overlaps(other.writes)
        || writes.overlaps(other.reads)
        || reads.overlaps(other.writes);
}

void Effects::dump(PrintStream& out) const
{
    CommaPrinter comma("|");
    if (terminal)
        out.print(comma, "Terminal");
    if (exitsSideways)
        out.print(comma, "ExitsSideways");
    if (controlDependent)
        out.print(comma, "ControlDependent");
    if (writesLocalState)
        out.print(comma, "WritesLocalState");
    if (readsLocalState)
        out.print(comma, "ReadsLocalState");
    if (writes)
        out.print(comma, "Writes:", writes);
    if (reads)
        out.print(comma, "Reads:", reads);
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

