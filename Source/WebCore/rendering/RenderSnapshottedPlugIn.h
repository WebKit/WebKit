/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef RenderSnapshottedPlugIn_h
#define RenderSnapshottedPlugIn_h

#include "RenderEmbeddedObject.h"
#include "RenderImageResource.h"
#include "Timer.h"

namespace WebCore {

class HTMLPlugInImageElement;

class RenderSnapshottedPlugIn final : public RenderEmbeddedObject {
public:
    RenderSnapshottedPlugIn(HTMLPlugInImageElement&, PassRef<RenderStyle>);
    virtual ~RenderSnapshottedPlugIn();

    HTMLPlugInImageElement& plugInImageElement() const;

    void updateSnapshot(PassRefPtr<Image>);

    void handleEvent(Event*);

private:
    void frameOwnerElement() const WTF_DELETED_FUNCTION;
    virtual const char* renderName() const { return "RenderSnapshottedPlugIn"; }

    virtual CursorDirective getCursor(const LayoutPoint&, Cursor&) const override final;
    virtual bool isSnapshottedPlugIn() const override final { return true; }
    virtual void paint(PaintInfo&, const LayoutPoint&) override;
    
    virtual bool canHaveWidget() const override final { return false; }

    void paintSnapshot(PaintInfo&, const LayoutPoint&);

    virtual void layout() override;

    OwnPtr<RenderImageResource> m_snapshotResource;
    bool m_isPotentialMouseActivation;
};

RENDER_OBJECT_TYPE_CASTS(RenderSnapshottedPlugIn, isSnapshottedPlugIn())

} // namespace WebCore

#endif // RenderSnapshottedPlugIn_h
