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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StyleImageClient.h"

#include "CachedImageClient.h"

namespace WebCore {
namespace Style {

ImageClient::ImageClient() = default;
ImageClient::~ImageClient() = default;

void ImageClient::imageChanged(const Image&, const IntRect*) const
{
}

void ImageClient::notifyFinished(const CachedImage&) const
{
}

bool ImageClient::allowsAnimation(const CachedImage&) const
{
    return true;
}

bool ImageClient::canDestroyDecodedData(const CachedImage&) const
{
    return true;
}

bool ImageClient::useSystemDarkAppearance(const CachedImage&) const
{
    return false;
}

VisibleInViewportState ImageClient::imageFrameAvailable(const CachedImage&, ImageAnimatingState, const IntRect*) const
{
    return VisibleInViewportState::No;
}

VisibleInViewportState ImageClient::imageVisibleInViewport(const CachedImage&, const Document&) const
{
    return VisibleInViewportState::No;
}

void ImageClient::didRemoveCachedImageClient(const CachedImage&) const
{
}

void ImageClient::imageContentChanged(const CachedImage&) const
{
}

void ImageClient::scheduleRenderingUpdateForImage(const CachedImage&) const
{
}

bool ImageClient::isRendererClient() const
{
    return false;
}

} // namespace Style
} // namespace WebCore
