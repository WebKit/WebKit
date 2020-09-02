/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

WI.EventBreakpointPopover = class EventBreakpointPopover extends WI.BreakpointPopover
{
    constructor(delegate, breakpoint)
    {
        console.assert(!breakpoint || breakpoint instanceof WI.EventBreakpoint, breakpoint);

        super(delegate, breakpoint);

        this._currentEventNameCompletions = [];
        this._eventNameSuggestionsView = new WI.CompletionSuggestionsView(this, {preventBlur: true});
    }

    // Static

    static get supportsEditing()
    {
        return WI.EventBreakpoint.supportsEditing;
    }

    // Public

    dismiss()
    {
        this._eventNameSuggestionsView.hide();

        super.dismiss();
    }

    // CompletionSuggestionsView delegate

    completionSuggestionsClickedCompletion(suggestionsView, selectedText)
    {
        this._domEventNameInputElement.value = selectedText;

        this.dismiss();
    }

    // Protected

    populateContent()
    {
        let eventLabelElement = document.createElement("label");
        eventLabelElement.textContent = WI.UIString("Event");

        this._domEventNameInputElement = document.createElement("input");
        this._domEventNameInputElement.id = "edit-breakpoint-popover-content-event-name";
        this._domEventNameInputElement.setAttribute("dir", "ltr");
        this._domEventNameInputElement.spellcheck = false;
        this._domEventNameInputElement.addEventListener("keydown", this._handleEventInputKeydown.bind(this));
        this._domEventNameInputElement.addEventListener("input", this._handleEventInputInput.bind(this));
        this._domEventNameInputElement.addEventListener("blur", this._handleEventInputBlur.bind(this));

        eventLabelElement.setAttribute("for", this._domEventNameInputElement.id);

        this.addRow("event", eventLabelElement, this._domEventNameInputElement);

        // Focus the event name input after the popover is shown.
        setTimeout(() => {
            this._domEventNameInputElement.focus();
        });
    }

    createBreakpoint(options = {})
    {
        console.assert(!options.eventName, options);
        options.eventName = this._domEventNameInputElement.value;
        if (!options.eventName)
            return null;

        return new WI.EventBreakpoint(WI.EventBreakpoint.Type.Listener, options);
    }

    // Private

     _showSuggestionsView()
     {
        let computedStyle = window.getComputedStyle(this._domEventNameInputElement);
        let padding = parseInt(computedStyle.borderLeftWidth) + parseInt(computedStyle.paddingLeft);

        let rect = WI.Rect.rectFromClientRect(this._domEventNameInputElement.getBoundingClientRect());
        rect.origin.x += padding;
        rect.size.width -= padding + parseInt(computedStyle.borderRightWidth) + parseInt(computedStyle.paddingRight);
        this._eventNameSuggestionsView.show(rect.pad(2));
     }

    _handleEventInputKeydown(event)
    {
        let shouldDismiss = isEnterKey(event);

        if (shouldDismiss || (event.key === "Tab" && this._eventNameSuggestionsView.visible)) {
            if (this._eventNameSuggestionsView.visible && this._eventNameSuggestionsView.selectedIndex < this._currentEventNameCompletions.length)
                this._domEventNameInputElement.value = this._currentEventNameCompletions[this._eventNameSuggestionsView.selectedIndex];
        } else if ((event.key === "ArrowUp" || event.key === "ArrowDown") && this._eventNameSuggestionsView.visible) {
            event.stop();

            if (event.key === "ArrowDown")
                this._eventNameSuggestionsView.selectNext();
            else
                this._eventNameSuggestionsView.selectPrevious();
        }

        if (shouldDismiss)
            this.dismiss();
    }

    _handleEventInputInput(event)
    {
        let eventName = this._domEventNameInputElement.value.toLowerCase();

        WI.domManager.getSupportedEventNames().then((supportedEventNames) => {
            this._currentEventNameCompletions = [];
            for (let supportedEventName of supportedEventNames) {
                if (supportedEventName.toLowerCase().startsWith(eventName))
                    this._currentEventNameCompletions.push(supportedEventName);
            }

            if (!this._currentEventNameCompletions.length || (this._currentEventNameCompletions.length === 1 && this._currentEventNameCompletions[0].toLowerCase() === eventName)) {
                this._eventNameSuggestionsView.hide();
                return;
            }

            this._eventNameSuggestionsView.update(this._currentEventNameCompletions);
            this._showSuggestionsView();
        });
    }

    _handleEventInputBlur(event)
    {
        this._eventNameSuggestionsView.hide();
    }
};

WI.EventBreakpointPopover.ReferencePage = WI.ReferencePage.EventBreakpoints;
