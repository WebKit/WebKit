/*
 * Copyright (C) 2019 Carlos Eduardo Ramalho <cadubentzen@gmail.com>.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "HTMLVideoElementPictureInPicture.h"

#if ENABLE(PICTURE_IN_PICTURE_API)

#include "HTMLVideoElement.h"
#include "JSDOMPromiseDeferred.h"
#include "PictureInPictureWindow.h"
#include "VideoTrackList.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLVideoElementPictureInPicture);

HTMLVideoElementPictureInPicture::HTMLVideoElementPictureInPicture(HTMLVideoElement& videoElement)
    : m_videoElement(videoElement)
{
    m_videoElement.setPictureInPictureObserver(this);
}

HTMLVideoElementPictureInPicture::~HTMLVideoElementPictureInPicture()
{
    m_videoElement.setPictureInPictureObserver(nullptr);
}

HTMLVideoElementPictureInPicture* HTMLVideoElementPictureInPicture::from(HTMLVideoElement& videoElement)
{
    HTMLVideoElementPictureInPicture* supplement = static_cast<HTMLVideoElementPictureInPicture*>(Supplement<HTMLVideoElement>::from(&videoElement, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<HTMLVideoElementPictureInPicture>(videoElement);
        supplement = newSupplement.get();
        provideTo(&videoElement, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

void HTMLVideoElementPictureInPicture::requestPictureInPicture(HTMLVideoElement& videoElement, Ref<DeferredPromise>&& promise)
{
    if (!videoElement.player() || !videoElement.player()->supportsPictureInPicture()) {
        promise->reject(NotSupportedError);
        return;
    }

    if (videoElement.readyState() == HTMLMediaElementEnums::HAVE_NOTHING) {
        promise->reject(InvalidStateError);
        return;
    }

#if ENABLE(VIDEO_TRACK)
    if (!videoElement.videoTracks() || !videoElement.videoTracks()->length()) {
        promise->reject(InvalidStateError);
        return;
    }
#endif

    bool userActivationRequired = !videoElement.document().pictureInPictureElement();
    if (userActivationRequired && !UserGestureIndicator::processingUserGesture()) {
        promise->reject(NotAllowedError);
        return;
    }

    if (videoElement.document().pictureInPictureElement() == &videoElement) {
        promise->reject(NotAllowedError);
        return;
    }

    auto videoElementPictureInPicture = HTMLVideoElementPictureInPicture::from(videoElement);
    if (videoElementPictureInPicture->m_enterPictureInPicturePromise || videoElementPictureInPicture->m_exitPictureInPicturePromise) {
        promise->reject(NotAllowedError);
        return;
    }

    if (videoElement.webkitSupportsPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture)) {
        videoElementPictureInPicture->m_enterPictureInPicturePromise = WTFMove(promise);
        videoElement.webkitSetPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture);
    } else
        promise->reject(NotSupportedError);
}

bool HTMLVideoElementPictureInPicture::autoPictureInPicture(HTMLVideoElement& videoElement)
{
    return HTMLVideoElementPictureInPicture::from(videoElement)->m_autoPictureInPicture;
}

void HTMLVideoElementPictureInPicture::setAutoPictureInPicture(HTMLVideoElement& videoElement, bool autoPictureInPicture)
{
    HTMLVideoElementPictureInPicture::from(videoElement)->m_autoPictureInPicture = autoPictureInPicture;
}

bool HTMLVideoElementPictureInPicture::disablePictureInPicture(HTMLVideoElement& videoElement)
{
    return HTMLVideoElementPictureInPicture::from(videoElement)->m_disablePictureInPicture;
}

void HTMLVideoElementPictureInPicture::setDisablePictureInPicture(HTMLVideoElement& videoElement, bool disablePictureInPicture)
{
    HTMLVideoElementPictureInPicture::from(videoElement)->m_disablePictureInPicture = disablePictureInPicture;
}

void HTMLVideoElementPictureInPicture::exitPictureInPicture(Ref<DeferredPromise>&& promise)
{
    if (m_enterPictureInPicturePromise || m_exitPictureInPicturePromise) {
        promise->reject(NotAllowedError);
        return;
    }

    m_exitPictureInPicturePromise = WTFMove(promise);
    m_videoElement.webkitSetPresentationMode(HTMLVideoElement::VideoPresentationMode::Inline);
}

void HTMLVideoElementPictureInPicture::didEnterPictureInPicture()
{
    m_videoElement.document().setPictureInPictureElement(&m_videoElement);
    if (m_enterPictureInPicturePromise) {
        m_enterPictureInPicturePromise->resolve();
        m_enterPictureInPicturePromise = nullptr;
    }
}

void HTMLVideoElementPictureInPicture::didExitPictureInPicture()
{
    m_videoElement.document().setPictureInPictureElement(nullptr);
    if (m_exitPictureInPicturePromise) {
        m_exitPictureInPicturePromise->resolve();
        m_exitPictureInPicturePromise = nullptr;
    }
}

} // namespace WebCore

#endif // ENABLE(PICTURE_IN_PICTURE_API)
