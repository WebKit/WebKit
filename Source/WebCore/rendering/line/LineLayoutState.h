/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 ChangSeok Oh <shivamidow@gmail.com>
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "LayoutRect.h"
#include "RenderBlockFlow.h"
#include <wtf/RefCounted.h>

namespace WebCore {

// Like LayoutState for layout(), LineLayoutState keeps track of global information
// during an entire linebox tree layout pass (aka layoutInlineChildren).
class LineLayoutState {
public:
    LineLayoutState(const RenderBlockFlow& blockFlow)
        : m_marginInfo(blockFlow, blockFlow.borderAndPaddingBefore(), blockFlow.borderAndPaddingAfter() + blockFlow.scrollbarLogicalHeight())
    {
    }

    LineInfo& lineInfo() { return m_lineInfo; }
    const LineInfo& lineInfo() const { return m_lineInfo; }

    RenderBlockFlow::MarginInfo& marginInfo() { return m_marginInfo; }

private:
    LineInfo m_lineInfo;

    RenderBlockFlow::MarginInfo m_marginInfo;
};

} // namespace WebCore
