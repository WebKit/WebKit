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

#include "config.h"
#include "Profile.h"

#include "ProfileNode.h"
#include "JSGlobalObject.h"
#include "ExecState.h"
#include "JSFunction.h"

#include <stdio.h>

namespace KJS {

static const char* NonJSExecution = "(idle)";

static void calculateVisibleTotalTime(ProfileNode* n) { n->calculateVisibleTotalTime(); }
static void restoreAll(ProfileNode* n) { n->restore(); }
static void stopProfiling(ProfileNode* n) { n->stopProfiling(); }

PassRefPtr<Profile> Profile::create(const UString& title, ExecState* originatingGlobalExec, unsigned pageGroupIdentifier, ProfilerClient* client)
{
    return adoptRef(new Profile(title, originatingGlobalExec, pageGroupIdentifier, client));
}

Profile::Profile(const UString& title, ExecState* originatingGlobalExec, unsigned pageGroupIdentifier, ProfilerClient* client)
    : m_title(title)
    , m_originatingGlobalExec(originatingGlobalExec)
    , m_pageGroupIdentifier(pageGroupIdentifier)
    , m_client(client)
    , m_stoppedProfiling(false)
{
    // FIXME: When multi-threading is supported this will be a vector and calls
    // into the profiler will need to know which thread it is executing on.
    m_head = ProfileNode::create(CallIdentifier("Thread_1", 0, 0), 0, 0);
    m_currentNode = m_head;
}

void Profile::stopProfiling()
{
    forEach(KJS::stopProfiling);
    removeProfileStart();
    removeProfileEnd();

    // Already at the head, set m_currentNode to prevent
    // didExecute from recording more nodes.
    if (m_currentNode == m_head)
        m_currentNode = 0;

    m_stoppedProfiling = true;
}

bool Profile::didFinishAllExecution()
{
    if (!m_stoppedProfiling)
        return false;

    if (double headSelfTime = m_head->selfTime()) {
        RefPtr<ProfileNode> idleNode = ProfileNode::create(CallIdentifier(NonJSExecution, 0, 0), m_head.get(), m_head.get());

        idleNode->setTotalTime(headSelfTime);
        idleNode->setSelfTime(headSelfTime);
        idleNode->setVisible(true);

        m_head->setSelfTime(0.0);
        m_head->addChild(idleNode.release());
    }

    m_currentNode = 0;
    m_originatingGlobalExec = 0;
    return true;
}

// The console.profile that started this profile will be the first child.
void Profile::removeProfileStart() {
    ProfileNode* currentNode = 0;
    for (ProfileNode* next = m_head.get(); next; next = next->firstChild())
        currentNode = next;

    if (currentNode->callIdentifier().name != "profile")
        return;

    for (ProfileNode* currentParent = currentNode->parent(); currentParent; currentParent = currentParent->parent())
        currentParent->setTotalTime(currentParent->totalTime() - currentNode->totalTime());

    currentNode->parent()->removeChild(0);
}

// The console.profileEnd that stopped this profile will be the last child.
void Profile::removeProfileEnd() {
    ProfileNode* currentNode = 0;
    for (ProfileNode* next = m_head.get(); next; next = next->lastChild())
        currentNode = next;

    if (currentNode->callIdentifier().name != "profileEnd")
        return;

    for (ProfileNode* currentParent = currentNode->parent(); currentParent; currentParent = currentParent->parent())
        currentParent->setTotalTime(currentParent->totalTime() - currentNode->totalTime());

    currentNode->parent()->removeChild(currentNode->parent()->children().size() - 1);
}

void Profile::willExecute(const CallIdentifier& callIdentifier)
{
    if (m_stoppedProfiling)
        return;

    ASSERT(m_currentNode);
    m_currentNode = m_currentNode->willExecute(callIdentifier);
}

void Profile::didExecute(const CallIdentifier& callIdentifier)
{
    if (!m_currentNode)
        return;

    if (m_currentNode == m_head) {
        m_currentNode = ProfileNode::create(callIdentifier, m_head.get(), m_head.get());
        m_currentNode->setStartTime(m_head->startTime());
        m_currentNode->didExecute();
        m_head->insertNode(m_currentNode.get());

        if (m_stoppedProfiling) {
            m_currentNode->setTotalTime(m_currentNode->totalTime() + m_head->selfTime());
            m_head->setSelfTime(0.0);
            m_currentNode->stopProfiling();
            m_currentNode = 0;
        } else
            m_currentNode = m_head;

        return;
    }

    if (m_stoppedProfiling) {
        m_currentNode = m_currentNode->parent();
        return;
    }

    m_currentNode = m_currentNode->didExecute();
}

void Profile::forEach(UnaryFunction function)
{
    ProfileNode* currentNode = m_head->firstChild();
    for (ProfileNode* nextNode = currentNode; nextNode; nextNode = nextNode->firstChild())
        currentNode = nextNode;

    ProfileNode* endNode = m_head->traverseNextNodePostOrder();
    while (currentNode && currentNode != endNode) {
        function(currentNode);
        currentNode = currentNode->traverseNextNodePostOrder();
    } 
}

void Profile::focus(const ProfileNode* profileNode)
{
    if (!profileNode || !m_head)
        return;

    bool processChildren;
    const CallIdentifier& callIdentifier = profileNode->callIdentifier();
    for (ProfileNode* currentNode = m_head.get(); currentNode; currentNode = currentNode->traverseNextNodePreOrder(processChildren))
        processChildren = currentNode->focus(callIdentifier);

    // Set the visible time of all nodes so that the %s display correctly.
    forEach(KJS::calculateVisibleTotalTime);
}

void Profile::exclude(const ProfileNode* profileNode)
{
    if (!profileNode || !m_head)
        return;

    const CallIdentifier& callIdentifier = profileNode->callIdentifier();

    for (ProfileNode* currentNode = m_head.get(); currentNode; currentNode = currentNode->traverseNextNodePreOrder())
        currentNode->exclude(callIdentifier);

    // Set the visible time of the head so the %s display correctly.
    m_head->setVisibleTotalTime(m_head->totalTime() - m_head->selfTime());
    m_head->setVisibleSelfTime(0.0);
}

void Profile::restoreAll()
{
    forEach(KJS::restoreAll);
}

#ifndef NDEBUG
void Profile::debugPrintData() const
{
    printf("Call graph:\n");
    m_head->debugPrintData(0);
}

typedef pair<UString::Rep*, unsigned> NameCountPair;

static inline bool functionNameCountPairComparator(const NameCountPair& a, const NameCountPair& b)
{
    return a.second > b.second;
}

void Profile::debugPrintDataSampleStyle() const
{
    typedef Vector<NameCountPair> NameCountPairVector;

    FunctionCallHashCount countedFunctions;
    printf("Call graph:\n");
    m_head->debugPrintDataSampleStyle(0, countedFunctions);

    printf("\nTotal number in stack:\n");
    NameCountPairVector sortedFunctions(countedFunctions.size());
    copyToVector(countedFunctions, sortedFunctions);

    std::sort(sortedFunctions.begin(), sortedFunctions.end(), functionNameCountPairComparator);
    for (NameCountPairVector::iterator it = sortedFunctions.begin(); it != sortedFunctions.end(); ++it)
        printf("        %-12d%s\n", (*it).second, UString((*it).first).UTF8String().c_str());

    printf("\nSort by top of stack, same collapsed (when >= 5):\n");
}
#endif

}   // namespace KJS
