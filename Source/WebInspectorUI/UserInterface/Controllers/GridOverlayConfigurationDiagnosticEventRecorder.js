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

WI.GridOverlayConfigurationDiagnosticEventRecorder = class GridOverlayConfigurationDiagnosticEventRecorder extends WI.DiagnosticEventRecorder
{
    constructor(controller)
    {
        super("GridOverlayConfiguration", controller);

        this._inspectorHasFocus = true;
        this._lastUserInteractionTimestamp = undefined;
        this._eventSamplingTimerIdentifier = undefined;
        this._overlayOptions = {};
    }

    // Static

    // In milliseconds.
    static get eventSamplingInterval() { return 60 * 1000; }

    // Protected

    setup()
    {
        const options = {
            capture: true,
        };
        window.addEventListener("focus", this, options);
        window.addEventListener("blur", this, options);
        window.addEventListener("keydown", this, options);
        window.addEventListener("mousedown", this, options);
        WI.overlayManager.addEventListener(WI.OverlayManager.Event.GridOverlayShown, this._handleGridOverlayShown, this);
        WI.overlayManager.addEventListener(WI.OverlayManager.Event.GridOverlayHidden, this._handleGridOverlayHidden, this);
    }

    teardown()
    {
        const options = {
            capture: true,
        };
        window.removeEventListener("focus", this, options);
        window.removeEventListener("blur", this, options);
        window.removeEventListener("keydown", this, options);
        window.removeEventListener("mousedown", this, options);
        WI.overlayManager.addEventListener(WI.OverlayManager.Event.GridOverlayShown, this._handleGridOverlayShown, this);
        WI.overlayManager.addEventListener(WI.OverlayManager.Event.GridOverlayHidden, this._handleGridOverlayHidden, this);

        this._stopEventSamplingTimerIfNeeded();
    }

    _handleGridOverlayShown(event)
    {
        this._overlayOptions.showTrackSizes = event.data.showTrackSizes;
        this._overlayOptions.showLineNumbers = event.data.showLineNumbers;
        this._overlayOptions.showLineNames = event.data.showLineNames;
        this._overlayOptions.showAreaNames = event.data.showAreaNames;
        this._overlayOptions.showExtendedGridLines = event.data.showExtendedGridLines;

        if (!this._eventSamplingTimerIdentifier)
            this._startEventSamplingTimer();
    }

    _handleGridOverlayHidden()
    {
        if (WI.overlayManager.hasVisibleGridOverlays())
            return;

        this._stopEventSamplingTimerIfNeeded();
        this._overlayOptions = {};
    }

    // Public

    handleEvent(event)
    {
        switch (event.type) {
        case "focus":
            this._handleWindowFocus(event);
            break;
        case "blur":
            this._handleWindowBlur(event);
            break;
        case "keydown":
            this._handleWindowKeyDown(event);
            break;
        case "mousedown":
            this._handleWindowMouseDown(event);
            break;
        }
    }

    // Private

    _startEventSamplingTimer()
    {
        this._stopEventSamplingTimerIfNeeded();

        this._eventSamplingTimerIdentifier = setTimeout(this._sampleCurrentOverlayConfiguration.bind(this), WI.GridOverlayConfigurationDiagnosticEventRecorder.eventSamplingInterval);
    }

    _stopEventSamplingTimerIfNeeded()
    {
        if (this._eventSamplingTimerIdentifier) {
            clearTimeout(this._eventSamplingTimerIdentifier);
            this._eventSamplingTimerIdentifier = undefined;
        }
    }

    _sampleCurrentOverlayConfiguration()
    {
        // Set up the next timer first so later code can bail out if there's nothing to do.
        this._stopEventSamplingTimerIfNeeded();
        this._startEventSamplingTimer();

        let intervalSinceLastUserInteraction = performance.now() - this._lastUserInteractionTimestamp;
        if (intervalSinceLastUserInteraction > WI.GridOverlayConfigurationDiagnosticEventRecorder.eventSamplingInterval) {
            if (WI.settings.debugAutoLogDiagnosticEvents.valueRespectingDebugUIAvailability)
                console.log("GridOverlayConfiguration: sample not reported, last user interaction was %.1f seconds ago.".format(intervalSinceLastUserInteraction / 1000));
            return;
        }

        let selectedTabContentView = WI.tabBrowser.selectedTabContentView;
        console.assert(selectedTabContentView);
        if (!selectedTabContentView)
            return;

        let interval = WI.GridOverlayConfigurationDiagnosticEventRecorder.eventSamplingInterval / 1000;
        let {showTrackSizes, showLineNumbers, showLineNames, showAreaNames, showExtendedGridLines} = this._overlayOptions;

        // Encode the configuration of overlay options as a sum of increasing powers of 10 for each overlay option that is enabled (zero if disabled), convert to string and pad with zero if necessary.
        // For example, "01100" = showTrackSizes: false, showLineNumbers: true, showLineNames: true, showAreaNames: false, showExtendedGridLines: false;
        let configuration = 0;
        configuration += showTrackSizes ? 10000 : 0;
        configuration += showLineNumbers ? 1000 : 0;
        configuration += showLineNames ? 100 : 0;
        configuration += showAreaNames ? 10 : 0;
        configuration += showExtendedGridLines ? 1 : 0;
        configuration = configuration.toString().padStart(5, "0");

        this.logDiagnosticEvent(this.name, {interval, configuration, showTrackSizes, showLineNumbers, showLineNames, showAreaNames, showExtendedGridLines});
    }

    _didObserveUserInteraction()
    {
        if (!this._inspectorHasFocus)
            return;

        this._lastUserInteractionTimestamp = performance.now();
    }

    _handleWindowFocus(event)
    {
        if (event.target !== window)
            return;

        this._inspectorHasFocus = true;
    }

    _handleWindowBlur(event)
    {
        if (event.target !== window)
            return;

        this._inspectorHasFocus = false;
    }

    _handleWindowKeyDown(event)
    {
        this._didObserveUserInteraction();
    }

    _handleWindowMouseDown(event)
    {
        this._didObserveUserInteraction();
    }
};

