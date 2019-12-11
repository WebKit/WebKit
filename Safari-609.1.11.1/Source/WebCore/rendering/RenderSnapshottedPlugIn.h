/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#include "RenderEmbeddedObject.h"

namespace WebCore {

class HTMLPlugInImageElement;
class RenderImageResource;

class RenderSnapshottedPlugIn final : public RenderEmbeddedObject {
    WTF_MAKE_ISO_ALLOCATED(RenderSnapshottedPlugIn);
public:
    RenderSnapshottedPlugIn(HTMLPlugInImageElement&, RenderStyle&&);
    virtual ~RenderSnapshottedPlugIn();

    HTMLPlugInImageElement& plugInImageElement() const;
    void updateSnapshot(Image*);
    void handleEvent(Event&);

private:
    void willBeDestroyed() override;
    void frameOwnerElement() const = delete;
    const char* renderName() const final { return "RenderSnapshottedPlugIn"; }
    CursorDirective getCursor(const LayoutPoint&, Cursor&) const final;
    bool isSnapshottedPlugIn() const final { return true; }
    void paint(PaintInfo&, const LayoutPoint&) final;
    bool canHaveWidget() const final { return false; }
    void paintSnapshot(PaintInfo&, const LayoutPoint&);
    void layout() final;

    std::unique_ptr<RenderImageResource> m_snapshotResource;
    bool m_isPotentialMouseActivation { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSnapshottedPlugIn, isSnapshottedPlugIn())
