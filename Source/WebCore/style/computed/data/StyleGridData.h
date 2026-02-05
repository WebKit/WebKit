/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include <WebCore/StyleFlowTolerance.h>
#include <WebCore/StyleGridAutoFlow.h>
#include <WebCore/StyleGridTemplateAreas.h>
#include <WebCore/StyleGridTemplateList.h>
#include <WebCore/StyleGridTrackSizes.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {
namespace Style {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(GridData);
class GridData : public RefCounted<GridData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(GridData, GridData);
public:
    static Ref<GridData> create() { return adoptRef(*new GridData); }
    Ref<GridData> copy() const;

    bool operator==(const GridData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const GridData&) const;
#endif

    GridAutoFlow gridAutoFlow;
    GridTrackSizes gridAutoColumns;
    GridTrackSizes gridAutoRows;
    GridTemplateAreas gridTemplateAreas;
    GridTemplateList gridTemplateColumns;
    GridTemplateList gridTemplateRows;
    FlowTolerance flowTolerance;

private:
    GridData();
    GridData(const GridData&);
};

} // namespace Style
} // namespace WebCore

