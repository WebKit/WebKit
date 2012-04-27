/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(CSS_SHADERS)
#include "CustomFilterOperation.h"

#include "CustomFilterParameter.h"
#include "CustomFilterProgram.h"
#include "FilterOperation.h"

#include <wtf/text/StringHash.h>

namespace WebCore {

bool customFilterParametersEqual(const CustomFilterParameterList& listA, const CustomFilterParameterList& listB)
{
    if (listA.size() != listB.size())
        return false;
    for (size_t i = 0; i < listA.size(); ++i) {
        if (listA.at(i).get() != listB.at(i).get() 
            && *listA.at(i).get() != *listB.at(i).get())
            return false;
    }
    return true;
}

#if !ASSERT_DISABLED
static bool checkCustomFilterParametersOrder(const CustomFilterParameterList& parameters)
{
    for (unsigned i = 1; i < parameters.size(); ++i) {
        // Break for equal or not-sorted parameters.
        if (!codePointCompareLessThan(parameters.at(i - 1)->name(), parameters.at(i)->name()))
            return false;
    }
    return true;
}
#endif

void blendCustomFilterParameters(const CustomFilterParameterList& fromList, const CustomFilterParameterList& toList, double progress, CustomFilterParameterList& resultList)
{
    // This method expects both lists to be sorted by parameter name and the result list is also sorted.
    ASSERT(checkCustomFilterParametersOrder(fromList));
    ASSERT(checkCustomFilterParametersOrder(toList));
    size_t fromListIndex = 0, toListIndex = 0;
    while (fromListIndex < fromList.size() && toListIndex < toList.size()) {
        CustomFilterParameter* paramFrom = fromList.at(fromListIndex).get();
        CustomFilterParameter* paramTo = toList.at(toListIndex).get();
        if (paramFrom->name() == paramTo->name()) {
            resultList.append(paramTo->blend(paramFrom, progress));
            ++fromListIndex;
            ++toListIndex;
            continue;
        }
        if (codePointCompareLessThan(paramFrom->name(), paramTo->name())) {
            resultList.append(paramFrom);
            ++fromListIndex;
            continue;
        }
        resultList.append(paramTo);
        ++toListIndex;
    }
    for (; fromListIndex < fromList.size(); ++fromListIndex)
        resultList.append(fromList.at(fromListIndex));
    for (; toListIndex < toList.size(); ++toListIndex)
        resultList.append(toList.at(toListIndex));
    ASSERT(checkCustomFilterParametersOrder(resultList));
}

CustomFilterOperation::CustomFilterOperation(PassRefPtr<CustomFilterProgram> program, const CustomFilterParameterList& sortedParameters, unsigned meshRows, unsigned meshColumns, MeshBoxType meshBoxType, MeshType meshType)
    : FilterOperation(CUSTOM)
    , m_program(program)
    , m_parameters(sortedParameters)
    , m_meshRows(meshRows)
    , m_meshColumns(meshColumns)
    , m_meshBoxType(meshBoxType)
    , m_meshType(meshType)
{
    // Make sure that the parameters are alwyas sorted by name. We use that to merge two CustomFilterOperations in animations.
    ASSERT(checkCustomFilterParametersOrder(m_parameters));
}
    
CustomFilterOperation::~CustomFilterOperation()
{
}

PassRefPtr<FilterOperation> CustomFilterOperation::blend(const FilterOperation* from, double progress, bool blendToPassthrough)
{
    // FIXME: There's no way to decide what is the "passthrough filter" for shaders using the current CSS Syntax.
    // https://bugs.webkit.org/show_bug.cgi?id=84903
    // https://www.w3.org/Bugs/Public/show_bug.cgi?id=16861
    if (blendToPassthrough || !from || !from->isSameType(*this))
        return this;
    
    const CustomFilterOperation* fromOp = static_cast<const CustomFilterOperation*>(from);
    if (*m_program.get() != *fromOp->m_program.get()
        || m_meshRows != fromOp->m_meshRows
        || m_meshColumns != fromOp->m_meshColumns
        || m_meshBoxType != fromOp->m_meshBoxType
        || m_meshType != fromOp->m_meshType)
        return this;
    
    CustomFilterParameterList animatedParameters;
    blendCustomFilterParameters(fromOp->m_parameters, m_parameters, progress, animatedParameters);
    return CustomFilterOperation::create(m_program, animatedParameters, m_meshRows, m_meshColumns, m_meshBoxType, m_meshType);
}

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS)
