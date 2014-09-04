/*
 * Copyright (C) 2008, 2014 Apple Inc. All rights reserved.
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
        static PassRefPtr<ProfileNode> create(ExecState* callerCallFrame, const CallIdentifier& callIdentifier, ProfileNode* parentNode)
        {
            return adoptRef(new ProfileNode(callerCallFrame, callIdentifier, parentNode));
        }

        static PassRefPtr<ProfileNode> create(ExecState* callerCallFrame, ProfileNode* node)
        {
            return adoptRef(new ProfileNode(callerCallFrame, node));
        }

        struct Call {
        public:
            Call(double startTime, double totalTime = NAN)
                : m_startTime(startTime)
                , m_totalTime(totalTime)
            {
            }

            double startTime() const { return m_startTime; }
            void setStartTime(double time)
            {
                ASSERT_ARG(time, time >= 0.0);
                m_startTime = time;
            }

            double totalTime() const { return m_totalTime; }
            void setTotalTime(double time)
            {
                ASSERT_ARG(time, time >= 0.0);
                m_totalTime = time;
            }

        private:
            double m_startTime;
            double m_totalTime;
        };

        bool operator==(ProfileNode* node) { return m_callIdentifier == node->callIdentifier(); }

        ExecState* callerCallFrame() const { return m_callerCallFrame; }
        const CallIdentifier& callIdentifier() const { return m_callIdentifier; }
        unsigned id() const { return m_callIdentifier.hash(); }
        const String& functionName() const { return m_callIdentifier.functionName(); }
        const String& url() const { return m_callIdentifier.url(); }
        unsigned lineNumber() const { return m_callIdentifier.lineNumber(); }
        unsigned columnNumber() const { return m_callIdentifier.columnNumber(); }

        ProfileNode* parent() const { return m_parent; }
        void setParent(ProfileNode* parent) { m_parent = parent; }

        const Vector<Call>& calls() const { return m_calls; }
        Call& lastCall() { ASSERT(!m_calls.isEmpty()); return m_calls.last(); }
        void appendCall(Call call) { m_calls.append(call); }

        const Vector<RefPtr<ProfileNode>>& children() const { return m_children; }
        ProfileNode* firstChild() const { return m_children.size() ? m_children.first().get() : nullptr; }
        ProfileNode* lastChild() const { return m_children.size() ? m_children.last().get() : nullptr; }

        void removeChild(ProfileNode*);
        void addChild(PassRefPtr<ProfileNode>);
        // Reparent our child nodes to the passed node, and make it a child node of |this|.
        void spliceNode(PassRefPtr<ProfileNode>);

#ifndef NDEBUG
        struct ProfileSubtreeData {
            HashMap<ProfileNode*, std::pair<double, double>> selfAndTotalTimes;
            double rootTotalTime;
        };

        // Use these functions to dump the subtree rooted at this node.
        void debugPrint();
        void debugPrintSampleStyle();

        // These are used to recursively print entire subtrees using precomputed self and total times.
        template <typename Functor> void forEachNodePostorder(Functor&);

        void debugPrintRecursively(int indentLevel, const ProfileSubtreeData&);
        double debugPrintSampleStyleRecursively(int indentLevel, FunctionCallHashCount&, const ProfileSubtreeData&);
#endif

    private:
        typedef Vector<RefPtr<ProfileNode>>::const_iterator StackIterator;

        ProfileNode(ExecState* callerCallFrame, const CallIdentifier&, ProfileNode* parentNode);
        ProfileNode(ExecState* callerCallFrame, ProfileNode* nodeToCopy);

#ifndef NDEBUG
        ProfileNode* nextSibling() const { return m_nextSibling; }
        void setNextSibling(ProfileNode* nextSibling) { m_nextSibling = nextSibling; }

        ProfileNode* traverseNextNodePostOrder() const;
#endif

        ExecState* m_callerCallFrame;
        CallIdentifier m_callIdentifier;
        ProfileNode* m_parent;
        Vector<Call> m_calls;
        Vector<RefPtr<ProfileNode>> m_children;

#ifndef NDEBUG
        ProfileNode* m_nextSibling;
#endif
    };

#ifndef NDEBUG
    template <typename Functor> inline void ProfileNode::forEachNodePostorder(Functor& functor)
    {
        ProfileNode* currentNode = this;
        // Go down to the first node of the traversal, and slowly walk back up.
        for (ProfileNode* nextNode = currentNode; nextNode; nextNode = nextNode->firstChild())
            currentNode = nextNode;

        ProfileNode* endNode = this;
        while (currentNode && currentNode != endNode) {
            functor(currentNode);
            currentNode = currentNode->traverseNextNodePostOrder();
        }

        functor(endNode);
    }

    struct CalculateProfileSubtreeDataFunctor {
        void operator()(ProfileNode* node)
        {
            double selfTime = 0.0;
            for (const ProfileNode::Call& call : node->calls())
                selfTime += call.totalTime();

            double totalTime = selfTime;
            for (RefPtr<ProfileNode> child : node->children()) {
                auto it = m_data.selfAndTotalTimes.find(child.get());
                if (it != m_data.selfAndTotalTimes.end())
                    totalTime += it->value.second;
            }

            ASSERT(node);
            m_data.selfAndTotalTimes.set(node, std::make_pair(selfTime, totalTime));
        }

        ProfileNode::ProfileSubtreeData returnValue() { return WTF::move(m_data); }

        ProfileNode::ProfileSubtreeData m_data;
    };
#endif

} // namespace JSC

#endif // ProfileNode_h
