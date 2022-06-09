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

        this._initiators = Object.values(WI.GridOverlayDiagnosticEventRecorder.Initiator);
    }

    // Protected

    setup()
    {
        WI.overlayManager.addEventListener(WI.OverlayManager.Event.GridOverlayShown, this._handleGridOverlayShown, this);
    }

    teardown()
    {
        WI.overlayManager.removeEventListener(WI.OverlayManager.Event.GridOverlayShown, this._handleGridOverlayShown, this);
    }

    // Private

    _handleGridOverlayShown(event)
    {
        let initiator = event.data.initiator;
        if (!initiator || !this._initiators.includes(initiator))
            return;

        let showTrackSizes = event.data.showTrackSizes ? 1 : 0;
        let showLineNumbers = event.data.showLineNumbers ? 1 : 0;
        let showLineNames = event.data.showLineNames ? 1 : 0;
        let showAreaNames = event.data.showAreaNames ? 1 : 0;
        let showExtendedGridLines = event.data.showExtendedGridLines ? 1 : 0;

        this.logDiagnosticEvent(this.name, {initiator, showTrackSizes, showLineNumbers, showLineNames, showAreaNames, showExtendedGridLines});
    }
};

WI.GridOverlayDiagnosticEventRecorder.Initiator = {
    Badge: "badge",
    Panel: "panel"
};
