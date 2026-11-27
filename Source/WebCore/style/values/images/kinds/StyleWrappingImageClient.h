/*
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

#include "StyleImage.h"

namespace WebCore {
namespace Style {

template<typename Derived> class WrappingImageClient : public ImageClient {
private:
    const Derived& wrapper() const { return static_cast<const Derived&>(*this); }
    const ImageClientSet& wrapperClients() const { return wrapper().clients(); }

    void imageChanged(const Image&, const IntRect* changedRect) override final { wrapperClients().imageChanged(protect(wrapper()), Derived::wrapsImageUnchanged ? changedRect : nullptr); }
    void imageFinishedLoading(const Image&) override final { wrapperClients().imageFinishedLoading(protect(wrapper())); }
    void imageFrameAvailable(const Image&) override final { wrapperClients().imageFrameAvailable(protect(wrapper())); }
    void imageContentChanged(const Image&) override final { wrapperClients().imageContentChanged(protect(wrapper())); }
    void imageAnimationPaused(const Image&) override final { wrapperClients().imageAnimationPaused(protect(wrapper())); }
    void scheduleRenderingUpdateForImage(const Image& image) override final { wrapperClients().scheduleRenderingUpdateForImage(image); }
    bool isShowingImageVisibly(const Image&) const override final { return wrapperClients().isShowingImageVisibly(protect(wrapper())); }
    bool isShowingImageVisiblyInDocument(const Image&, const Document& document) const override final { return wrapperClients().isShowingImageVisiblyInDocument(protect(wrapper()), document); }
    bool allowsImageAnimation(const Image&) const override final { return wrapperClients().allowsImageAnimation(protect(wrapper())); }
    void collectElementsShowingImage(const Image&, HashSet<Ref<Element>>& elements) const override final { wrapperClients().collectElementsShowingImage(protect(wrapper()), elements); }
};

} // namespace Style
} // namespace WebCore
