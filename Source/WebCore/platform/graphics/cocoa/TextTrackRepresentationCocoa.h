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

#ifndef TextTrackRepresentationCocoa_h
#define TextTrackRepresentationCocoa_h

#if (PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))) && ENABLE(VIDEO)

#include "TextTrackRepresentation.h"
#include <QuartzCore/CALayer.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>

@class WebCoreTextTrackRepresentationCocoaHelper;

namespace WebCore {

class TextTrackRepresentationCocoa final : public TextTrackRepresentation, public CanMakeWeakPtr<TextTrackRepresentationCocoa, WeakPtrFactoryInitialization::Eager> {
public:
    explicit TextTrackRepresentationCocoa(TextTrackRepresentationClient&);
    virtual ~TextTrackRepresentationCocoa();

    TextTrackRepresentationClient& client() const { return m_client; }

    PlatformLayer* platformLayer() final { return m_layer.get(); }

    IntRect bounds() const final;
    void boundsChanged();

private:
    void update() final;
    void setContentScale(float) final;
    void setHidden(bool) const final;

    TextTrackRepresentationClient& m_client;
    RetainPtr<CALayer> m_layer;
    RetainPtr<WebCoreTextTrackRepresentationCocoaHelper> m_delegate;
};

}

#endif // (PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))) && ENABLE(VIDEO)

#endif // TextTrackRepresentationCocoa_h
