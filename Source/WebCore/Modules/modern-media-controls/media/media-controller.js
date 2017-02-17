/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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

const CompactModeMaxWidth = 241;
const ReducedPaddingMaxWidth = 300;
const AudioTightPaddingMaxWidth = 400;

class MediaController
{

    constructor(shadowRoot, media, host)
    {
        this.shadowRoot = shadowRoot;
        this.media = media;
        this.host = host;

        this.container = shadowRoot.appendChild(document.createElement("div"));
        this.container.className = "media-controls-container";
        this.container.addEventListener("click", this, true);

        if (host) {
            host.controlsDependOnPageScaleFactor = this.layoutTraits & LayoutTraits.iOS;
            this.container.appendChild(host.textTrackContainer);
        }

        this._updateControlsIfNeeded();

        shadowRoot.addEventListener("resize", this);

        media.videoTracks.addEventListener("addtrack", this);
        media.videoTracks.addEventListener("removetrack", this);

        if (media.webkitSupportsPresentationMode)
            media.addEventListener("webkitpresentationmodechanged", this);
        else
            media.addEventListener("webkitfullscreenchange", this);
    }

    // Public

    get layoutTraits()
    {
        let traits = window.navigator.platform === "MacIntel" ? LayoutTraits.macOS : LayoutTraits.iOS;
        if (this.media.webkitSupportsPresentationMode) {
            if (this.media.webkitPresentationMode === "fullscreen")
                return traits | LayoutTraits.Fullscreen;
        } else if (this.media.webkitDisplayingFullscreen)
            return traits | LayoutTraits.Fullscreen;

        const controlsWidth = this._controlsWidth();
        if (controlsWidth <= CompactModeMaxWidth)
            return traits | LayoutTraits.Compact;

        const isAudio = this.media instanceof HTMLAudioElement || this.media.videoTracks.length === 0;
        if (isAudio && controlsWidth <= AudioTightPaddingMaxWidth)
            return traits | LayoutTraits.TightPadding;

        if (!isAudio && controlsWidth <= ReducedPaddingMaxWidth)
            return traits | LayoutTraits.ReducedPadding;

        return traits;
    }

    togglePlayback()
    {
        if (this.media.paused)
            this.media.play();
        else
            this.media.pause();
    }

    // Protected

    set pageScaleFactor(pageScaleFactor)
    {
        this.controls.scaleFactor = pageScaleFactor;
        this._updateControlsSize();
    }

    set usesLTRUserInterfaceLayoutDirection(flag)
    {
        this.controls.usesLTRUserInterfaceLayoutDirection = flag;
    }

    macOSControlsBackgroundWasClicked()
    {
        // Toggle playback when clicking on the video but not on any controls on macOS.
        if (this.media.controls)
            this.togglePlayback();
    }

    handleEvent(event)
    {
        if (event instanceof TrackEvent && event.currentTarget === this.media.videoTracks)
            this._updateControlsIfNeeded();
        else if (event.type === "resize" && event.currentTarget === this.shadowRoot)
            this._updateControlsIfNeeded();
        else if (event.type === "click" && event.currentTarget === this.container)
            this._containerWasClicked(event);
        else if (event.currentTarget === this.media) {
            this._updateControlsIfNeeded();
            if (event.type === "webkitpresentationmodechanged")
                this._returnMediaLayerToInlineIfNeeded();
        }
    }

    // Private

    _containerWasClicked(event)
    {
        // We need to call preventDefault() here since, in the case of Media Documents,
        // playback may be toggled when clicking on the video.
        event.preventDefault();
    }

    _updateControlsIfNeeded()
    {
        const layoutTraits = this.layoutTraits;
        const previousControls = this.controls;
        const ControlsClass = this._controlsClassForLayoutTraits(layoutTraits);
        if (previousControls && previousControls.constructor === ControlsClass) {
            this.controls.layoutTraits = layoutTraits;
            this._updateControlsSize();
            return;
        }

        // Before we reset the .controls property, we need to destroy the previous
        // supporting objects so we don't leak.
        if (this._supportingObjects) {
            for (let supportingObject of this._supportingObjects)
                supportingObject.destroy();
        }

        this.controls = new ControlsClass;
        this.controls.delegate = this;

        if (this.shadowRoot.host && this.shadowRoot.host.dataset.autoHideDelay)
            this.controls.controlsBar.autoHideDelay = this.shadowRoot.host.dataset.autoHideDelay;

        if (previousControls) {
            this.controls.fadeIn();
            this.container.replaceChild(this.controls.element, previousControls.element);
            this.controls.usesLTRUserInterfaceLayoutDirection = previousControls.usesLTRUserInterfaceLayoutDirection;
        } else
            this.container.appendChild(this.controls.element);

        this.controls.layoutTraits = layoutTraits;
        this._updateControlsSize();

        this._supportingObjects = [AirplaySupport, ControlsVisibilitySupport, FullscreenSupport, MuteSupport, PiPSupport, PlacardSupport, PlaybackSupport, ScrubbingSupport, SeekBackwardSupport, SeekForwardSupport, SkipBackSupport, StartSupport, StatusSupport, TimeLabelsSupport, TracksSupport, VolumeSupport, VolumeDownSupport, VolumeUpSupport].map(SupportClass => {
            return new SupportClass(this);
        }, this);
    }

    _updateControlsSize()
    {
        this.controls.width = this._controlsWidth();
        this.controls.height = Math.round(this.media.offsetHeight * this.controls.scaleFactor);
    }

    _controlsWidth()
    {
        return Math.round(this.media.offsetWidth * (this.controls ? this.controls.scaleFactor : 1));
    }

    _returnMediaLayerToInlineIfNeeded()
    {
        if (this.host)
            window.requestAnimationFrame(() => this.host.setPreparedToReturnVideoLayerToInline(this.media.webkitPresentationMode !== PiPMode));
    }

    _controlsClassForLayoutTraits(layoutTraits)
    {
        if (layoutTraits & LayoutTraits.iOS)
            return IOSInlineMediaControls;
        if (layoutTraits & LayoutTraits.Fullscreen)
            return MacOSFullscreenMediaControls;
        return MacOSInlineMediaControls;
    }

}
