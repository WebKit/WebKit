/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef SessionIDHash_h
#define SessionIDHash_h

#include "SessionID.h"
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>

namespace WTF {

// The empty value is emptySessionID(), the deleted value is (-1)
struct SessionIDHash {
    static unsigned hash(const WebCore::SessionID& p) { return (unsigned)p.sessionID(); }
    static bool equal(const WebCore::SessionID& a, const WebCore::SessionID& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};
template<> struct HashTraits<WebCore::SessionID> : GenericHashTraits<WebCore::SessionID> {
    static const bool needsDestruction = false;
    static WebCore::SessionID emptyValue() { return WebCore::SessionID::emptySessionID(); }

    static void constructDeletedValue(WebCore::SessionID& slot) { slot = WebCore::SessionID(-2); }
    static bool isDeletedValue(const WebCore::SessionID& slot) { return slot == WebCore::SessionID(-2); }
};
template<> struct DefaultHash<WebCore::SessionID> {
    typedef SessionIDHash Hash;
};

}

#endif // SessionIDHash_h
