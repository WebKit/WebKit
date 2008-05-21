/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 
#ifndef Profile_h
#define Profile_h

#include "ProfileNode.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace KJS {

    class ExecState;
    class FunctionImp;
    class JSObject;

    class Profile : public RefCounted<Profile> {
    public:
        static PassRefPtr<Profile> create(const UString& title) { return adoptRef(new Profile(title)); }

        void willExecute(const Vector<CallIdentifier>& CallIdentifier);
        void didExecute(const Vector<CallIdentifier>& CallIdentifier);

        void stopProfiling() { m_callTree->stopProfiling(0, true); };
        const UString& title() const { return m_title; };
        ProfileNode* callTree() const { return m_callTree.get(); };

        double totalTime() const { return m_callTree->totalTime(); }

        void sortTotalTimeDescending() { m_callTree->sortTotalTimeDescending(); }
        void sortTotalTimeAscending() { m_callTree->sortTotalTimeAscending(); }
        void sortSelfTimeDescending() { m_callTree->sortSelfTimeDescending(); }
        void sortSelfTimeAscending() { m_callTree->sortSelfTimeAscending(); }
        void sortCallsDescending() { m_callTree->sortCallsDescending(); }
        void sortCallsAscending() { m_callTree->sortCallsAscending(); }
        void sortFunctionNameDescending() { m_callTree->sortFunctionNameDescending(); }
        void sortFunctionNameAscending() { m_callTree->sortFunctionNameAscending(); }
        
        void focus(const CallIdentifier& callIdentifier) { m_callTree->focus(callIdentifier); }

#ifndef NDEBUG
        void debugPrintData() const;
        void debugPrintDataSampleStyle() const;
#endif

    private:
        Profile(const UString& title);

        UString m_title;

        RefPtr<ProfileNode> m_callTree;
    };

} // namespace KJS

#endif // Profiler_h
