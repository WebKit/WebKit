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

#ifndef ProfileNode_h
#define ProfileNode_h

#include <kjs/ustring.h>
#include <wtf/Vector.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/StrHash.h>

namespace KJS {

    class ProfileNode;

    typedef Vector<RefPtr<ProfileNode> >::const_iterator StackIterator;
    typedef HashCountedSet<UString::Rep*> FunctionCallHashCount;

    struct CallIdentifier {
        UString name;
        UString url;
        unsigned lineNumber;
        
        CallIdentifier(UString name, UString url, int lineNumber) : name(name), url(url), lineNumber(lineNumber) {}
        CallIdentifier(const CallIdentifier& ci) : name(ci.name), url(ci.url), lineNumber(ci.lineNumber) {}

        inline bool operator== (const CallIdentifier& ci) const { return ci.name == name && ci.lineNumber == lineNumber && ci.url == url; }
    };

    class ProfileNode : public RefCounted<ProfileNode> {
    public:
        static PassRefPtr<ProfileNode> create(const CallIdentifier& callIdentifier) { return adoptRef(new ProfileNode(callIdentifier)); }

        void willExecute();
        void didExecute(const Vector<CallIdentifier>& callIdentifiers, unsigned int stackIndex);

        void addChild(PassRefPtr<ProfileNode> prpChild);
        ProfileNode* findChild(const CallIdentifier& functionName);

        void stopProfiling(double totalProfileTime, bool headProfileNode = false);

        CallIdentifier callIdentifier() const { return m_callIdentifier; }
        UString functionName() const { return m_callIdentifier.name; }
        UString url() const { return m_callIdentifier.url; }
        unsigned lineNumber() const { return m_callIdentifier.lineNumber; }

        double totalTime() const { return m_totalTime; }
        void setTotalTime(double time) { m_totalTime = time; }
        double selfTime() const { return m_selfTime; }
        void setSelfTime(double time) { m_selfTime = time; }
        double totalPercent() const { return m_totalPercent; }
        double selfPercent() const { return m_selfPercent; }
        unsigned numberOfCalls() const { return m_numberOfCalls; }
        void setNumberOfCalls(unsigned number) { m_numberOfCalls = number; }
        const Vector<RefPtr<ProfileNode> >& children() { return m_children; }

        // Sorting functions
        void sortTotalTimeDescending();
        void sortTotalTimeAscending();
        void sortSelfTimeDescending();
        void sortSelfTimeAscending();
        void sortCallsDescending();
        void sortCallsAscending();
        void sortFunctionNameDescending();
        void sortFunctionNameAscending();

        void endAndRecordCall();

        void calculatePercentages(double totalProfileTime);

        void printDataInspectorStyle(int indentLevel) const;
        double printDataSampleStyle(int indentLevel, FunctionCallHashCount&) const;

    private:
        ProfileNode(const CallIdentifier& callIdentifier);

        CallIdentifier m_callIdentifier;

        double m_startTime;
        double m_totalTime;
        double m_selfTime;
        double m_totalPercent;
        double m_selfPercent;
        unsigned m_numberOfCalls;

        Vector<RefPtr<ProfileNode> > m_children;
    };

} // namespace KJS

#endif // ProfileNode_h
