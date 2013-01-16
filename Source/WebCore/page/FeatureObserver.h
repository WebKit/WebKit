/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FeatureObserver_h
#define FeatureObserver_h

#include <wtf/Noncopyable.h>

namespace WebCore {

class DOMWindow;

class FeatureObserver {
    WTF_MAKE_NONCOPYABLE(FeatureObserver);
public:
    FeatureObserver();
    ~FeatureObserver();

    enum Feature {
        PageDestruction,
        LegacyNotifications,
        UnusedSlot01, // Prior to 10/2012, we used this slot for LegacyBlobBuilder.
        PrefixedIndexedDB,
        WorkerStart,
        SharedWorkerStart,
        LegacyWebAudio,
        WebAudioStart,
        PrefixedContentSecurityPolicy,
        UnprefixedIndexedDB,
        OpenWebDatabase,
        LegacyHTMLNotifications,
        LegacyTextNotifications,
        UnprefixedRequestAnimationFrame,
        PrefixedRequestAnimationFrame,
        ContentSecurityPolicy,
        ContentSecurityPolicyReportOnly,
        PrefixedContentSecurityPolicyReportOnly,
        PrefixedTransitionEndEvent,
        UnprefixedTransitionEndEvent,
        PrefixedAndUnprefixedTransitionEndEvent,
        // Add new features above this line.
        NumberOfFeatures, // This enum value must be last.
    };

    static void observe(DOMWindow*, Feature);

private:
    void didObserve(Feature feature)
    {
        COMPILE_ASSERT(sizeof(m_featureMask) * 8 >= NumberOfFeatures, FeaturesMustNotOverflowBitmask);
        ASSERT(feature != PageDestruction); // PageDestruction is reserved as a scaling factor.
        ASSERT(feature < NumberOfFeatures);
        m_featureMask |= 1 << static_cast<int>(feature);
    }

    int m_featureMask;
};

} // namespace WebCore
    
#endif // FeatureObserver_h
