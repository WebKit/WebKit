/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaControlsChromiumAndroid_h
#define MediaControlsChromiumAndroid_h

#if ENABLE(VIDEO)
#include "MediaControls.h"
#include "MediaControlsChromium.h"

namespace WebCore {

class MediaControlOverlayPlayButtonElement;

class MediaControlOverlayEnclosureElement : public MediaControlChromiumEnclosureElement {
public:
    static PassRefPtr<MediaControlOverlayEnclosureElement> create(Document*);

private:
    explicit MediaControlOverlayEnclosureElement(Document*);
    virtual const AtomicString& shadowPseudoId() const;
};

class MediaControlsChromiumAndroid : public MediaControlsChromium {
public:
    static PassRefPtr<MediaControlsChromiumAndroid> createControls(Document*);

    virtual void setMediaController(MediaControllerInterface*) OVERRIDE;
    virtual void playbackStarted() OVERRIDE;
    virtual void playbackStopped() OVERRIDE;

private:
    explicit MediaControlsChromiumAndroid(Document*);

    MediaControlOverlayPlayButtonElement* m_overlayPlayButton;
    MediaControlOverlayEnclosureElement* m_overlayEnclosure;
};

}

#endif

#endif
