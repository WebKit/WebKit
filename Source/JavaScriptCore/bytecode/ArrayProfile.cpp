/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "ArrayProfile.h"

namespace JSC {

void ArrayProfile::computeUpdatedPrediction(OperationInProgress operation)
{
    if (m_lastSeenStructure) {
        m_observedArrayModes |= arrayModeFromStructure(m_lastSeenStructure);
        if (!m_structureIsPolymorphic) {
            if (!m_expectedStructure)
                m_expectedStructure = m_lastSeenStructure;
            else if (m_expectedStructure != m_lastSeenStructure) {
                m_expectedStructure = 0;
                m_structureIsPolymorphic = true;
            }
        }
        m_lastSeenStructure = 0;
    }
    
    if (operation == Collection
        && m_expectedStructure
        && !Heap::isMarked(m_expectedStructure)) {
        m_expectedStructure = 0;
        m_structureIsPolymorphic = true;
    }
}

} // namespace JSC

