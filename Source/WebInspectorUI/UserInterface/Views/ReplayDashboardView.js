/*
 * Copyright (C) 2014 University of Washington. All rights reserved.
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

WebInspector.ReplayDashboardView = function(representedObject)
{
    WebInspector.DashboardView.call(this, representedObject, "replay");

    this._navigationBar = new WebInspector.NavigationBar;
    this.element.appendChild(this._navigationBar.element);

    this._captureButtonItem = new WebInspector.ActivateButtonNavigationItem("replay-dashboard-capture", WebInspector.UIString("Start Recording"), WebInspector.UIString("Stop Recording"), "Images/ReplayRecordingButton.svg", 16, 16);
    this._captureButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._captureButtonItemClicked, this);
    this._captureButtonItem.hidden = true;
    this._navigationBar.addNavigationItem(this._captureButtonItem);

    this._replayButtonItem = new WebInspector.ToggleButtonNavigationItem("replay-dashboard-replay", WebInspector.UIString("Start Playback"), WebInspector.UIString("Pause Playback"), "Images/ReplayPlayButton.svg", "Images/ReplayPauseButton.svg", 16, 16);
    this._replayButtonItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._replayButtonItemClicked, this);
    this._replayButtonItem.hidden = true;
    this._navigationBar.addNavigationItem(this._replayButtonItem);

    // Add events required to track capture and replay state.
    WebInspector.replayManager.addEventListener(WebInspector.ReplayManager.Event.CaptureStarted, this._captureStarted, this);
    WebInspector.replayManager.addEventListener(WebInspector.ReplayManager.Event.CaptureStopped, this._captureStopped, this);
    WebInspector.replayManager.addEventListener(WebInspector.ReplayManager.Event.PlaybackStarted, this._playbackStarted, this);
    WebInspector.replayManager.addEventListener(WebInspector.ReplayManager.Event.PlaybackPaused, this._playbackPaused, this);
    WebInspector.replayManager.addEventListener(WebInspector.ReplayManager.Event.PlaybackFinished, this._playbackFinished, this);

    // Manually initialize style classes by querying current replay state.
    if (WebInspector.replayManager.sessionState === WebInspector.ReplayManager.SessionState.Capturing)
        this._captureStarted();
    else if (WebInspector.replayManager.sessionState === WebInspector.ReplayManager.SessionState.Inactive)
        this._captureStopped();
    // ReplayManager.sessionState must be Replaying.
    else if (WebInspector.replayManager.segmentState === WebInspector.ReplayManager.SegmentState.Dispatching)
        this._playbackStarted();
    // ReplayManager.sessionState must be Unloaded or Loaded, so execution is paused.
    else
        this._playbackPaused();
};

// Class names for top-level flex items within the replay dashboard.
WebInspector.ReplayDashboardView.RecordingContainerStyleClassName = "recording-container";

// Class names for single buttons.
WebInspector.ReplayDashboardView.RecordButtonStyleClassName = "record-button";
WebInspector.ReplayDashboardView.ReplayButtonStyleClassName = "replay-button";

WebInspector.ReplayDashboardView.prototype = {
    constructor: WebInspector.ReplayDashboardView,
    __proto__: WebInspector.DashboardView.prototype,

    // Private

    _captureButtonItemClicked: function()
    {
        if (WebInspector.replayManager.sessionState !== WebInspector.ReplayManager.SessionState.Capturing)
            WebInspector.replayManager.startCapturing();
        else
            WebInspector.replayManager.stopCapturing();
    },

    _replayButtonItemClicked: function(event)
    {
        console.assert(WebInspector.replayManager.sessionState !== WebInspector.ReplayManager.SessionState.Capturing, "Tried to start replaying while SessionState is Capturing!");

        if (WebInspector.replayManager.sessionState === WebInspector.ReplayManager.SessionState.Inactive)
            WebInspector.replayManager.replayToCompletion();
        else if (WebInspector.replayManager.segmentState === WebInspector.ReplayManager.SegmentState.Dispatching)
            WebInspector.replayManager.pausePlayback();
        else
            WebInspector.replayManager.replayToCompletion();
    },

    _captureStarted: function()
    {
        this._captureButtonItem.hidden = false;
        this._captureButtonItem.activated = true;
        this._replayButtonItem.hidden = true;
    },

    _captureStopped: function()
    {
        this._captureButtonItem.activated = false;
        this._captureButtonItem.hidden = true;
        this._replayButtonItem.hidden = false;
    },

    _playbackStarted: function()
    {
        this._replayButtonItem.toggled = true;
    },

    _playbackPaused: function()
    {
        this._replayButtonItem.toggled = false;
    },

    _playbackFinished: function()
    {
        this._replayButtonItem.toggled = false;
    }
};
