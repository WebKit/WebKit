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

#include "config.h"
#include "ProfileNode.h"

#include "Profiler.h"
#include "DateMath.h"

#include <stdio.h>

namespace KJS {

static const char* NonJSExecution = "(idle)";

ProfileNode::ProfileNode(const CallIdentifier& callIdentifier, ProfileNode* headNode, ProfileNode* parentNode)
    : m_callIdentifier(callIdentifier)
    , m_headNode(headNode)
    , m_parentNode(parentNode)
    , m_startTime(0.0)
    , m_actualTotalTime (0.0)
    , m_visibleTotalTime (0.0)
    , m_actualSelfTime (0.0)
    , m_visibleSelfTime (0.0)
    , m_numberOfCalls(0)
    , m_visible(true)
{
    if (!m_headNode)
        m_headNode = this;

    startTimer();
}

ProfileNode* ProfileNode::willExecute(const CallIdentifier& callIdentifier)
{
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild) {
        if ((*currentChild)->callIdentifier() == callIdentifier) {
            (*currentChild)->startTimer();
            return (*currentChild).get();
        }
    }

    m_children.append(ProfileNode::create(callIdentifier, m_headNode, this));
    return m_children.last().get();
}

ProfileNode* ProfileNode::didExecute()
{
    endAndRecordCall();
    return m_parentNode;
}

void ProfileNode::addChild(PassRefPtr<ProfileNode> prpChild)
{
    RefPtr<ProfileNode> child = prpChild;
    child->setParent(this);
    m_children.append(child.release());
}
        
void ProfileNode::insertNode(PassRefPtr<ProfileNode> prpNode)
{
    RefPtr<ProfileNode> node = prpNode;

    double sumOfChildrensTime = 0.0;
    for (unsigned i = 0; i < m_children.size(); ++i) {
        sumOfChildrensTime += m_children[i]->totalTime();
        node->addChild(m_children[i].release());
    }

    m_children.clear();

    node->didExecute();
    node->setTotalTime(sumOfChildrensTime);
    node->setSelfTime(0.0);
    m_children.append(node.release());
}

void ProfileNode::stopProfiling()
{
    if (m_startTime)
        endAndRecordCall();

    m_visibleTotalTime = m_actualTotalTime;

    ASSERT(m_actualSelfTime == 0.0 && m_startTime == 0.0);

    // Calculate Self time and the percentages once we stop profiling.
    StackIterator endOfChildren = m_children.end();
    for (StackIterator currentChild = m_children.begin(); currentChild != endOfChildren; ++currentChild) {
        (*currentChild)->stopProfiling();
        m_actualSelfTime += (*currentChild)->totalTime();
    }

    ASSERT(m_actualSelfTime <= m_actualTotalTime);
    m_actualSelfTime = m_actualTotalTime - m_actualSelfTime;

    if (m_headNode == this && m_actualSelfTime) {
        ProfileNode* idleNode = willExecute(CallIdentifier(NonJSExecution, 0, 0));

        idleNode->setTotalTime(m_actualSelfTime);
        idleNode->setSelfTime(m_actualSelfTime);
        idleNode->setNumberOfCalls(0);

        m_actualSelfTime = 0.0;
    }

    m_visibleSelfTime = m_actualSelfTime;
}

// Sorting methods

static inline bool totalTimeDescendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b)
{
    return a->totalTime() > b->totalTime();
}

void ProfileNode::sortTotalTimeDescending()
{
    std::sort(m_children.begin(), m_children.end(), totalTimeDescendingComparator);

    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->sortTotalTimeDescending();
}

static inline bool totalTimeAscendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b)
{
    return a->totalTime() < b->totalTime();
}

void ProfileNode::sortTotalTimeAscending()
{
    std::sort(m_children.begin(), m_children.end(), totalTimeAscendingComparator);

    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->sortTotalTimeAscending();
}

static inline bool selfTimeDescendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b)
{
    return a->selfTime() > b->selfTime();
}

void ProfileNode::sortSelfTimeDescending()
{
    std::sort(m_children.begin(), m_children.end(), selfTimeDescendingComparator);

    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->sortSelfTimeDescending();
}

static inline bool selfTimeAscendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b)
{
    return a->selfTime() < b->selfTime();
}

void ProfileNode::sortSelfTimeAscending()
{
    std::sort(m_children.begin(), m_children.end(), selfTimeAscendingComparator);

    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->sortSelfTimeAscending();
}

static inline bool callsDescendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b)
{
    return a->numberOfCalls() > b->numberOfCalls();
}

void ProfileNode::sortCallsDescending()
{
    std::sort(m_children.begin(), m_children.end(), callsDescendingComparator);

    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->sortCallsDescending();
}

static inline bool callsAscendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b)
{
    return a->numberOfCalls() < b->numberOfCalls();
}

void ProfileNode::sortCallsAscending()
{
    std::sort(m_children.begin(), m_children.end(), callsAscendingComparator);

    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->sortCallsAscending();
}

static inline bool functionNameDescendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b)
{
    return a->functionName() > b->functionName();
}

void ProfileNode::sortFunctionNameDescending()
{
    std::sort(m_children.begin(), m_children.end(), functionNameDescendingComparator);

    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->sortFunctionNameDescending();
}

static inline bool functionNameAscendingComparator(const RefPtr<ProfileNode>& a, const RefPtr<ProfileNode>& b)
{
    return a->functionName() < b->functionName();
}

void ProfileNode::sortFunctionNameAscending()
{
    std::sort(m_children.begin(), m_children.end(), functionNameAscendingComparator);

    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->sortFunctionNameAscending();
}

void ProfileNode::setTreeVisible(bool visible)
{
    m_visible = visible;

    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->setTreeVisible(visible);
}

void ProfileNode::focus(const CallIdentifier& callIdentifier, bool forceVisible)
{
    if (m_callIdentifier == callIdentifier) {
        m_visible = true;

        for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
            (*currentChild)->focus(callIdentifier, true);
    } else {
        m_visible = forceVisible;
        double totalChildrenTime = 0.0;

        for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild) {
            (*currentChild)->focus(callIdentifier, forceVisible);
            m_visible |= (*currentChild)->visible();
            totalChildrenTime += (*currentChild)->totalTime();
        }
        
        if (m_visible)
            m_visibleTotalTime = m_visibleSelfTime + totalChildrenTime;
    }
}

double ProfileNode::exclude(const CallIdentifier& callIdentifier)
{
    if (m_callIdentifier == callIdentifier) {
        m_visible = false;

        for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
            (*currentChild)->setTreeVisible(false);

        return m_visibleTotalTime;
    }

    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        m_visibleSelfTime += (*currentChild)->exclude(callIdentifier);

    return 0;
}

void ProfileNode::restoreAll()
{
    m_visibleTotalTime = m_actualTotalTime;
    m_visibleSelfTime = m_actualSelfTime;
    m_visible = true;

    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->restoreAll();
}

void ProfileNode::endAndRecordCall()
{
    m_actualTotalTime += getCurrentUTCTimeWithMicroseconds() - m_startTime;
    m_startTime = 0.0;

    ++m_numberOfCalls;
}

void ProfileNode::startTimer()
{
    if (!m_startTime)
        m_startTime = getCurrentUTCTimeWithMicroseconds();
}

#ifndef NDEBUG
void ProfileNode::debugPrintData(int indentLevel) const
{
    // Print function names
    for (int i = 0; i < indentLevel; ++i)
        printf("  ");

    printf("%d SelfTime %.3fms/%.3f%% TotalTime %.3fms/%.3f%% VSelf %.3fms VTotal %.3fms Function Name %s Visible %s\n",
        m_numberOfCalls, m_actualSelfTime, selfPercent(), m_actualTotalTime, totalPercent(),
        m_visibleSelfTime, m_visibleTotalTime, 
        functionName().UTF8String().c_str(), (m_visible ? "True" : "False"));

    ++indentLevel;

    // Print children's names and information
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->debugPrintData(indentLevel);
}

// print the profiled data in a format that matches the tool sample's output.
double ProfileNode::debugPrintDataSampleStyle(int indentLevel, FunctionCallHashCount& countedFunctions) const
{
    printf("    ");

    // Print function names
    const char* name = functionName().UTF8String().c_str();
    double sampleCount = m_actualTotalTime * 1000;
    if (indentLevel) {
        for (int i = 0; i < indentLevel; ++i)
            printf("  ");

         countedFunctions.add(functionName().rep());

        printf("%.0f %s\n", sampleCount ? sampleCount : 1, name);
    } else
        printf("%s\n", name);

    ++indentLevel;

    // Print children's names and information
    double sumOfChildrensCount = 0.0;
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        sumOfChildrensCount += (*currentChild)->debugPrintDataSampleStyle(indentLevel, countedFunctions);

    sumOfChildrensCount *= 1000;    //
    // Print remainder of samples to match sample's output
    if (sumOfChildrensCount < sampleCount) {
        printf("    ");
        while (indentLevel--)
            printf("  ");

        printf("%.0f %s\n", sampleCount - sumOfChildrensCount, functionName().UTF8String().c_str());
    }

    return m_actualTotalTime;
}
#endif

}   // namespace KJS
