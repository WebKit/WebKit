/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
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

class BackgroundClickDelegateNotifier
{

    constructor(mediaControls)
    {
        this._mediaControls = mediaControls;
        mediaControls.element.addEventListener("mousedown", this);
        mediaControls.element.addEventListener("click", this);
    }

    // Protected

    handleEvent(event)
    {
        const mediaControls = this._mediaControls;
        if (event.currentTarget !== mediaControls.element)
            return;

        // Only notify that the background was clicked when the "mousedown" event
        // was also received, which wouldn't happen if the "mousedown" event caused
        // the tracks panel to be hidden, unless we're in fullscreen in which case
        // we can simply check that the panel is not currently presented.
        if (event.type === "mousedown" && !mediaControls.tracksPanel.presented)
            this._receivedMousedown = true;
        else if (event.type === "click") {
            if (this._receivedMousedown && event.target === mediaControls.element && mediaControls.delegate && typeof mediaControls.delegate.macOSControlsBackgroundWasClicked === "function")
                mediaControls.delegate.macOSControlsBackgroundWasClicked();
            delete this._receivedMousedown
        }
    }

}
