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

void FunctionCallProfile::addChild(PassRefPtr<FunctionCallProfile> prpChild)
{
    ASSERT(prpChild);

    RefPtr<FunctionCallProfile> child = prpChild;
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild) {
        if ((*currentChild)->functionName() == child->functionName())
            return;
    }

    m_children.append(child.release());
}

FunctionCallProfile* FunctionCallProfile::findChild(const UString& name)
{
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild) {
        if ((*currentChild)->functionName() == name)
            return (*currentChild).get();
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

        printf("%.3fms %s\n", m_timeSum, m_functionName.UTF8String().c_str());
    } else
        printf("%s\n", m_functionName.UTF8String().c_str());

    ++indentLevel;

    // Print children's names and information
    for (StackIterator currentChild = m_children.begin(); currentChild != m_children.end(); ++currentChild)
        (*currentChild)->printDataInspectorStyle(indentLevel);
}

// print the profiled data in a format that matches the tool sample's output.
double FunctionCallProfile::printDataSampleStyle(int indentLevel, FunctionCallHashCount& countedFunctions) const
{
    printf("    ");

    // Print function names
    const char* name = m_functionName.UTF8String().c_str();
    double sampleCount = m_timeSum * 1000;
    if (indentLevel) {
        for (int i = 0; i < indentLevel; ++i)
            printf("  ");

         countedFunctions.add(m_functionName.rep());

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

        printf("%.0f %s\n", sampleCount - sumOfChildrensCount, m_functionName.UTF8String().c_str());
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
