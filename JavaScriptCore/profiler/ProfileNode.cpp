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

ProfileNode::ProfileNode(const CallIdentifier& callIdentifier)
    : m_callIdentifier(callIdentifier)
    , m_totalTime (0.0)
    , m_selfTime (0.0)
    , m_totalPercent (0.0)
    , m_selfPercent (0.0)
    , m_numberOfCalls(0)
{
    m_startTime = getCurrentUTCTimeWithMicroseconds();
}

void ProfileNode::willExecute()
{
    if (!m_startTime)
        m_startTime = getCurrentUTCTimeWithMicroseconds();
}

// We start at the end of stackNames and work our way forwards since the names are in 
// the reverse order of how the ProfileNode tree is created (e.g. the leaf node is at
// index 0 and the top of the stack is at index stackNames.size() - 1)
void ProfileNode::didExecute(const Vector<CallIdentifier>& stackNames, unsigned int stackIndex)
{
    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_children[i]->callIdentifier() == stackNames[stackIndex]) {
            if (stackIndex)   // We are not at the bottom of the stack yet
                m_children[i]->didExecute(stackNames, stackIndex - 1);
            else    // This is the child we were looking for
                m_children[i]->endAndRecordCall();
            return;
        }
    }
}

void ProfileNode::addChild(PassRefPtr<ProfileNode> prpChild)
{
    ASSERT(prpChild);

    RefPtr<ProfileNode> child = prpChild;
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild) {
        if ((*currentChild)->functionName() == child->functionName())
            return;
    }

    m_children.append(child.release());
}

ProfileNode* ProfileNode::findChild(const CallIdentifier& functionName)
{
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild) {
        if ((*currentChild)->callIdentifier() == functionName)
            return (*currentChild).get();
    }

    return 0;
}

void ProfileNode::stopProfiling(double totalProfileTime, bool headProfileNode)
{
    if (m_startTime)
        endAndRecordCall();

    ASSERT(m_selfTime == 0.0);

    if (headProfileNode)
        totalProfileTime = m_totalTime;

    // Calculate Self time and the percentages once we stop profiling.
    StackIterator endOfChildren = m_children.end();
    for (StackIterator currentChild = m_children.begin(); currentChild != endOfChildren; ++currentChild) {
        (*currentChild)->stopProfiling(totalProfileTime);
        m_selfTime += (*currentChild)->totalTime();
    }

    ASSERT(m_selfTime <= m_totalTime);
    m_selfTime = m_totalTime - m_selfTime;

    m_totalPercent = (m_totalTime / totalProfileTime) * 100.0;
    m_selfPercent = (m_selfTime / totalProfileTime) * 100.0;
}

#pragma mark Sorting methods

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

void ProfileNode::endAndRecordCall()
{
    m_totalTime += getCurrentUTCTimeWithMicroseconds() - m_startTime;
    m_startTime = 0.0;

    ++m_numberOfCalls;
}

void ProfileNode::printDataInspectorStyle(int indentLevel) const
{
    // Print function names
    for (int i = 0; i < indentLevel; ++i)
        printf("  ");

    printf("%d SelfTime %.3fms/%.3f%% TotalTime %.3fms/%.3f%% Full Name %s\n", m_numberOfCalls, m_selfTime, selfPercent(), m_totalTime, totalPercent(), functionName().UTF8String().c_str());

    ++indentLevel;

    // Print children's names and information
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->printDataInspectorStyle(indentLevel);
}

// print the profiled data in a format that matches the tool sample's output.
double ProfileNode::printDataSampleStyle(int indentLevel, FunctionCallHashCount& countedFunctions) const
{
    printf("    ");

    // Print function names
    const char* name = functionName().UTF8String().c_str();
    double sampleCount = m_totalTime * 1000;
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
        sumOfChildrensCount += (*currentChild)->printDataSampleStyle(indentLevel, countedFunctions);

    sumOfChildrensCount *= 1000;    //
    // Print remainder of samples to match sample's output
    if (sumOfChildrensCount < sampleCount) {
        printf("    ");
        while (indentLevel--)
            printf("  ");

        printf("%.0f %s\n", sampleCount - sumOfChildrensCount, functionName().UTF8String().c_str());
    }

    return m_totalTime;
}

}   // namespace KJS
