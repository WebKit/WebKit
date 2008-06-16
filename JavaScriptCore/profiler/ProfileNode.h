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

namespace KJS {

    class ProfileNode;

    typedef Vector<RefPtr<ProfileNode> >::const_iterator StackIterator;
    typedef HashCountedSet<UString::Rep*> FunctionCallHashCount;

    struct CallIdentifier {
        UString name;
        UString url;
        unsigned lineNumber;
        
        CallIdentifier(UString name, UString url, int lineNumber)
            : name(name)
            , url(url)
            , lineNumber(lineNumber)
        {
        }

        CallIdentifier(const CallIdentifier& ci)
            : name(ci.name)
            , url(ci.url)
            , lineNumber(ci.lineNumber)
        {
        }

        inline bool operator==(const CallIdentifier& ci) const { return ci.lineNumber == lineNumber && ci.name == name && ci.url == url; }
        inline bool operator!=(const CallIdentifier& ci) const { return !(*this == ci); }

#ifndef NDEBUG
        const char* toString() const { return name.UTF8String().c_str(); }
#endif
    };

    class ProfileNode : public RefCounted<ProfileNode> {
    public:
        static PassRefPtr<ProfileNode> create(const CallIdentifier& callIdentifier, ProfileNode* headNode, ProfileNode* parentNode) {
            return adoptRef(new ProfileNode(callIdentifier, headNode, parentNode)); }

        ProfileNode* willExecute(const CallIdentifier&);
        ProfileNode* didExecute();

        void stopProfiling();

        const CallIdentifier& callIdentifier() const { return m_callIdentifier; }
        ProfileNode* parent() const { return m_parent; }
        void setParent(ProfileNode* parent) { m_parent = parent; }
        ProfileNode* nextSibling() const { return m_nextSibling; }
        void setNextSibling(ProfileNode* nextSibling) { m_nextSibling = nextSibling; }
        UString functionName() const { return m_callIdentifier.name; }
        UString url() const { return m_callIdentifier.url; }
        unsigned lineNumber() const { return m_callIdentifier.lineNumber; }

        double startTime() const { return m_startTime; }
        void setStartTime(double startTime) { m_startTime = startTime; }
        double totalTime() const { return m_visibleTotalTime; }
        void setTotalTime(double time) { m_actualTotalTime = time; m_visibleTotalTime = time; }
        void setActualTotalTime(double time) { m_actualTotalTime = time; }
        void setVisibleTotalTime(double time) { m_visibleTotalTime = time; }
        double selfTime() const { return m_visibleSelfTime; }
        void setSelfTime(double time) {m_actualSelfTime = time; m_visibleSelfTime = time; }
        void setActualSelfTime(double time) { m_actualSelfTime = time; }
        void setVisibleSelfTime(double time) { m_visibleSelfTime = time; }
        double totalPercent() const { return (m_visibleTotalTime / (m_head ? m_head->totalTime() : totalTime())) * 100.0; }
        double selfPercent() const { return (m_visibleSelfTime / (m_head ? m_head->totalTime() : totalTime())) * 100.0; }
        unsigned numberOfCalls() const { return m_numberOfCalls; }
        void setNumberOfCalls(unsigned number) { m_numberOfCalls = number; }
        const Vector<RefPtr<ProfileNode> >& children() const { return m_children; }
        ProfileNode* firstChild() const { return m_children.size() ? m_children[0].get() : 0; }
        void addChild(PassRefPtr<ProfileNode> prpChild);
        void insertNode(PassRefPtr<ProfileNode> prpNode);
        bool visible() const { return m_visible; }
        void setVisible(bool visible) { m_visible = visible; }

        static void setTreeVisible(ProfileNode*, bool visible);

        ProfileNode* traverseNextNodePostOrder() const;
        ProfileNode* traverseNextNodePreOrder(bool processChildren = true) const;
        
        void sort(bool (*)(const RefPtr<ProfileNode>&, const RefPtr<ProfileNode>&));
        static void sortTotalTimeDescending(ProfileNode* n) { n->sort(totalTimeDescendingComparator); }
        static void sortTotalTimeAscending(ProfileNode* n) { n->sort(totalTimeAscendingComparator); }
        static void sortSelfTimeDescending(ProfileNode* n) { n->sort(selfTimeDescendingComparator); }
        static void sortSelfTimeAscending(ProfileNode* n) { n->sort(selfTimeAscendingComparator); }
        static void sortCallsDescending(ProfileNode* n) { n->sort(callsDescendingComparator); }
        static void sortCallsAscending(ProfileNode* n) { n->sort(callsAscendingComparator); }
        static void sortFunctionNameDescending(ProfileNode* n) { n->sort(functionNameDescendingComparator); }
        static void sortFunctionNameAscending(ProfileNode* n) { n->sort(functionNameAscendingComparator); }

        void calculateVisibleTotalTime();
        bool focus(const CallIdentifier&);
        void exclude(const CallIdentifier&);
        void restore();

        void endAndRecordCall();
        
#ifndef NDEBUG
        const char* toString() const { return m_callIdentifier.toString(); }
        void debugPrintData(int indentLevel) const;
        double debugPrintDataSampleStyle(int indentLevel, FunctionCallHashCount&) const;
#endif
    private:
        ProfileNode(const CallIdentifier&, ProfileNode* headNode, ProfileNode* parentNode);
        void startTimer();
        void resetChildrensSiblings();
        RefPtr<ProfileNode>* childrenBegin() { return m_children.begin(); }
        RefPtr<ProfileNode>* childrenEnd() { return m_children.end(); }

        // Sorting comparators
        static inline bool totalTimeDescendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b) { return a->totalTime() > b->totalTime(); }
        static inline bool totalTimeAscendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b) { return a->totalTime() < b->totalTime(); }
        static inline bool selfTimeDescendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b) { return a->selfTime() > b->selfTime(); }
        static inline bool selfTimeAscendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b) { return a->selfTime() < b->selfTime(); }
        static inline bool callsDescendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b) { return a->numberOfCalls() > b->numberOfCalls(); }
        static inline bool callsAscendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b) { return a->numberOfCalls() < b->numberOfCalls(); }
        static inline bool functionNameDescendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b) { return a->functionName() > b->functionName(); }
        static inline bool functionNameAscendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b) { return a->functionName() < b->functionName(); }

        CallIdentifier m_callIdentifier;
        ProfileNode* m_head;
        ProfileNode* m_parent;
        ProfileNode* m_nextSibling;

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
