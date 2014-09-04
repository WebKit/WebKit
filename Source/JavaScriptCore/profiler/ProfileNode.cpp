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

#include "config.h"
#include "ProfileNode.h"

#include "LegacyProfiler.h"
#include <wtf/DateMath.h>
#include <wtf/DataLog.h>
#include <wtf/text/StringHash.h>

using namespace WTF;

namespace JSC {

ProfileNode::ProfileNode(ExecState* callerCallFrame, const CallIdentifier& callIdentifier, ProfileNode* parentNode)
    : m_callerCallFrame(callerCallFrame)
    , m_callIdentifier(callIdentifier)
    , m_parent(parentNode)
#ifndef NDEBUG
    , m_nextSibling(nullptr)
#endif
{
}

ProfileNode::ProfileNode(ExecState* callerCallFrame, ProfileNode* nodeToCopy)
    : m_callerCallFrame(callerCallFrame)
    , m_callIdentifier(nodeToCopy->callIdentifier())
    , m_parent(nodeToCopy->parent())
    , m_calls(nodeToCopy->calls())
#ifndef NDEBUG
    , m_nextSibling(nullptr)
#endif
{
}

void ProfileNode::addChild(PassRefPtr<ProfileNode> prpChild)
{
    RefPtr<ProfileNode> child = prpChild;
    child->setParent(this);
#ifndef NDEBUG
    if (m_children.size())
        m_children.last()->setNextSibling(child.get());
#endif
    m_children.append(child.release());
}

void ProfileNode::removeChild(ProfileNode* node)
{
    if (!node)
        return;

    for (size_t i = 0; i < m_children.size(); ++i) {
        if (*node == m_children[i].get()) {
            m_children.remove(i);
            break;
        }
    }

#ifndef NDEBUG
    size_t size = m_children.size();
    for (size_t i = 0; i < size; ++i)
        m_children[i]->setNextSibling(i + 1 == size ? nullptr : m_children[i + 1].get());
#endif
}

void ProfileNode::spliceNode(PassRefPtr<ProfileNode> prpNode)
{
    RefPtr<ProfileNode> node = prpNode;

    for (unsigned i = 0; i < m_children.size(); ++i)
        node->addChild(m_children[i].release());

    m_children.clear();
    m_children.append(node.release());
}

#ifndef NDEBUG
ProfileNode* ProfileNode::traverseNextNodePostOrder() const
{
    ProfileNode* next = m_nextSibling;
    if (!next)
        return m_parent;
    while (ProfileNode* firstChild = next->firstChild())
        next = firstChild;
    return next;
}

void ProfileNode::debugPrint()
{
    CalculateProfileSubtreeDataFunctor functor;
    forEachNodePostorder(functor);
    ProfileNode::ProfileSubtreeData data = functor.returnValue();

    debugPrintRecursively(0, data);
}

void ProfileNode::debugPrintSampleStyle()
{
    FunctionCallHashCount countedFunctions;

    CalculateProfileSubtreeDataFunctor functor;
    forEachNodePostorder(functor);
    ProfileNode::ProfileSubtreeData data = functor.returnValue();

    debugPrintSampleStyleRecursively(0, countedFunctions, data);
}

void ProfileNode::debugPrintRecursively(int indentLevel, const ProfileSubtreeData& data)
{
    // Print function names
    for (int i = 0; i < indentLevel; ++i)
        dataLogF("  ");

    auto it = data.selfAndTotalTimes.find(this);
    ASSERT(it != data.selfAndTotalTimes.end());

    double nodeSelfTime = it->value.first;
    double nodeTotalTime = it->value.second;
    double rootTotalTime = data.rootTotalTime;

    dataLogF("Function Name %s %zu SelfTime %.3fms/%.3f%% TotalTime %.3fms/%.3f%% Next Sibling %s\n",
        functionName().utf8().data(),
        m_calls.size(), nodeSelfTime, nodeSelfTime / rootTotalTime * 100.0, nodeTotalTime, nodeTotalTime / rootTotalTime * 100.0,
        m_nextSibling ? m_nextSibling->functionName().utf8().data() : "");

    ++indentLevel;

    // Print children's names and information
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->debugPrintRecursively(indentLevel, data);
}

// print the profiled data in a format that matches the tool sample's output.
double ProfileNode::debugPrintSampleStyleRecursively(int indentLevel, FunctionCallHashCount& countedFunctions, const ProfileSubtreeData& data)
{
    dataLogF("    ");

    auto it = data.selfAndTotalTimes.find(this);
    ASSERT(it != data.selfAndTotalTimes.end());
    double nodeTotalTime = it->value.second;

    // Print function names
    const char* name = functionName().utf8().data();
    double sampleCount = nodeTotalTime * 1000;
    if (indentLevel) {
        for (int i = 0; i < indentLevel; ++i)
            dataLogF("  ");

         countedFunctions.add(functionName().impl());

        dataLogF("%.0f %s\n", sampleCount ? sampleCount : 1, name);
    } else
        dataLogF("%s\n", name);

    ++indentLevel;

    // Print children's names and information
    double sumOfChildrensCount = 0.0;
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        sumOfChildrensCount += (*currentChild)->debugPrintSampleStyleRecursively(indentLevel, countedFunctions, data);

    sumOfChildrensCount *= 1000;    //
    // Print remainder of samples to match sample's output
    if (sumOfChildrensCount < sampleCount) {
        dataLogF("    ");
        while (indentLevel--)
            dataLogF("  ");

        dataLogF("%.0f %s\n", sampleCount - sumOfChildrensCount, functionName().utf8().data());
    }

    return nodeTotalTime;
}
#endif

} // namespace JSC
