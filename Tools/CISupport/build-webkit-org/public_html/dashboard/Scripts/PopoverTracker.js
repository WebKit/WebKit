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
    console.assert(presentPopoverCallback && typeof presentPopoverCallback === "function");

    this._element = element;
    this._presentPopover = presentPopoverCallback;
    this._context = context;
    this._active = false;

    element.classList.add("popover-tracking");
    element.addEventListener("mouseenter", this._mouseEnteredPopoverOrElement.bind(this), true);
    element.addEventListener("mouseleave", this._mouseExitedPopoverOrElement.bind(this), true);
    element.popoverTracker = this;
};

// Only one popover may be active at any time. An active popover is not necessarily visible already.
PopoverTracker._popover = null;

// A candidate timer may be active whether we have an active popover or not.
PopoverTracker._candidateTracker = null;
PopoverTracker._candidateTimer = null;
PopoverTracker.CandidateSwitchDelay = 200;

BaseObject.addConstructorFunctions(PopoverTracker);

PopoverTracker._setCandidatePopoverTracker = function(popoverTracker)
{
    console.assert(!popoverTracker || !popoverTracker._active);

    if (PopoverTracker._candidateTracker) {
        clearTimeout(PopoverTracker._candidateTimer);
        PopoverTracker._candidateTimer = null;
    }

    console.assert(!PopoverTracker._candidateTimer);
    
    PopoverTracker._candidateTracker = popoverTracker;

    if (popoverTracker)
        PopoverTracker._candidateTimer = setTimeout(PopoverTracker._candidatePopoverTrackerTimerFired, PopoverTracker.CandidateSwitchDelay);
}

PopoverTracker._candidatePopoverTrackerTimerFired = function()
{
    var hadPopover = !!PopoverTracker._popover;
    if (hadPopover) {
        console.assert(PopoverTracker._popover.visible);
        PopoverTracker._popover.dismissImmediately();
    }
    console.assert(!PopoverTracker._popover);

    var tracker = PopoverTracker._candidateTracker;

    PopoverTracker._setCandidatePopoverTracker(null);
    tracker._showPopover();

    if (PopoverTracker._popover && hadPopover)
        PopoverTracker._popover.makeVisibleImmediately();
}

PopoverTracker._onblur = function()
{
    if (PopoverTracker._popover)
        PopoverTracker._popover.dismissImmediately();

    PopoverTracker._setCandidatePopoverTracker(null);
}

window.addEventListener("blur", PopoverTracker._onblur, true);

PopoverTracker.prototype = {
    constructor: PopoverTracker,
    __proto__: BaseObject.prototype,

    _showPopover: function()
    {
        var popover = new Dashboard.Popover(this);
        if (!this._presentPopover(this._element, popover, this._context))
            return;

        this._active = true;
        PopoverTracker._popover = popover;
        popover.element.addEventListener("mouseenter", this._mouseEnteredPopoverOrElement.bind(this), true);
        popover.element.addEventListener("mouseleave", this._mouseExitedPopoverOrElement.bind(this), true);
    },

    _mouseEnteredPopoverOrElement: function(event)
    {
        var popover = PopoverTracker._popover;

        if (popover) {
            // Abort fade-out when re-entering the same element or an existing popover.
            if ((this._active && this._element.isSelfOrAncestor(event.toElement))
                || popover.element.isSelfOrAncestor(event.toElement)) {
                popover.makeVisibleImmediately();
                PopoverTracker._setCandidatePopoverTracker(null);
                return;
            }

            // We entered a different element.
            if (popover.visible) {
                PopoverTracker._setCandidatePopoverTracker(this);
                return;
            }

            // The popover wasn't visible yet, so it was effectively non-existent.
            popover.dismissImmediately();
        }

        console.assert(!PopoverTracker._popover);
        console.assert(!PopoverTracker._candidateTracker);
        console.assert(!PopoverTracker._candidateTimer);

        PopoverTracker._setCandidatePopoverTracker(this);
    },

    _mouseExitedPopoverOrElement: function(event)
    {
        if (this === PopoverTracker._candidateTracker) {
            if (this._element.isSelfOrAncestor(event.toElement))
                return;
            PopoverTracker._setCandidatePopoverTracker(null);
            return;
        }

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
