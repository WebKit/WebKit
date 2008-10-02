/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

WebInspector.CallStackSidebarPane = function()
{
    WebInspector.SidebarPane.call(this, WebInspector.UIString("Call Stack"));
}

WebInspector.CallStackSidebarPane.prototype = {
    update: function(callFrame, sourceIDMap)
    {
        this.bodyElement.removeChildren();

        this.placards = [];
        delete this._selectedCallFrame;

        if (!callFrame) {
            var infoElement = document.createElement("div");
            infoElement.className = "info";
            infoElement.textContent = WebInspector.UIString("Not Paused");
            this.bodyElement.appendChild(infoElement);
            return;
        }

        var title;
        var subtitle;
        var scriptOrResource;

        do {
            switch (callFrame.type) {
            case "function":
                title = callFrame.functionName || WebInspector.UIString("(anonymous function)");
                break;
            case "program":
                title = WebInspector.UIString("(program)");
                break;
            }

            scriptOrResource = sourceIDMap[callFrame.sourceID];
            subtitle = WebInspector.displayNameForURL(scriptOrResource.sourceURL || scriptOrResource.url);

            if (callFrame.line > 0) {
                if (subtitle)
                    subtitle += ":" + callFrame.line;
                else
                    subtitle = WebInspector.UIString("line %d", callFrame.line);
            }

            var placard = new WebInspector.Placard(title, subtitle);
            placard.callFrame = callFrame;

            placard.element.addEventListener("click", this._placardSelected.bind(this), false);

            this.placards.push(placard);
            this.bodyElement.appendChild(placard.element);

            callFrame = callFrame.caller;
        } while (callFrame);
    },

    get selectedCallFrame()
    {
        return this._selectedCallFrame;
    },

    set selectedCallFrame(x)
    {
        if (this._selectedCallFrame === x)
            return;

        this._selectedCallFrame = x;

        for (var i = 0; i < this.placards.length; ++i) {
            var placard = this.placards[i];
            placard.selected = (placard.callFrame === this._selectedCallFrame);
        }

        this.dispatchEventToListeners("call frame selected");
    },

    _placardSelected: function(event)
    {
        var placardElement = event.target.enclosingNodeOrSelfWithClass("placard");
        this.selectedCallFrame = placardElement.placard.callFrame;
    }
}

WebInspector.CallStackSidebarPane.prototype.__proto__ = WebInspector.SidebarPane.prototype;
