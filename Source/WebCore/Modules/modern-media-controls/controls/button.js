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

class Button extends LayoutItem
{

    constructor(layoutDelegate)
    {
        super({
            element: "<button />",
            layoutDelegate
        });

        this._enabled = true;

        if (GestureRecognizer.SupportsTouches)
            this._tapGestureRecognizer = new TapGestureRecognizer(this.element, this);
        else
            this.element.addEventListener("click", this);
    }

    // Public

    get enabled()
    {
        return this._enabled;
    }

    set enabled(flag)
    {
        if (this._enabled === flag)
            return;

        this._enabled = flag;
        if (this.layoutDelegate && typeof this.layoutDelegate.layout === "function")
            this.layoutDelegate.layout();
    }

    // Protected

    handleEvent(event)
    {
        if (event.type === "click" && event.currentTarget === this.element)
            this._notifyDelegateOfActivation();
    }

    gestureRecognizerStateDidChange(recognizer)
    {
        if (this._tapGestureRecognizer === recognizer && recognizer.state === GestureRecognizer.States.Recognized)
            this._notifyDelegateOfActivation();
    }

    // Private

    _notifyDelegateOfActivation()
    {
        if (this._enabled && this.uiDelegate && typeof this.uiDelegate.buttonWasPressed === "function")
            this.uiDelegate.buttonWasPressed(this);
    }

}
