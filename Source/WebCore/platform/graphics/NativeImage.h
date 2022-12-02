/*
 * Copyright (C) 2004-2020 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 * Copyright (C) 2012 Company 100 Inc.
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

#include "Color.h"
#include "IntSize.h"
#include "PlatformImage.h"
#include "RenderingResourceIdentifier.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class NativeImage : public ThreadSafeRefCounted<NativeImage>, public CanMakeWeakPtr<NativeImage> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Observer {
    public:
        virtual ~Observer() = default;
        virtual void releaseNativeImage(RenderingResourceIdentifier) = 0;
    protected:
        Observer() = default;
    };

    static WEBCORE_EXPORT RefPtr<NativeImage> create(PlatformImagePtr&&, RenderingResourceIdentifier = RenderingResourceIdentifier::generate());

    WEBCORE_EXPORT ~NativeImage();

    const PlatformImagePtr& platformImage() const { return m_platformImage; }
    RenderingResourceIdentifier renderingResourceIdentifier() const { return m_renderingResourceIdentifier; }

    WEBCORE_EXPORT IntSize size() const;
    bool hasAlpha() const;
    Color singlePixelSolidColor() const;
    WEBCORE_EXPORT DestinationColorSpace colorSpace() const;

    void addObserver(Observer& observer) { m_observers.add(&observer); }
    void removeObserver(Observer& observer) { m_observers.remove(&observer); }
    
    void clearSubimages();

private:
    NativeImage(PlatformImagePtr&&);
    NativeImage(PlatformImagePtr&&, RenderingResourceIdentifier);

    PlatformImagePtr m_platformImage;
    HashSet<Observer*> m_observers;
    RenderingResourceIdentifier m_renderingResourceIdentifier;
};

} // namespace WebCore
