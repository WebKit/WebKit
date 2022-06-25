/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#pragma once

#include <wtf/HashSet.h>
#include <wtf/Hasher.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
OBJC_CLASS NSRunLoop;
#endif

namespace WTF {

class SchedulePair : public ThreadSafeRefCounted<SchedulePair> {
public:
    static Ref<SchedulePair> create(CFRunLoopRef runLoop, CFStringRef mode) { return adoptRef(*new SchedulePair(runLoop, mode)); }

#if PLATFORM(COCOA)
    static Ref<SchedulePair> create(NSRunLoop* runLoop, CFStringRef mode) { return adoptRef(*new SchedulePair(runLoop, mode)); }
    NSRunLoop* nsRunLoop() const { return m_nsRunLoop.get(); }
#endif

    CFRunLoopRef runLoop() const { return m_runLoop.get(); }
    CFStringRef mode() const { return m_mode.get(); }

    WTF_EXPORT_PRIVATE bool operator==(const SchedulePair& other) const;

private:
    SchedulePair(CFRunLoopRef runLoop, CFStringRef mode)
        : m_runLoop(runLoop)
    {
        if (mode)
            m_mode = adoptCF(CFStringCreateCopy(nullptr, mode));
    }

#if PLATFORM(COCOA)
    WTF_EXPORT_PRIVATE SchedulePair(NSRunLoop*, CFStringRef);
    RetainPtr<NSRunLoop> m_nsRunLoop;
#endif

    RetainPtr<CFRunLoopRef> m_runLoop;
    RetainPtr<CFStringRef> m_mode;
};

inline void add(Hasher& hasher, const SchedulePair& pair)
{
    // FIXME: Hashing a CFHash here is unfortunate.
    add(hasher, pair.runLoop(), pair.mode() ? CFHash(pair.mode()) : 0);
}

struct SchedulePairHash {
    static unsigned hash(const RefPtr<SchedulePair>& pair)
    {
        return computeHash(*pair);
    }

    static bool equal(const RefPtr<SchedulePair>& a, const RefPtr<SchedulePair>& b) { return a == b; }

    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

typedef HashSet<RefPtr<SchedulePair>, SchedulePairHash> SchedulePairHashSet;

} // namespace WTF

using WTF::SchedulePair;
using WTF::SchedulePairHashSet;
