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

#include "config.h"
#include "StyleImageClientSet.h"

#include <wtf/Borrow.h>
#include <wtf/Vector.h>

namespace WebCore::Style {

void ImageClientSet::forEach(NOESCAPE const auto& function) const
{
    Vector<Ref<ImageClient>, 4> clients;
    for (auto entry : m_clients)
        clients.append(protect(entry.key));
    for (auto& client : borrow(clients))
        function(client.get());
}

bool ImageClientSet::anyOf(NOESCAPE const auto& predicate) const
{
    for (auto entry : m_clients) {
        Ref client = entry.key;
        if (predicate(client.get()))
            return true;
    }
    return false;
}

void ImageClientSet::imageChanged(const Image& image, const IntRect* changedRect) const
{
    forEach([&](auto& client) {
        client.imageChanged(image, changedRect);
    });
}

void ImageClientSet::imageFinishedLoading(const Image& image) const
{
    forEach([&](auto& client) {
        client.imageFinishedLoading(image);
    });
}

void ImageClientSet::imageFrameAvailable(const Image& image) const
{
    forEach([&](auto& client) {
        client.imageFrameAvailable(image);
    });
}

void ImageClientSet::imageContentChanged(const Image& image) const
{
    forEach([&](auto& client) {
        client.imageContentChanged(image);
    });
}

void ImageClientSet::imageAnimationPaused(const Image& image) const
{
    forEach([&](auto& client) {
        client.imageAnimationPaused(image);
    });
}

void ImageClientSet::scheduleRenderingUpdateForImage(const Image& image) const
{
    forEach([&](auto& client) {
        client.scheduleRenderingUpdateForImage(image);
    });
}

bool ImageClientSet::frameAvailable(const Image& image, bool isAnimating, const IntRect* changedRect) const
{
    bool isVisible = false;
    forEach([&](auto& client) {
        bool clientIsVisible = client.isShowingImageVisibly(image);
        if (clientIsVisible || !isAnimating)
            client.imageChanged(image, changedRect);
        else
            client.imageAnimationPaused(image);
        isVisible |= clientIsVisible;
        client.imageFrameAvailable(image);
    });
    return isVisible;
}

bool ImageClientSet::isShowingImageVisibly(const Image& image) const
{
    return anyOf([&](auto& client) {
        return client.isShowingImageVisibly(image);
    });
}

bool ImageClientSet::isShowingImageVisiblyInDocument(const Image& image, const Document& document) const
{
    return anyOf([&](auto& client) {
        return client.isShowingImageVisiblyInDocument(image, document);
    });
}

bool ImageClientSet::allowsImageAnimation(const Image& image) const
{
    return !anyOf([&](auto& client) {
        return !client.allowsImageAnimation(image);
    });
}

void ImageClientSet::collectElementsShowingImage(const Image& image, HashSet<Ref<Element>>& elements) const
{
    forEach([&](auto& client) {
        client.collectElementsShowingImage(image, elements);
    });
}

} // namespace WebCore::Style
