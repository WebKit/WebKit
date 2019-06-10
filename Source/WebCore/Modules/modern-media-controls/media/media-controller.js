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

class MediaController
{

    constructor(shadowRoot, media, host)
    {
        this.shadowRoot = shadowRoot;
        this.media = media;
        this.host = host;

        this.fullscreenChangeEventType = media.webkitSupportsPresentationMode ? "webkitpresentationmodechanged" : "webkitfullscreenchange";

        this.hasPlayed = false;

        this.container = shadowRoot.appendChild(document.createElement("div"));
        this.container.className = "media-controls-container";

        this._updateControlsIfNeeded();
        this._usesLTRUserInterfaceLayoutDirection = false;

        if (host) {
            host.controlsDependOnPageScaleFactor = this.layoutTraits & LayoutTraits.iOS;
            this.container.insertBefore(host.textTrackContainer, this.controls.element);
            if (host.isInMediaDocument)
                this.mediaDocumentController = new MediaDocumentController(this);
        }

        scheduler.flushScheduledLayoutCallbacks();

        shadowRoot.addEventListener("resize", this);

        media.videoTracks.addEventListener("addtrack", this);
        media.videoTracks.addEventListener("removetrack", this);

        media.addEventListener("play", this);
        media.addEventListener(this.fullscreenChangeEventType, this);

        window.addEventListener("keydown", this);

        new MutationObserver(this._updateControlsAvailability.bind(this)).observe(this.media, { attributes: true, attributeFilter: ["controls"] });
    }

    // Public

    get isAudio()
    {
        if (this.media instanceof HTMLAudioElement)
            return true;

        if (this.host && !this.host.isInMediaDocument && this.media instanceof HTMLVideoElement)
            return false;

        if (this.media.readyState < HTMLMediaElement.HAVE_METADATA)
            return false;

        if (this.media.videoWidth || this.media.videoHeight)
            return false;

        return !this.media.videoTracks.length;
    }

    get isYouTubeEmbedWithTitle()
    {
        const url = new URL(this.media.ownerDocument.defaultView.location.href);
        return url.href.includes("youtube.com/embed/") && url.searchParams.get("showinfo") !== "0";
    }

    get isFullscreen()
    {
        return this.media.webkitSupportsPresentationMode ? this.media.webkitPresentationMode === "fullscreen" : this.media.webkitDisplayingFullscreen;
    }

    get layoutTraits()
    {
        if (this.host && this.host.compactMode)
            return LayoutTraits.Compact;

        let traits = window.isIOSFamily ? LayoutTraits.iOS : LayoutTraits.macOS;
        if (this.isFullscreen)
            return traits | LayoutTraits.Fullscreen;
        return traits;
    }

    togglePlayback()
    {
        if (this.media.paused)
            this.media.play().catch(e => {});
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
        if (this._usesLTRUserInterfaceLayoutDirection === flag)
            return;

        this._usesLTRUserInterfaceLayoutDirection = flag;
        this.controls.usesLTRUserInterfaceLayoutDirection = flag;
    }

    mediaControlsVisibilityDidChange()
    {
        this._controlsUserVisibilityDidChange();
    }

    mediaControlsFadedStateDidChange()
    {
        this._controlsUserVisibilityDidChange();
        this._updateTextTracksClassList();
    }

    macOSControlsBackgroundWasClicked()
    {
        // Toggle playback when clicking on the video but not on any controls on macOS.
        if (this.media.controls)
            this.togglePlayback();
    }

    iOSInlineMediaControlsRecognizedTapGesture()
    {
        // Initiate playback when tapping anywhere over the video when showsStartButton is true.
        if (this.media.controls)
            this.media.play();
    }

    iOSInlineMediaControlsRecognizedPinchInGesture()
    {
        this.media.webkitEnterFullscreen();
    }

    handleEvent(event)
    {
        if (event instanceof TrackEvent && event.currentTarget === this.media.videoTracks)
            this._updateControlsIfNeeded();
        else if (event.type === "resize" && event.currentTarget === this.shadowRoot) {
            this._updateControlsIfNeeded();
            // We must immediately perform layouts so that we don't lag behind the media layout size.
            scheduler.flushScheduledLayoutCallbacks();
        } else if (event.currentTarget === this.media) {
            if (event.type === "play")
                this.hasPlayed = true;
            this._updateControlsIfNeeded();
            this._updateControlsAvailability();
            if (event.type === "webkitpresentationmodechanged")
                this._returnMediaLayerToInlineIfNeeded();
        } else if (event.type === "keydown" && this.isFullscreen && event.key === " ") {
            this.togglePlayback();
            event.preventDefault();
        }
    }

    // Private

    _supportingObjectClasses()
    {
        if (this.layoutTraits & LayoutTraits.Compact)
            return [CompactMediaControlsSupport];

        return [AirplaySupport, AudioSupport, ControlsVisibilitySupport, FullscreenSupport, MuteSupport, PiPSupport, PlacardSupport, PlaybackSupport, ScrubbingSupport, SeekBackwardSupport, SeekForwardSupport, SkipBackSupport, SkipForwardSupport, StartSupport, StatusSupport, TimeControlSupport, TracksSupport, VolumeSupport, VolumeDownSupport, VolumeUpSupport];
    }

    _updateControlsIfNeeded()
    {
        const layoutTraits = this.layoutTraits;
        const previousControls = this.controls;
        const ControlsClass = this._controlsClassForLayoutTraits(layoutTraits);
        if (previousControls && previousControls.constructor === ControlsClass) {
            this._updateTextTracksClassList();
            this._updateControlsSize();
            return;
        }

        // Before we reset the .controls property, we need to disable the previous
        // supporting objects so we don't leak.
        if (this._supportingObjects) {
            for (let supportingObject of this._supportingObjects)
                supportingObject.disable();
        }

        this.controls = new ControlsClass;
        this.controls.delegate = this;

        if (this.controls.autoHideController && this.shadowRoot.host && this.shadowRoot.host.dataset.autoHideDelay)
            this.controls.autoHideController.autoHideDelay = this.shadowRoot.host.dataset.autoHideDelay;

        if (previousControls) {
            this.controls.fadeIn();
            this.container.replaceChild(this.controls.element, previousControls.element);
            this.controls.usesLTRUserInterfaceLayoutDirection = previousControls.usesLTRUserInterfaceLayoutDirection;
        } else
            this.container.appendChild(this.controls.element);

        this._updateTextTracksClassList();
        this._updateControlsSize();

        this._supportingObjects = this._supportingObjectClasses().map(SupportClass => new SupportClass(this), this);

        this.controls.shouldUseSingleBarLayout = this.controls instanceof InlineMediaControls && this.isYouTubeEmbedWithTitle;

        this._updateControlsAvailability();
    }

    _updateControlsSize()
    {
        // To compute the bounds of the controls, we need to account for the computed transform applied
        // to the media element, and apply the inverted transform to the bounds computed on the container
        // element in the shadow root, which is naturally sized to match the metrics of its host,
        // excluding borders.

        // First, we traverse the node hierarchy up from the media element to compute the effective
        // transform matrix applied to the media element.
        let node = this.media;
        let transform = new DOMMatrix;
        while (node && node instanceof HTMLElement) {
            transform = transform.multiply(new DOMMatrix(getComputedStyle(node).transform));
            node = node.parentNode;
        }

        // Then, we take each corner of the container element in the shadow root and transform
        // each with the inverted matrix we just computed so that we can compute the untransformed
        // bounds of the media element.
        const bounds = this.container.getBoundingClientRect();
        const invertedTransform = transform.inverse();
        let minX = Infinity;
        let minY = Infinity;
        let maxX = -Infinity;
        let maxY = -Infinity;
        [
            new DOMPoint(bounds.left, bounds.top),
            new DOMPoint(bounds.right, bounds.top),
            new DOMPoint(bounds.right, bounds.bottom),
            new DOMPoint(bounds.left, bounds.bottom)
        ].forEach(corner => {
            const point = corner.matrixTransform(invertedTransform);
            if (point.x < minX)
                minX = point.x;
            if (point.x > maxX)
                maxX = point.x;
            if (point.y < minY)
                minY = point.y;
            if (point.y > maxY)
                maxY = point.y;
        });

        // Finally, we factor in the scale factor of the controls themselves, which reflects the page's scale factor.
        this.controls.width = Math.round((maxX - minX) * this.controls.scaleFactor);
        this.controls.height = Math.round((maxY - minY) * this.controls.scaleFactor);

        this.controls.shouldCenterControlsVertically = this.isAudio;
    }

    _returnMediaLayerToInlineIfNeeded()
    {
        if (this.host)
            this.host.setPreparedToReturnVideoLayerToInline(this.media.webkitPresentationMode !== PiPMode);
    }

    _controlsClassForLayoutTraits(layoutTraits)
    {
        if (layoutTraits & LayoutTraits.Compact)
            return CompactMediaControls;
        if (layoutTraits & LayoutTraits.iOS)
            return IOSInlineMediaControls;
        if (layoutTraits & LayoutTraits.Fullscreen)
            return MacOSFullscreenMediaControls;
        return MacOSInlineMediaControls;
    }

    _updateTextTracksClassList()
    {
        if (!this.host)
            return;

        const layoutTraits = this.layoutTraits;
        if (layoutTraits & LayoutTraits.Fullscreen)
            return;

        this.host.textTrackContainer.classList.toggle("visible-controls-bar", !this.controls.faded);
    }

    _controlsUserVisibilityDidChange()
    {
        if (!this.controls || !this._supportingObjects)
            return;

        this._supportingObjects.forEach(supportingObject => supportingObject.controlsUserVisibilityDidChange());
    }

    _shouldControlsBeAvailable()
    {
        // Controls are always available with compact layout.
        if (this.layoutTraits & LayoutTraits.Compact)
            return true;

        // Controls are always available while in fullscreen on macOS, and they are never available when in fullscreen on iOS.
        if (this.isFullscreen)
            return !!(this.layoutTraits & LayoutTraits.macOS);

        // Otherwise, for controls to be available, the controls attribute must be present on the media element
        // or the MediaControlsHost must indicate that controls are forced.
        return this.media.controls || !!(this.host && this.host.shouldForceControlsDisplay);
    }

    _updateControlsAvailability()
    {
        const shouldControlsBeAvailable = this._shouldControlsBeAvailable();
        if (!shouldControlsBeAvailable)
            this._supportingObjects.forEach(supportingObject => supportingObject.disable());
        else
            this._supportingObjects.forEach(supportingObject => supportingObject.enable());

        this.controls.visible = shouldControlsBeAvailable;
    }

}
