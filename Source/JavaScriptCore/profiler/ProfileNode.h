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

#include "CallIdentifier.h"
#include <wtf/HashCountedSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace JSC {

    class ExecState;
    class ProfileNode;

    typedef HashCountedSet<StringImpl*> FunctionCallHashCount;

    class ProfileNode : public RefCounted<ProfileNode> {
    public:
        static PassRefPtr<ProfileNode> create(ExecState* callerCallFrame, const CallIdentifier& callIdentifier, ProfileNode* headNode, ProfileNode* parentNode)
        {
            return adoptRef(new ProfileNode(callerCallFrame, callIdentifier, headNode, parentNode));
        }

        static PassRefPtr<ProfileNode> create(ExecState* callerCallFrame, ProfileNode* headNode, ProfileNode* node)
        {
            return adoptRef(new ProfileNode(callerCallFrame, headNode, node));
        }

        bool operator==(ProfileNode* node) { return m_callIdentifier == node->callIdentifier(); }

        ProfileNode* willExecute(ExecState* callerCallFrame, const CallIdentifier&);
        ProfileNode* didExecute();

        void stopProfiling();

        // CallIdentifier members
        ExecState* callerCallFrame() const { return m_callerCallFrame; }
        const CallIdentifier& callIdentifier() const { return m_callIdentifier; }
        unsigned long callUID() const { return m_callIdentifier.hash(); };
        const String& functionName() const { return m_callIdentifier.m_name; }
        const String& url() const { return m_callIdentifier.m_url; }
        unsigned lineNumber() const { return m_callIdentifier.m_lineNumber; }

        // Relationships
        ProfileNode* head() const { return m_head; }
        void setHead(ProfileNode* head) { m_head = head; }

        ProfileNode* parent() const { return m_parent; }
        void setParent(ProfileNode* parent) { m_parent = parent; }

        ProfileNode* nextSibling() const { return m_nextSibling; }
        void setNextSibling(ProfileNode* nextSibling) { m_nextSibling = nextSibling; }

        // Time members
        double startTime() const { return m_startTime; }
        void setStartTime(double startTime) { m_startTime = startTime; }

        double totalTime() const { return m_totalTime; }
        void setTotalTime(double time) { m_totalTime = time; }

        double selfTime() const { return m_selfTime; }
        void setSelfTime(double time) { m_selfTime = time; }

        double totalPercent() const { return (m_totalTime / (m_head ? m_head->totalTime() : totalTime())) * 100.0; }
        double selfPercent() const { return (m_selfTime / (m_head ? m_head->totalTime() : totalTime())) * 100.0; }

        unsigned numberOfCalls() const { return m_numberOfCalls; }

        // Children members
        const Vector<RefPtr<ProfileNode>>& children() const { return m_children; }
        ProfileNode* firstChild() const { return m_children.size() ? m_children.first().get() : 0; }
        ProfileNode* lastChild() const { return m_children.size() ? m_children.last().get() : 0; }
        void removeChild(ProfileNode*);
        void addChild(PassRefPtr<ProfileNode> prpChild);
        void insertNode(PassRefPtr<ProfileNode> prpNode);

        ProfileNode* traverseNextNodePostOrder() const;

        void endAndRecordCall();

#ifndef NDEBUG
        const char* c_str() const { return m_callIdentifier; }
        void debugPrintData(int indentLevel) const;
        double debugPrintDataSampleStyle(int indentLevel, FunctionCallHashCount&) const;
#endif

    private:
        typedef Vector<RefPtr<ProfileNode>>::const_iterator StackIterator;

        ProfileNode(ExecState* callerCallFrame, const CallIdentifier&, ProfileNode* headNode, ProfileNode* parentNode);
        ProfileNode(ExecState* callerCallFrame, ProfileNode* headNode, ProfileNode* nodeToCopy);

        void startTimer();
        void resetChildrensSiblings();

        ExecState* m_callerCallFrame;
        CallIdentifier m_callIdentifier;
        ProfileNode* m_head;
        ProfileNode* m_parent;
        ProfileNode* m_nextSibling;

        double m_startTime;
        double m_totalTime;
        double m_selfTime;
        unsigned m_numberOfCalls;

        Vector<RefPtr<ProfileNode>> m_children;
    };

} // namespace JSC

#endif // ProfileNode_h
