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

WI.GridOverlayDiagnosticEventRecorder = class GridOverlayDiagnosticEventRecorder extends WI.DiagnosticEventRecorder
{
    constructor(controller)
    {
        super("GridOverlay", controller);
    }

    // Protected

    setup()
    {
        WI.DOMNode.addEventListener(WI.DOMNode.Event.LayoutOverlayShown, this._handleGridOverlayShown, this);
    }

    teardown()
    {
        WI.DOMNode.removeEventListener(WI.DOMNode.Event.LayoutOverlayShown, this._handleGridOverlayShown, this);
    }

    // Private

    _handleGridOverlayShown(event)
    {
        if (event.target.layoutContextType !== WI.DOMNode.LayoutContextType.Grid)
            return;

        let {initiator} = event.data;
        if (!initiator)
            return;

        console.assert(Object.values(WI.GridOverlayDiagnosticEventRecorder.Initiator).includes(initiator), initiator);

        let showTrackSizes = WI.settings.gridOverlayShowTrackSizes.value ? 1 : 0;
        let showLineNumbers = WI.settings.gridOverlayShowLineNumbers.value ? 1 : 0;
        let showLineNames = WI.settings.gridOverlayShowLineNames.value ? 1 : 0;
        let showAreaNames = WI.settings.gridOverlayShowAreaNames.value ? 1 : 0;
        let showExtendedGridLines = WI.settings.gridOverlayShowExtendedGridLines.value ? 1 : 0;

        this.logDiagnosticEvent(this.name, {initiator, showTrackSizes, showLineNumbers, showLineNames, showAreaNames, showExtendedGridLines});
    }
};

WI.GridOverlayDiagnosticEventRecorder.Initiator = {
    Badge: "badge",
    Panel: "panel"
};
