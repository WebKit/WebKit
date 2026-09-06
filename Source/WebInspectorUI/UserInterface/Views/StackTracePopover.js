/*
 * Copyright (C) 2026 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

WI.StackTracePopover = class StackTracePopover extends WI.Popover
{
    constructor()
    {
        super();

        const selectable = false;
        this._treeOutline = new WI.TreeOutline(selectable);
        this._treeOutline.disclosureButtons = false;
        this._treeOutline.addEventListener(WI.TreeOutline.Event.ElementClicked, this._handleTreeOutlineElementClicked, this);

        this._treeController = new WI.StackTraceTreeController(this._treeOutline);
    }

    // Static

    static present(stackTrace, targetElement)
    {
        WI.StackTracePopover._sharedInstance ??= new WI.StackTracePopover;
        WI.StackTracePopover._sharedInstance.show(stackTrace, targetElement);
    }

    // Public

    show(stackTrace, targetElement)
    {
        this._treeController.stackTrace = stackTrace;

        let content = document.createElement("div");
        content.className = "stack-trace-popover";
        content.appendChild(this._treeOutline.element);
        this.content = content;

        let targetFrame = WI.Rect.rectFromClientRect(targetElement.getBoundingClientRect());
        this.present(targetFrame.pad(2), [WI.RectEdge.MAX_Y, WI.RectEdge.MIN_Y, WI.RectEdge.MAX_X]);
    }

    // Private

    _handleTreeOutlineElementClicked(event)
    {
        this.dismiss();
    }
};
