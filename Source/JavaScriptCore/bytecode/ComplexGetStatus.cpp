/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "ComplexGetStatus.h"

#include "JSCInlines.h"

namespace JSC {

ComplexGetStatus ComplexGetStatus::computeFor(
    CodeBlock* profiledBlock, Structure* headStructure, StructureChain* chain,
    unsigned chainCount, StringImpl* uid)
{
    // FIXME: We should assert that we never see a structure that
    // hasImpureGetOwnPropertySlot() but for which we don't
    // newImpurePropertyFiresWatchpoints(). We're not at a point where we can do
    // that, yet.
    // https://bugs.webkit.org/show_bug.cgi?id=131810
    
    if (headStructure->takesSlowPathInDFGForImpureProperty())
        return takesSlowPath();
    
    ComplexGetStatus result;
    result.m_kind = Inlineable;
    
    if (chain) {
        result.m_chain = adoptRef(new IntendedStructureChain(
            profiledBlock, headStructure, chain, chainCount));
        
        if (!result.m_chain->isStillValid())
            return skip();
        
        if (headStructure->takesSlowPathInDFGForImpureProperty()
            || result.m_chain->takesSlowPathInDFGForImpureProperty())
            return takesSlowPath();
        
        JSObject* currentObject = result.m_chain->terminalPrototype();
        Structure* currentStructure = result.m_chain->last();
        
        ASSERT_UNUSED(currentObject, currentObject);
        
        result.m_offset = currentStructure->getConcurrently(
            *profiledBlock->vm(), uid, result.m_attributes);
    } else {
        result.m_offset = headStructure->getConcurrently(
            *profiledBlock->vm(), uid, result.m_attributes);
    }
    
    if (!isValidOffset(result.m_offset))
        return takesSlowPath();
    
    return result;
}

} // namespace JSC


