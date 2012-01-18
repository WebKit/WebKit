/*
 * Copyright 2012 Adobe Systems Incorporated. All Rights Reserved.
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
    ASSERT(hasSortedParameterList());
}
    
CustomFilterOperation::~CustomFilterOperation()
{
}

#ifndef NDEBUG
bool CustomFilterOperation::hasSortedParameterList()
{
    for (unsigned i = 1; i < m_parameters.size(); ++i) {
        // Break for equal or not-sorted parameters.
        if (!codePointCompareLessThan(m_parameters.at(i - i)->name(), m_parameters.at(i)->name()))
            return false;
    }
    return true;
}
#endif

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS)
