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

        inline bool operator== (const CallIdentifier& ci) const { return ci.lineNumber == lineNumber && ci.name == name && ci.url == url; }
    };

    class ProfileNode : public RefCounted<ProfileNode> {
    public:
        static PassRefPtr<ProfileNode> create(const CallIdentifier& callIdentifier, ProfileNode* headNode, ProfileNode* parentNode) {
            return adoptRef(new ProfileNode(callIdentifier, headNode, parentNode)); }

        void willExecute();
        ProfileNode* didExecute();

        ProfileNode* addOrStartChild(const CallIdentifier&);

        void stopProfiling();

        const CallIdentifier& callIdentifier() const { return m_callIdentifier; }
        ProfileNode* parent() const { return m_parentNode; }
        UString functionName() const { return m_callIdentifier.name; }
        UString url() const { return m_callIdentifier.url; }
        unsigned lineNumber() const { return m_callIdentifier.lineNumber; }

        double totalTime() const { return m_visibleTotalTime; }
        void setTotalTime(double time) { m_actualTotalTime = time; m_visibleTotalTime = time; }
        double selfTime() const { return m_visibleSelfTime; }
        void setSelfTime(double time) { m_actualSelfTime = time; m_visibleTotalTime = time; }
        double totalPercent() const { return (m_visibleTotalTime / m_headNode->totalTime()) * 100.0; }
        double selfPercent() const { return (m_visibleSelfTime / m_headNode->totalTime()) * 100.0; }
        unsigned numberOfCalls() const { return m_numberOfCalls; }
        void setNumberOfCalls(unsigned number) { m_numberOfCalls = number; }
        const Vector<RefPtr<ProfileNode> >& children() { return m_children; }
        bool visible() const { return m_visible; }
        void setVisible(bool visible) { m_visible = visible; }

        // Sorting functions
        void sortTotalTimeDescending();
        void sortTotalTimeAscending();
        void sortSelfTimeDescending();
        void sortSelfTimeAscending();
        void sortCallsDescending();
        void sortCallsAscending();
        void sortFunctionNameDescending();
        void sortFunctionNameAscending();

        void focus(const CallIdentifier& callIdentifier, bool forceVisible = false);
        void restoreAll();

        void endAndRecordCall();
        
#ifndef NDEBUG
        void debugPrintData(int indentLevel) const;
        double debugPrintDataSampleStyle(int indentLevel, FunctionCallHashCount&) const;
#endif
    private:
        ProfileNode(const CallIdentifier& callIdentifier, ProfileNode* headNode, ProfileNode* parentNode);

        CallIdentifier m_callIdentifier;
        ProfileNode* m_headNode;
        ProfileNode* m_parentNode;

        double m_startTime;
        double m_actualTotalTime;
        double m_visibleTotalTime;
        double m_actualSelfTime;
        double m_visibleSelfTime;
        unsigned m_numberOfCalls;

        bool m_visible;

        Vector<RefPtr<ProfileNode> > m_children;
    };

} // namespace KJS

#endif // ProfileNode_h
