/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "RenderViewTransitionRoot.h"

#include "RenderBoxModelObjectInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderViewTransitionRoot);

RenderViewTransitionRoot::RenderViewTransitionRoot(Document& document, RenderStyle&& style)
    : RenderBlockFlow(RenderObject::Type::ViewTransitionRoot, document, WTFMove(style), RenderObject::BlockFlowFlag::IsViewTransitionContainer)
{
}

RenderViewTransitionRoot::~RenderViewTransitionRoot() = default;

void RenderViewTransitionRoot::addChildGroup(const AtomString& name, RenderBlockFlow& group)
{
    m_groups.add(name, &group);
}

void RenderViewTransitionRoot::removeChildGroup(const AtomString& name)
{
    m_groups.remove(name);
}

RenderBlockFlow* RenderViewTransitionRoot::childGroupForName(const AtomString& name)
{
    return m_groups.get(name).get();
}

} // namespace WebCore
