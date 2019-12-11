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

        if (GestureRecognizer.SupportsTouches) {
            this._touchIdentifiersPreventingAutoHide = new Set;
            this._tapGestureRecognizer = new TapGestureRecognizer(this._mediaControls.element, this);
        }

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

        if (GestureRecognizer.SupportsTouches) {
            if (flag) {
                this._mediaControls.element.addEventListener("touchstart", this);
                this._mediaControls.element.addEventListener("touchend", this);
            } else {
                this._mediaControls.element.removeEventListener("touchstart", this);
                this._mediaControls.element.removeEventListener("touchend", this);
            }
            this._tapGestureRecognizer.enabled = flag;
        } else {
            if (flag) {
                this._mediaControls.element.addEventListener("mousemove", this);
                this._mediaControls.element.addEventListener("mouseleave", this);
            } else {
                this._mediaControls.element.removeEventListener("mousemove", this);
                this._mediaControls.element.removeEventListener("mouseleave", this);
            }
        }

        if (flag && !this._mediaControls.faded)
            this._resetAutoHideTimer(false);
        else if (!flag)
            this._mediaControls.faded = false;
    }

    // Protected

    handleEvent(event)
    {
        if (event.currentTarget !== this._mediaControls.element)
            return;

        if (event.type === "mousemove") {
            this._mediaControls.faded = false;
            this._resetAutoHideTimer(true);
            if (this._mediaControls.isPointInControls(new DOMPoint(event.clientX, event.clientY))) {
                this._disableAutoHiding = true;
                this._cancelNonEnforcedAutoHideTimer();
            } else {
                delete this._disableAutoHiding;
                this._resetAutoHideTimer(true);
            }
        } else if (event.type === "mouseleave" && this._fadesWhileIdle && !this.hasSecondaryUIAttached && !this._enforceAutoHideTimer)
            this._mediaControls.faded = !this._mediaControls.isPointInControls(new DOMPoint(event.clientX, event.clientY));

        if (event.type === "touchstart") {
            // Remember the current faded state so that we can determine,
            // if we recognize a tap, if it should fade the controls out.
            this._nextTapCanFadeControls = !this._mediaControls.faded;
            for (var touch in event.changedTouches)
                this._touchIdentifiersPreventingAutoHide.add(touch.identifier);
            this._mediaControls.faded = false;
            this._disableAutoHiding = true;
            this._cancelNonEnforcedAutoHideTimer();
        } else if (event.type === "touchend") {
            for (var touch in event.changedTouches)
                this._touchIdentifiersPreventingAutoHide.delete(touch.identifier);
            if (!this._touchIdentifiersPreventingAutoHide.size) {
                delete this._disableAutoHiding;
                this._resetAutoHideTimer(true);
            }
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
        if (this._mediaControls.faded)
            delete this._enforceAutoHideTimer;
        else
            this._resetAutoHideTimer(true);
    }

    mediaControlsBecameInvisible()
    {
        this._cancelNonEnforcedAutoHideTimer();
    }

    // Private

    _cancelNonEnforcedAutoHideTimer()
    {
        if (!this._enforceAutoHideTimer)
            this._cancelAutoHideTimer();
    }

    _cancelAutoHideTimer()
    {
        window.clearTimeout(this._autoHideTimer);
        delete this._autoHideTimer;
    }

    _resetAutoHideTimer(cancelable)
    {
        if (cancelable && this._enforceAutoHideTimer)
            return;

        this._cancelAutoHideTimer();

        if (cancelable)
            delete this._enforceAutoHideTimer;
        else
            this._enforceAutoHideTimer = true;

        this._autoHideTimer = window.setTimeout(this._autoHideTimerFired.bind(this), this.autoHideDelay);
    }

    _autoHideTimerFired()
    {
        delete this._enforceAutoHideTimer;
        if (this._disableAutoHiding)
            return;

        this._cancelAutoHideTimer();
        this._mediaControls.faded = this._fadesWhileIdle && !this.hasSecondaryUIAttached;
    }

}
