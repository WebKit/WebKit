/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WHLSLEntryPointScaffolding.h"

#if ENABLE(WEBGPU)

#include "WHLSLGatherEntryPointItems.h"

namespace WebCore {

namespace WHLSL {

namespace Metal {

EntryPointScaffolding::EntryPointScaffolding(AST::FunctionDefinition& functionDefinition, Intrinsics& intrinsics)
    : m_functionDefinition(&functionDefinition)
    , m_intrinsics(&intrinsics)
{
    // FIXME: Implement this.
    gatherEntryPointItems(*m_intrinsics, *m_functionDefinition);
}

String EntryPointScaffolding::helperTypes()
{
    // FIXME: Implement this.
    return String();
}

String EntryPointScaffolding::signature()
{
    // FIXME: Implement this.
    return String();
}

String EntryPointScaffolding::unpack()
{
    // FIXME: Implement this.
    return String();
}

String EntryPointScaffolding::pack(const String&, const String&)
{
    // FIXME: Implement this.
    return String();
}

}

}

}

#endif
