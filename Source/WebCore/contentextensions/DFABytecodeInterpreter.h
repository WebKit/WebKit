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

namespace WebCore::ContentExtensions {

class DFABytecodeInterpreter {
public:
    DFABytecodeInterpreter(Span<const uint8_t> bytecode)
        : m_bytecode(bytecode) { }

    using Actions = HashSet<uint64_t, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>>;

    WEBCORE_EXPORT Actions interpret(const String&, ResourceFlags);
    Actions actionsMatchingEverything();

private:
    void interpretAppendAction(unsigned& programCounter, Actions&);
    void interpretTestFlagsAndAppendAction(unsigned& programCounter, ResourceFlags, Actions&);

    template<bool caseSensitive>
    void interpetJumpTable(Span<const char> url, uint32_t& urlIndex, uint32_t& programCounter);

    const Span<const uint8_t> m_bytecode;
};

} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
