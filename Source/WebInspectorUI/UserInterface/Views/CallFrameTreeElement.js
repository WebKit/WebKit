/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WebInspector.CallFrameTreeElement = class CallFrameTreeElement extends WebInspector.GeneralTreeElement
{
    constructor(callFrame)
    {
        console.assert(callFrame instanceof WebInspector.CallFrame);

        let className = WebInspector.CallFrameView.iconClassNameForCallFrame(callFrame);
        let title = callFrame.functionName || WebInspector.UIString("(anonymous function)");

        super(className, title, null, callFrame, false);

        if (!callFrame.nativeCode && callFrame.sourceCodeLocation) {
            let displayScriptURL = callFrame.sourceCodeLocation.displaySourceCode.url;
            if (displayScriptURL) {
                this.subtitle = document.createElement("span");
                callFrame.sourceCodeLocation.populateLiveDisplayLocationString(this.subtitle, "textContent");
                // Set the tooltip on the entire tree element in onattach, once the element is created.
                this.tooltipHandledSeparately = true;
            }
        }

        this._callFrame = callFrame;
    }

    // Public

    get callFrame()
    {
        return this._callFrame;
    }

    // Protected

    onattach()
    {
        super.onattach();

        console.assert(this.element);

        if (this.tooltipHandledSeparately) {
            let tailCallSuffix = "";
            if (this._callFrame.isTailDeleted)
                tailCallSuffix = " " + WebInspector.UIString("(Tail Call)");
            let tooltipPrefix = this.mainTitle + tailCallSuffix + "\n";
            this._callFrame.sourceCodeLocation.populateLiveDisplayLocationTooltip(this.element, tooltipPrefix);
        }
    }
};
