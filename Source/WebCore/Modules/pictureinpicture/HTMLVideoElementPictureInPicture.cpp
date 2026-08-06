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

#include "Document.h"
#include "EventNames.h"
#include "HTMLVideoElement.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMPromiseDeferred.h"
#include "JSPictureInPictureWindow.h"
#include "Logging.h"
#include "NodeDocument.h"
#include "PictureInPictureEvent.h"
#include "PictureInPictureSupport.h"
#include "PictureInPictureWindow.h"
#include "UserGestureIndicator.h"
#include "VideoTrackList.h"
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLVideoElementPictureInPicture);

HTMLVideoElementPictureInPicture::HTMLVideoElementPictureInPicture(HTMLVideoElement& videoElement)
    : m_videoElement(videoElement)
    , m_pictureInPictureWindow(PictureInPictureWindow::create(protect(videoElement.document())))
#if !RELEASE_LOG_DISABLED
    , m_logger(protect(videoElement)->document().logger())
    , m_logIdentifier(uniqueLogIdentifier())
#endif
{
    ALWAYS_LOG(LOGIDENTIFIER);
    videoElement.setPictureInPictureObserver(this);
}

HTMLVideoElementPictureInPicture::~HTMLVideoElementPictureInPicture()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (auto* videoElement = m_videoElement.ptr())
        videoElement->setPictureInPictureObserver(nullptr);
}

void HTMLVideoElementPictureInPicture::ref() const
{
    m_videoElement->ref();
}

void HTMLVideoElementPictureInPicture::deref() const
{
    m_videoElement->deref();
}

HTMLVideoElementPictureInPicture& HTMLVideoElementPictureInPicture::from(HTMLVideoElement& videoElement)
{
    if (!Supplement<HTMLVideoElement>::from(&videoElement, supplementName())) {
        auto newSupplement = makeUniqueWithoutRefCountedCheck<HTMLVideoElementPictureInPicture>(videoElement);
        provideTo(&videoElement, supplementName(), WTF::move(newSupplement));
    }
    return *downcast<HTMLVideoElementPictureInPicture>(Supplement<HTMLVideoElement>::from(&videoElement, supplementName()));
}

void HTMLVideoElementPictureInPicture::providePictureInPictureTo(HTMLVideoElement& videoElement)
{
    auto newSupplement = makeUniqueWithoutRefCountedCheck<HTMLVideoElementPictureInPicture>(videoElement);
    provideTo(&videoElement, supplementName(), WTF::move(newSupplement));
}

void HTMLVideoElementPictureInPicture::requestPictureInPicture(HTMLVideoElement& videoElement, Ref<DeferredPromise>&& promise)
{
    if (!supportsPictureInPicture()) {
        promise->reject(ExceptionCode::NotSupportedError, "The Picture-in-Picture mode is not supported."_s);
        return;
    }

    if (videoElement.readyState() == HTMLMediaElementEnums::HAVE_NOTHING) {
        promise->reject(ExceptionCode::InvalidStateError, "The video element is not ready to enter the Picture-in-Picture mode."_s);
        return;
    }

    if (!videoElement.hasVideo()) {
        promise->reject(ExceptionCode::InvalidStateError, "The video element does not have a video track or it has not detected a video track yet."_s);
        return;
    }

    RefPtr window = videoElement.document().window();
    if (!window) {
        promise->reject(ExceptionCode::InvalidStateError, "The video element does not have a window object."_s);
        return;
    }

    bool userActivationRequired = !protect(videoElement)->document().pictureInPictureElement();
    if (userActivationRequired && !window->hasTransientActivation()) {
        promise->reject(ExceptionCode::NotAllowedError, "The request is not triggered by a user activation."_s);
        return;
    }

    // Consumed on exit rather than here because MediaElementSession::fullscreenPermitted(), reached through webkitSetPresentationMode() below, accepts this transient activation as the user gesture.
    auto consumeUserActivation = makeScopeExit([&] {
        if (userActivationRequired)
            window->consumeTransientActivation();
    });

    Ref videoElementPictureInPicture = HTMLVideoElementPictureInPicture::from(videoElement);
    if (protect(videoElement)->document().pictureInPictureElement() == &videoElement) {
        promise->resolve<IDLInterface<PictureInPictureWindow>>(videoElementPictureInPicture->m_pictureInPictureWindow);
        return;
    }

    if (videoElementPictureInPicture->m_enterPictureInPicturePromise || videoElementPictureInPicture->m_exitPictureInPicturePromise) {
        promise->reject(ExceptionCode::NotAllowedError, "The video element is processing a Picture-in-Picture request."_s);
        return;
    }

    if (videoElement.webkitSupportsPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture)) {
        videoElementPictureInPicture->m_enterPictureInPicturePromise = WTF::move(promise);
        videoElement.webkitSetPresentationMode(HTMLVideoElement::VideoPresentationMode::PictureInPicture);
    } else
        promise->reject(ExceptionCode::NotSupportedError, "The video element does not support the Picture-in-Picture mode."_s);
}

bool HTMLVideoElementPictureInPicture::autoPictureInPicture(HTMLVideoElement& videoElement)
{
    return HTMLVideoElementPictureInPicture::from(videoElement).m_autoPictureInPicture;
}

void HTMLVideoElementPictureInPicture::setAutoPictureInPicture(HTMLVideoElement& videoElement, bool autoPictureInPicture)
{
    HTMLVideoElementPictureInPicture::from(videoElement).m_autoPictureInPicture = autoPictureInPicture;
}

bool HTMLVideoElementPictureInPicture::disablePictureInPicture(HTMLVideoElement& videoElement)
{
    return HTMLVideoElementPictureInPicture::from(videoElement).m_disablePictureInPicture;
}

void HTMLVideoElementPictureInPicture::setDisablePictureInPicture(HTMLVideoElement& videoElement, bool disablePictureInPicture)
{
    HTMLVideoElementPictureInPicture::from(videoElement).m_disablePictureInPicture = disablePictureInPicture;
}

void HTMLVideoElementPictureInPicture::exitPictureInPicture(Ref<DeferredPromise>&& promise)
{
    INFO_LOG(LOGIDENTIFIER);
    RefPtr videoElement = m_videoElement.ptr();
    if (m_enterPictureInPicturePromise || m_exitPictureInPicturePromise || !videoElement) {
        promise->reject(ExceptionCode::NotAllowedError);
        return;
    }

    m_exitPictureInPicturePromise = WTF::move(promise);
    videoElement->webkitSetPresentationMode(HTMLVideoElement::VideoPresentationMode::Inline);
}

void HTMLVideoElementPictureInPicture::didEnterPictureInPicture(const IntSize& windowSize)
{
    RefPtr videoElement = m_videoElement.ptr();
    if (!videoElement)
        return;

    INFO_LOG(LOGIDENTIFIER);

    m_pictureInPictureWindow->setSize(windowSize);

    // https://w3c.github.io/picture-in-picture/#dom-htmlvideoelement-requestpictureinpicture
    // 5. Queue a global task on the media element event task source given global, to perform the following steps:
    // 5.1. If pictureInPictureElement is not null, run the exit Picture-in-Picture algorithm.
    //      NOTE: This step is explicitly outside the EventQueue below so that the exit steps
    //      happen both within their own event queue task and before the steps below.
    if (RefPtr existing = videoElement->document().pictureInPictureElement())
        HTMLVideoElementPictureInPicture::from(*existing).didExitPictureInPicture();

    ActiveDOMObject::queueTaskKeepingObjectAlive(*videoElement, TaskSource::MediaElement, [enterPictureInPicturePromise = std::exchange(m_enterPictureInPicturePromise, nullptr), pictureInPictureWindow = m_pictureInPictureWindow](auto& videoElement) mutable {

        // 5.2. Set doc’s Picture-in-Picture element to this.
        videoElement.document().setPictureInPictureElement(&videoElement);

        // 5.3. Append relevant settings object’s origin to initiators of active Picture-in-Picture sessions.
        // 5.4. If this is fullscreenElement, exit fullscreen.
        // 5.5. Fire an event named enterpictureinpicture using PictureInPictureEvent at this with its bubbles
        //      attribute initialized to true and its pictureInPictureWindow attribute initialized to
        //      Picture-in-Picture window.
        auto initializer = PictureInPictureEvent::Init {
            { true, false, false },
            pictureInPictureWindow
        };
        videoElement.dispatchEvent(PictureInPictureEvent::create(eventNames().enterpictureinpictureEvent, WTF::move(initializer)));

        // 6. Resolve p with pipWindow.
        if (enterPictureInPicturePromise)
            enterPictureInPicturePromise->resolve<IDLInterface<PictureInPictureWindow>>(pictureInPictureWindow);
    });
}

void HTMLVideoElementPictureInPicture::didExitPictureInPicture()
{
    RefPtr videoElement = m_videoElement.ptr();
    if (!videoElement)
        return;

    INFO_LOG(LOGIDENTIFIER);
    m_pictureInPictureWindow->close();
    videoElement->document().setPictureInPictureElement(nullptr);

    ActiveDOMObject::queueTaskKeepingObjectAlive(*videoElement, TaskSource::MediaElement, [exitPictureInPicturePromise = std::exchange(m_exitPictureInPicturePromise, nullptr), pictureInPictureWindow = m_pictureInPictureWindow](auto& videoElement) mutable {

        auto initializer = PictureInPictureEvent::Init {
            { true, false, false },
            pictureInPictureWindow
        };
        videoElement.dispatchEvent(PictureInPictureEvent::create(eventNames().leavepictureinpictureEvent, WTF::move(initializer)));

        if (exitPictureInPicturePromise)
            exitPictureInPicturePromise->resolve();
    });
}

void HTMLVideoElementPictureInPicture::pictureInPictureWindowResized(const IntSize& windowSize)
{
    m_pictureInPictureWindow->setSize(windowSize);
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& HTMLVideoElementPictureInPicture::logChannel() const
{
    return LogMedia;
}
#endif

} // namespace WebCore

#endif // ENABLE(PICTURE_IN_PICTURE_API)
