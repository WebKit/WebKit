/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

WI.OverlayManager = class OverlayManager extends WI.Object
{
    constructor()
    {
        super();

        this._gridOverlayForNodeMap = new Map;
    }

    // Public

    get nodesWithGridOverlay()
    {
        return Array.from(this._gridOverlayForNodeMap.keys());
    }

    showGridOverlay(domNode, {color, showLineNames, showLineNumbers, showExtendedGridLines, showTrackSizes, showAreaNames} = {})
    {
        console.assert(!domNode.destroyed, domNode);
        if (domNode.destroyed)
            return;

        console.assert(domNode instanceof WI.DOMNode, domNode);
        console.assert(!color || color instanceof WI.Color, color);

        color ||= WI.Color.fromString("magenta"); // fallback color

        let target = WI.assumingMainTarget();
        let commandArguments = {
            nodeId: domNode.id,
            gridColor: color.toProtocol(),
            showLineNames: !!showLineNames,
            showLineNumbers: !!showLineNumbers,
            showExtendedGridLines: !!showExtendedGridLines,
            showTrackSizes: !!showTrackSizes,
            showAreaNames: !!showAreaNames,
        };
        target.DOMAgent.showGridOverlay.invoke(commandArguments);

        let overlay = {domNode, ...commandArguments};
        this._gridOverlayForNodeMap.set(domNode, overlay);

        this.dispatchEventToListeners(WI.OverlayManager.Event.GridOverlayShown, overlay);
    }

    hideGridOverlay(domNode)
    {
        console.assert(domNode instanceof WI.DOMNode, domNode);
        console.assert(!domNode.destroyed, domNode);
        if (domNode.destroyed)
            return;

        let overlay = this._gridOverlayForNodeMap.take(domNode);
        if (!overlay)
            return;

        let target = WI.assumingMainTarget();
        target.DOMAgent.hideGridOverlay(domNode.id);

        this.dispatchEventToListeners(WI.OverlayManager.Event.GridOverlayHidden, overlay);
    }
};

WI.OverlayManager.Event = {
    GridOverlayShown: "overlay-manager-grid-overlay-shown",
    GridOverlayHidden: "overlay-manager-grid-overlay-hidden",
};
