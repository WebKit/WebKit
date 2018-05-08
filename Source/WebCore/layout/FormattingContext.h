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

#include "FloatingState.h"
#include "LayoutUnit.h"
#include <wtf/IsoMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace Display {
class Box;
}

namespace Layout {

class Box;
class Container;
class FormattingState;
class LayoutContext;

class FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(FormattingContext);
public:
    FormattingContext(const Box& formattingContextRoot);
    virtual ~FormattingContext();

    virtual void layout(LayoutContext&, FormattingState&) const = 0;
    virtual std::unique_ptr<FormattingState> createFormattingState(Ref<FloatingState>&&) const = 0;
    virtual Ref<FloatingState> createOrFindFloatingState(LayoutContext&) const = 0;

protected:
    struct LayoutPair {
        const Box& layoutBox;
        Display::Box& displayBox;
    };
    using LayoutQueue = Vector<std::unique_ptr<LayoutPair>>;

    const Box& root() const { return *m_root; }

    virtual void computeStaticPosition(LayoutContext&, const Box&, Display::Box&) const;
    virtual void computeInFlowPositionedPosition(const Box&, Display::Box&) const;
    virtual void computeOutOfFlowPosition(const Box&, Display::Box&) const;

    virtual void computeWidth(const Box&, Display::Box&) const;
    virtual void computeHeight(const Box&, Display::Box&) const;

    virtual void computeOutOfFlowWidth(const Box&, Display::Box&) const;
    virtual void computeFloatingWidth(const Box&, Display::Box&) const;
    virtual void computeInFlowWidth(const Box&, Display::Box&) const = 0;

    virtual void computeOutOfFlowHeight(const Box&, Display::Box&) const;
    virtual void computeFloatingHeight(const Box&, Display::Box&) const;
    virtual void computeInFlowHeight(const Box&, Display::Box&) const = 0;

    virtual LayoutUnit marginTop(const Box&) const;
    virtual LayoutUnit marginLeft(const Box&) const;
    virtual LayoutUnit marginBottom(const Box&) const;
    virtual LayoutUnit marginRight(const Box&) const;

    void placeInFlowPositionedChildren(const Container&) const;
    void layoutOutOfFlowDescendants(LayoutContext&s) const;

private:
    WeakPtr<Box> m_root;
};

}
}
#endif
