/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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

#import "config.h"
#import "KillRing.h"

#if PLATFORM(MAC)

namespace PAL {

extern "C" {

// Kill ring calls. Would be better to use NSKillRing.h, but that's not available as API or SPI.

void _NSInitializeKillRing();
void _NSAppendToKillRing(NSString *);
void _NSPrependToKillRing(NSString *);
NSString *_NSYankFromKillRing();
void _NSNewKillRingSequence();
void _NSSetKillRingToYankedState();
void _NSResetKillRingOperationFlag();

}

static void initializeKillRingIfNeeded()
{
    static bool initializedKillRing = false;
    if (!initializedKillRing) {
        initializedKillRing = true;
        _NSInitializeKillRing();
    }
}

void KillRing::append(const String& string)
{
    initializeKillRingIfNeeded();
    // Necessary to prevent an implicit new sequence if the previous command was NSPrependToKillRing.
    _NSResetKillRingOperationFlag();
    _NSAppendToKillRing(string);
}

void KillRing::prepend(const String& string)
{
    initializeKillRingIfNeeded();
    // Necessary to prevent an implicit new sequence if the previous command was NSAppendToKillRing.
    _NSResetKillRingOperationFlag();
    _NSPrependToKillRing(string);
}

String KillRing::yank()
{
    initializeKillRingIfNeeded();
    return _NSYankFromKillRing();
}

void KillRing::startNewSequence()
{
    initializeKillRingIfNeeded();
    _NSNewKillRingSequence();
}

void KillRing::setToYankedState()
{
    initializeKillRingIfNeeded();
    _NSSetKillRingToYankedState();
}

} // namespace PAL

#endif // PLATFORM(MAC)
