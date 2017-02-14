/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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

class ControlsBar extends LayoutNode
{

    constructor(mediaControls)
    {
        super(`<div class="controls-bar"></div>`);

        this._translation = new DOMPoint;
        this._mediaControls = mediaControls;

        if (GestureRecognizer.SupportsTouches)
            this._tapGestureRecognizer = new TapGestureRecognizer(this._mediaControls.element, this);

        this.autoHideDelay = ControlsBar.DefaultAutoHideDelay;

        this.fadesWhileIdle = false;
        this.userInteractionEnabled = true;
    }

    // Public

    get translation()
    {
        return new DOMPoint(this._translation.x, this._translation.y);
    }

    set translation(point)
    {
        if (this._translation.x === point.x && this._translation.y === point.y)
            return;

        this._translation = new DOMPoint(point.x, point.y);
        this.markDirtyProperty("translation");
    }

    get userInteractionEnabled()
    {
        return this._userInteractionEnabled;
    }

    set userInteractionEnabled(flag)
    {
        if (this._userInteractionEnabled === flag)
            return;

        this._userInteractionEnabled = flag;
        this.markDirtyProperty("userInteractionEnabled");
    }

    get fadesWhileIdle()
    {
        return this._fadesWhileIdle;
    }

    set fadesWhileIdle(flag)
    {
        if (this._fadesWhileIdle == flag)
            return;

        this._fadesWhileIdle = flag;

        if (GestureRecognizer.SupportsTouches)
            this._tapGestureRecognizer.enabled = flag;
        else {
            if (flag) {
                this._mediaControls.element.addEventListener("mousemove", this);
                this._mediaControls.element.addEventListener("mouseleave", this);
                this.element.addEventListener("mouseenter", this);
                this.element.addEventListener("mouseleave", this);
            } else {
                this._mediaControls.element.removeEventListener("mousemove", this);
                this._mediaControls.element.removeEventListener("mouseleave", this);
                this.element.removeEventListener("mouseenter", this);
                this.element.removeEventListener("mouseleave", this);
            }
        }

        if (flag && !this.faded)
            this._resetAutoHideTimer(false);
        else if (!flag)
            this.faded = false;
    }

    get visible()
    {
        return super.visible;
    }

    set visible(flag)
    {
        if (this.visible === flag)
            return;

        // If we just got made visible again, let's fade the controls in.
        if (flag && !this.visible)
            this.faded = false;
        else if (!flag)
            this._cancelNonEnforcedAutoHideTimer();

        super.visible = flag;

        this._mediaControls.controlsBarVisibilityDidChange(this);
    }

    get faded()
    {
        return !!this._faded;
    }

    set faded(flag)
    {
        if (this._faded === flag)
            return;

        this._faded = flag;
        if (!flag)
            this._resetAutoHideTimer(true);
        else
            delete this._enforceAutoHideTimer;

        this.markDirtyProperty("faded");
    }

    // Protected

    handleEvent(event)
    {
        if (event.currentTarget === this._mediaControls.element) {
            if (event.type === "mousemove") {
                this.faded = false;
                this._resetAutoHideTimer(true);
            } else if (event.type === "mouseleave" && this._fadesWhileIdle && !this._enforceAutoHideTimer)
                this.faded = true;
        } else if (event.currentTarget === this.element) {
            if (event.type === "mouseenter") {
                this._disableAutoHiding = true;
                this._cancelNonEnforcedAutoHideTimer();
            } else if (event.type === "mouseleave") {
                delete this._disableAutoHiding;
                this._resetAutoHideTimer(true);
            } else if (event.type === "focus")
                this.faded = false;
        }
    }

    gestureRecognizerStateDidChange(recognizer)
    {
        if (this._tapGestureRecognizer !== recognizer || recognizer.state !== GestureRecognizer.States.Recognized)
            return;

        if (this.faded)
            this.faded = false;
        else {
            let ancestor = this.element.parentNode;
            while (ancestor && !(ancestor instanceof ShadowRoot))
                ancestor = ancestor.parentNode;

            const shadowRoot = ancestor;
            if (!shadowRoot)
                return;

            const tapLocation = recognizer.locationInClient();
            const tappedElement = shadowRoot.elementFromPoint(tapLocation.x, tapLocation.y);
            if (!this.element.contains(tappedElement))
                this.faded = true;
        }
    }

    commitProperty(propertyName)
    {
        if (propertyName === "translation")
            this.element.style.transform = `translate(${this._translation.x}px, ${this._translation.y}px)`;
        else if (propertyName === "faded")
            this.element.classList.toggle("faded", this.faded);
        else if (propertyName === "userInteractionEnabled")
            this.element.style.pointerEvents = this._userInteractionEnabled ? "all" : "none";
        else
            super.commitProperty(propertyName);
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
        this.faded = this._fadesWhileIdle;
    }

}

ControlsBar.DefaultAutoHideDelay = 4000;
