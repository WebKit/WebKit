/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(CONTENT_EXTENSIONS)

#include "ContentExtensionRule.h"
#include "ContentExtensionsDebugging.h"
#include "DFABytecode.h"
#include <wtf/DataLog.h>
#include <wtf/HashSet.h>

namespace WebCore {
    
namespace ContentExtensions {

class WEBCORE_EXPORT DFABytecodeInterpreter {
public:
    DFABytecodeInterpreter(const DFABytecode* bytecode, unsigned bytecodeLength)
        : m_bytecode(bytecode)
        , m_bytecodeLength(bytecodeLength)
    {
    }
    
    typedef HashSet<uint64_t, DefaultHash<uint64_t>::Hash, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> Actions;
    
    Actions interpret(const CString&, uint16_t flags);
    Actions interpretWithConditions(const CString&, uint16_t flags, const DFABytecodeInterpreter::Actions& conditionActions);
    Actions actionsMatchingEverything();

private:
    void interpretAppendAction(unsigned& programCounter, Actions&, bool ifCondition);
    void interpretTestFlagsAndAppendAction(unsigned& programCounter, uint16_t flags, Actions&, bool ifCondition);

    template<bool caseSensitive>
    void interpetJumpTable(const char* url, uint32_t& urlIndex, uint32_t& programCounter, bool& urlIndexIsAfterEndOfString);

    const DFABytecode* m_bytecode;
    const unsigned m_bytecodeLength;
    const DFABytecodeInterpreter::Actions* m_topURLActions { nullptr };
};

} // namespace ContentExtensions    
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
