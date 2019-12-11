/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.CallFrameTreeController = class CallFrameTreeController extends WI.Object
{
    constructor(treeOutline)
    {
        console.assert(treeOutline instanceof WI.TreeOutline);

        super();

        this._treeOutline = treeOutline;

        if (this._treeOutline.selectable)
            this._treeOutline.addEventListener(WI.TreeOutline.Event.SelectionDidChange, this._treeSelectionDidChange, this);
        else
            this._treeOutline.addEventListener(WI.TreeOutline.Event.ElementClicked, this._treeElementClicked, this);
    }

    // Public

    get treeOutline() { return this._treeOutline; }

    get callFrames()
    {
        return this._callFrames;
    }

    set callFrames(callFrames)
    {
        callFrames = callFrames || [];
        if (this._callFrames === callFrames)
            return;

        this._callFrames = callFrames;

        this._treeOutline.removeChildren();

        for (let callFrame of this._callFrames)
            this._treeOutline.appendChild(new WI.CallFrameTreeElement(callFrame));
    }

    disconnect()
    {
        this._treeOutline.removeEventListener(null, null, this);
    }

    // Private

    _treeElementClicked(event)
    {
        this._showSourceCodeLocation(event.data.treeElement);
    }

    _treeSelectionDidChange(event)
    {
        this._showSourceCodeLocation(this._treeOutline.selectedTreeElement);
    }

    _showSourceCodeLocation(treeElement)
    {
        let callFrame = treeElement.callFrame;
        if (!callFrame.sourceCodeLocation)
            return;

        WI.showSourceCodeLocation(callFrame.sourceCodeLocation, {
            ignoreNetworkTab: true,
            ignoreSearchTab: true,
        });
    }
};
