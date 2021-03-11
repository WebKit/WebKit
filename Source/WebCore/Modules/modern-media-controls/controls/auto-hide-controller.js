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

const AutoHideDelayMS = 4000;

class AutoHideController
{

    constructor(mediaControls)
    {
        this._mediaControls = mediaControls;

        this._pointerIdentifiersPreventingAutoHide = new Set;
        this._pointerIdentifiersPreventingAutoHideForHover = new Set;

        this._mediaControls.element.addEventListener("pointermove", this);
        this._mediaControls.element.addEventListener("pointerdown", this);
        this._mediaControls.element.addEventListener("pointerup", this);
        this._mediaControls.element.addEventListener("pointerleave", this);
        this._mediaControls.element.addEventListener("pointerout", this);

        if (GestureRecognizer.SupportsTouches)
            this._tapGestureRecognizer = new TapGestureRecognizer(this._mediaControls.element, this);

        this.autoHideDelay = AutoHideDelayMS;
    }

    // Public

    get fadesWhileIdle()
    {
        return this._fadesWhileIdle;
    }

    set fadesWhileIdle(flag)
    {
        if (this._fadesWhileIdle == flag)
            return;

        this._fadesWhileIdle = flag;

        if (!this._fadesWhileIdle)
            this._mediaControls.faded = false;

        this._resetAutoHideTimer();
    }

    get hasSecondaryUIAttached()
    {
        return this._hasSecondaryUIAttached;
    }

    set hasSecondaryUIAttached(flag)
    {
        if (this._hasSecondaryUIAttached == flag)
            return;

        this._hasSecondaryUIAttached = flag;

        this._resetAutoHideTimer();
    }

    // Protected

    handleEvent(event)
    {
        if (event.currentTarget !== this._mediaControls.element)
            return;

        switch (event.type) {
        case "pointermove":
            this._mediaControls.faded = false;
            if (this._mediaControls.isPointInControls(new DOMPoint(event.clientX, event.clientY))) {
                this._pointerIdentifiersPreventingAutoHideForHover.add(event.pointerId);
                this._cancelAutoHideTimer();
            } else {
                this._pointerIdentifiersPreventingAutoHideForHover.delete(event.pointerId);
                this._resetAutoHideTimer();
            }
            return;

        case "pointerleave":
            this._pointerIdentifiersPreventingAutoHide.delete(event.pointerId);
            this._pointerIdentifiersPreventingAutoHideForHover.delete(event.pointerId);

            // If the pointer is a mouse (supports hover), see if we can
            // immediately hide without waiting for the auto-hide timer.
            if (event.pointerType == "mouse")
                this._autoHideTimerFired();

            this._resetAutoHideTimer();
            return;

        case "pointerdown":
            // Remember the current faded state so that we can determine,
            // if we recognize a tap, if it should fade the controls out.
            this._nextTapCanFadeControls = !this._mediaControls.faded;
            this._pointerIdentifiersPreventingAutoHide.add(event.pointerId);
            this._mediaControls.faded = false;
            this._cancelAutoHideTimer();
            return;

        case "pointerup":
            this._pointerIdentifiersPreventingAutoHide.delete(event.pointerId);
            this._resetAutoHideTimer();
            return;
        }
    }

    gestureRecognizerStateDidChange(recognizer)
    {
        if (this._tapGestureRecognizer !== recognizer || recognizer.state !== GestureRecognizer.States.Recognized)
            return;

        this._mediaControls.faded = this._nextTapCanFadeControls && !this._mediaControls.isPointInControls(recognizer.locationInClient());
        delete this._nextTapCanFadeControls;
    }

    mediaControlsFadedStateDidChange()
    {
        this._resetAutoHideTimer();
    }

    mediaControlsBecameInvisible()
    {
        this._cancelAutoHideTimer();
    }

    // Private

    get _canFadeControls()
    {
        return this._fadesWhileIdle && !this._hasSecondaryUIAttached;
    }

    _cancelAutoHideTimer()
    {
        window.clearTimeout(this._autoHideTimer);
        delete this._autoHideTimer;
    }

    _resetAutoHideTimer()
    {
        this._cancelAutoHideTimer();

        if (this._mediaControls.faded || !this._canFadeControls)
            return;

        this._autoHideTimer = window.setTimeout(this._autoHideTimerFired.bind(this), this.autoHideDelay);
    }

    _autoHideTimerFired()
    {
        delete this._autoHideTimer;

        this._mediaControls.faded = this._canFadeControls && !this._pointerIdentifiersPreventingAutoHide.size && !this._pointerIdentifiersPreventingAutoHideForHover.size;
    }

}
