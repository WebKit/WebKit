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
#include "FunctionCallProfile.h"

#include "Profiler.h"
#include "DateMath.h"

#include <stdio.h>

namespace KJS {

FunctionCallProfile::FunctionCallProfile(const UString& name)
    : m_functionName(name)
    , m_timeSum(0)
    , m_numberOfCalls(0)
{
    m_startTime = getCurrentUTCTime();
}

FunctionCallProfile::~FunctionCallProfile()
{
    deleteAllValues(m_children);
}


void FunctionCallProfile::willExecute()
{
    m_startTime = getCurrentUTCTime();
}

void FunctionCallProfile::didExecute(Vector<UString> stackNames, unsigned int stackIndex)
{
    if (stackIndex && stackIndex == stackNames.size()) {
        ASSERT(stackNames[stackIndex - 1] == m_functionName);
        endAndRecordCall();
        return;
    }

    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end() && stackIndex < stackNames.size(); ++currentChild) {
        if ((*currentChild)->functionName() == stackNames[stackIndex]) {
            (*currentChild)->didExecute(stackNames, ++stackIndex);
            return;
        }
    }
}

void FunctionCallProfile::addChild(FunctionCallProfile* child)
{
    if (!child)
        return;

    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild) {
        if ((*currentChild)->functionName() == child->functionName())
            return;
    }

    m_children.append(child);
}

FunctionCallProfile* FunctionCallProfile::findChild(const UString& name)
{
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild) {
        if ((*currentChild)->functionName() == name)
            return *currentChild;
    }

    return 0;
}

void FunctionCallProfile::stopProfiling()
{
    if (m_startTime)
        endAndRecordCall();

    StackIterator endOfChildren = m_children.end();
    for (StackIterator it = m_children.begin(); it != endOfChildren; ++it)
        (*it)->stopProfiling();
}

void FunctionCallProfile::printDataInspectorStyle(int indentLevel) const
{
    // Print function names
    if (indentLevel) {
        for (int i = 0; i < indentLevel; ++i)
            printf("  ");

        printf("%.0fms %s\n", m_timeSum, m_functionName.UTF8String().c_str());
    } else
        printf("%s\n", m_functionName.UTF8String().c_str());

    ++indentLevel;

    // Print children's names and information
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->printDataInspectorStyle(indentLevel);
}

// print the profiled data in a format that matches the tool sample's output.
double FunctionCallProfile::printDataSampleStyle(int indentLevel) const
{
    printf("    ");

    // Print function names
    if (indentLevel) {
        for (int i = 0; i < indentLevel; ++i)
            printf("  ");

        // We've previously asserted that m_timeSum will always be >= 1
        printf("%.0f %s\n", m_timeSum ? m_timeSum : 1, m_functionName.UTF8String().c_str());
    } else
        printf("%s\n", m_functionName.UTF8String().c_str());

    ++indentLevel;

    // Print children's names and information
    double sumOfChildrensTimes = 0.0;
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        sumOfChildrensTimes += (*currentChild)->printDataSampleStyle(indentLevel);

    // Print remainder of time to match sample's output
    if (sumOfChildrensTimes < m_timeSum) {
        printf("    ");
        while (indentLevel--)
            printf("  ");

        printf("%f %s\n", m_timeSum - sumOfChildrensTimes, m_functionName.UTF8String().c_str());
    }

    return m_timeSum;
}

void FunctionCallProfile::endAndRecordCall()
{
    m_timeSum += getCurrentUTCTime() - m_startTime;
    m_startTime = 0.0;

    ++m_numberOfCalls;
}

}   // namespace KJS
