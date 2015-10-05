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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CellState_h
#define CellState_h

namespace JSC {

enum class CellState : uint8_t {
    // The object is black as far as this GC is concerned. When not in GC, this just means that it's an
    // old gen object. Note that we deliberately arrange OldBlack to be zero, so that the store barrier on
    // a target object "from" is just:
    //
    // if (!from->cellState())
    //     slowPath(from);
    //
    // There is a bunch of code in the LLInt and JITs that rely on this being the case. You'd have to
    // change a lot of code if you ever wanted the store barrier to be anything but a non-zero check on
    // cellState.
    OldBlack = 0,
    
    // The object is in eden. During GC, this means that the object has not been marked yet.
    NewWhite = 1,

    // The object is grey - i.e. it will be scanned - but it either belongs to old gen (if this is eden
    // GC) or it is grey a second time in this current GC (because a concurrent store barrier requested
    // re-greying).
    OldGrey = 2,

    // The object is grey - i.e. it will be scanned - and this is the first time in this GC that we are
    // going to scan it. If this is an eden GC, this also means that the object is in eden.
    NewGrey = 3
};

} // namespace JSC

#endif // CellState_h

