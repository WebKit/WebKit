/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

PopoverTracker = function(element, presentPopoverCallback, context)
{
    BaseObject.call(this);

    console.assert(element);
    console.assert(presentPopoverCallback && typeof presentPopoverCallback == "function");

    this._element = element;
    this._presentPopover = presentPopoverCallback;
    this._context = context;
    this._active = false;

    element.classList.add("popover-tracking");
    element.addEventListener("mouseenter", this._mouseEnteredPopoverOrElement.bind(this), true);
    element.addEventListener("mouseleave", this._mouseExitedPopoverOrElement.bind(this), true);
};

PopoverTracker._popover = null; // Only one popover may be active at any time.

BaseObject.addConstructorFunctions(PopoverTracker);

PopoverTracker.prototype = {
    constructor: PopoverTracker,
    __proto__: BaseObject.prototype,

    _mouseEnteredPopoverOrElement: function(event)
    {
        var popover = PopoverTracker._popover;
        var popoverWasVisible = popover && popover.visible;
        if (popover) {
            // Abort fade-out when re-entering the same element or an existing popover.
            if ((this._active && this._element.isSelfOrAncestor(event.toElement))
                || popover.element.isSelfOrAncestor(event.toElement)) {
                popover.makeVisibleImmediately();
                return;
            }

            // We entered a different element, dismiss the old popover.
            popover.dismissImmediately();
        }
        console.assert(!PopoverTracker._popover);

        var popover = new Dashboard.Popover(this);
        if (!this._presentPopover(event.target, popover, this._context))
            return;

        if (popoverWasVisible)
            popover.makeVisibleImmediately();

        this._active = true;
        PopoverTracker._popover = popover;
        popover.element.addEventListener("mouseenter", this._mouseEnteredPopoverOrElement.bind(this), true);
        popover.element.addEventListener("mouseleave", this._mouseExitedPopoverOrElement.bind(this), true);
    },

    _mouseExitedPopoverOrElement: function(event)
    {
        var popover = PopoverTracker._popover;

        if (!popover)
            return;

        if (this._element.isSelfOrAncestor(event.toElement) || popover.element.isSelfOrAncestor(event.toElement))
            return;

        if (popover.visible)
            popover.dismiss();
        else
            popover.dismissImmediately();
    },

    didDismissPopover: function(popover)
    {
        console.assert(popover === PopoverTracker._popover);
        this._active = false;
        PopoverTracker._popover = null;
    }
};
