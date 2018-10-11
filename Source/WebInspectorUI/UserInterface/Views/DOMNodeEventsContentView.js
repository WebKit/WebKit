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

WI.DOMNodeEventsContentView = class DOMNodeEventsContentView extends WI.ContentView
{
    constructor(domNode, {startTimestamp} = {})
    {
        console.assert(domNode instanceof WI.DOMNode);

        const representedObject = null;
        super(representedObject);

        this._domNode = domNode;
        this._startTimestamp = startTimestamp || 0;

        this.element.classList.add("dom-node-details", "dom-events");

        this._breakdownView = null;
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        this._breakdownView = new WI.DOMEventsBreakdownView(this._domNode.domEvents.slice(), {
            includeGraph: true,
            startTimestamp: this._startTimestamp,
        });
        this.addSubview(this._breakdownView);

        this._domNode.addEventListener(WI.DOMNode.Event.DidFireEvent, this._handleDOMNodeDidFireEvent, this);
    }

    closed()
    {
        this._domNode.removeEventListener(null, null, this);

        super.closed();
    }

    // Private

    _handleDOMNodeDidFireEvent(event)
    {
        let {domEvent} = event.data;

        if (this._breakdownView)
            this._breakdownView.addEvent(domEvent);
    }
};
