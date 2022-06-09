/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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

#pragma once

#include "RenderFragmentContainer.h"

namespace WebCore {

class RenderFragmentedFlow;

// RenderFragmentContainerSet represents a set of regions that all have the same width and height. It is a "composite region box" that
// can be used to represent a single run of contiguous regions.
//
// By combining runs of same-size columns or pages into a single object, we significantly reduce the number of unique RenderObjects
// required to represent those objects.
//
// This class is abstract and is only intended for use by renderers that generate anonymous runs of identical regions, i.e.,
// columns and printing. RenderMultiColumnSet and RenderPageSet represent runs of columns and pages respectively.
//
// FIXME: For now we derive from RenderFragmentContainer, but this may change at some point.

class RenderFragmentContainerSet : public RenderFragmentContainer {
    WTF_MAKE_ISO_ALLOCATED(RenderFragmentContainerSet);
public:
    void expandToEncompassFragmentedFlowContentsIfNeeded();

protected:
    RenderFragmentContainerSet(Document&, RenderStyle&&, RenderFragmentedFlow&);

private:
    void installFragmentedFlow() final;

    const char* renderName() const override = 0;
    
    bool isRenderFragmentContainerSet() const final { return true; }
};

} // namespace WebCore
