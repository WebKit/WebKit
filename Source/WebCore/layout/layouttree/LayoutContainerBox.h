/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBox.h"
#include <wtf/IsoMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class RenderStyle;

namespace Layout {

class ContainerBox : public Box {
    WTF_MAKE_ISO_ALLOCATED(ContainerBox);
public:
    ContainerBox(std::optional<ElementAttributes>, RenderStyle&&, std::unique_ptr<RenderStyle>&& firstLineStyle = nullptr, OptionSet<BaseTypeFlag> = { ContainerBoxFlag });
    ~ContainerBox();

    const Box* firstChild() const { return m_firstChild.get(); }
    const Box* firstInFlowChild() const;
    const Box* firstInFlowOrFloatingChild() const;
    const Box* lastChild() const { return m_lastChild.get(); }
    const Box* lastInFlowChild() const;
    const Box* lastInFlowOrFloatingChild() const;

    // FIXME: This is currently needed for style updates.
    Box* firstChild() { return m_firstChild.get(); }

    bool hasChild() const { return firstChild(); }
    bool hasInFlowChild() const { return firstInFlowChild(); }
    bool hasInFlowOrFloatingChild() const { return firstInFlowOrFloatingChild(); }

    void appendChild(UniqueRef<Box>);
    void destroyChildren();

private:
    CheckedPtr<Box> m_firstChild;
    CheckedPtr<Box> m_lastChild;
};

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_BOX(ContainerBox, isContainerBox())

#endif
