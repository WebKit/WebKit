/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "FrameIndexedDBAgent.h"

#include "Document.h"
#include "DocumentSecurityOrigin.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "SecurityOrigin.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameIndexedDBAgent);

FrameIndexedDBAgent::FrameIndexedDBAgent(FrameAgentContext& context)
    : InspectorIndexedDBAgent(context)
    , m_inspectedFrame(context.inspectedFrame)
{
}

FrameIndexedDBAgent::~FrameIndexedDBAgent() = default;

Inspector::Protocol::ErrorStringOr<Ref<LocalFrame>> FrameIndexedDBAgent::frameForSecurityOrigin(const String& securityOrigin)
{
    Ref frame = m_inspectedFrame.get();
    RefPtr document = frame->document();
    if (!document)
        return makeUnexpected("Missing document for given frame"_s);

    if (protect(document->securityOrigin())->toRawString() != securityOrigin)
        return makeUnexpected("Frame target's document does not have given securityOrigin"_s);

    return frame;
}

} // namespace WebCore
