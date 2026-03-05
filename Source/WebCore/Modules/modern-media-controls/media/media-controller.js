/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

const maxNonLiveDuration = 604800; // 604800 seconds == 1 week
const MaximumNarrowViewerControlsBarWidth = 468;

class MediaController
{
    static EmptyRanges = {
        get length() { return 0; },
        start: function(index) { return undefined; },
        end: function(index) { return undefined; },
    }

    static NullMedia = {
        get audioTracks() { return []; },
        get autoplay() { return false; },
        get buffered() { return EmptyRanges; },
        get controls() { return false; },
        get currentTime() { return 0; },
        set currentTime(time) { },
        get defaultPlaybackRate() { return 1; },
        set defaultPlaybackRate(rate) { },
        get duration() { return 0; },
        get error() { return null; },
        get muted() { return false; },
        set muted(muted) { },
        get networkState() { return HTMLMediaElement.NETWORK_NO_SOURCE; },
        get paused() { return false; },
        get playbackRate() { return 1; },
        set playbackRate(rate) { },
        get played() { return EmptyRanges; },
        get readyState() { return HTMLMediaElement.HAVE_NOTHING; },
        get seekable() { return EmptyRanges; },
        get textTracks() { return []; },
        get videoTracks() { return []; },
        get volume() { return 1; },
        set volume(volume) { },
        get webkitCurrentPlaybackTargetIsWireless() { return false; },
        get webkitPresentationMode() { return "inline"; },
        get webkitSupportsFullscreen() { return false; },

        pause: function() { },
        play: function() { },
        fastSeek: function(time) { },
        requestPictureInPicture: function() { },
        webkitEnterFullscreen: function() { },
        webkitExitFullscreen: function() { },
        webkitSetPresentationMode: function(mode) { },
        webkitShowPlaybackTargetPicker: function() { },
        webkitSupportsPresentationMode: function(mode) { return false; },
    };

    constructor(shadowRoot, media, host)
    {
        this.shadowRootWeakRef = new WeakRef(shadowRoot);
        this.mediaWeakRef = new WeakRef(media);
        this.host = host;

        this.fullscreenChangeEventType = media.webkitSupportsPresentationMode ? "webkitpresentationmodechanged" : "webkitfullscreenchange";

        this.hasPlayed = !media.paused || !!media.played.length;

        this.container = shadowRoot.appendChild(document.createElement("div"));

        this._updateControlsIfNeeded();
        this._usesLTRUserInterfaceLayoutDirection = false;

        if (host) {
            this.container.className = host.mediaControlsContainerClassName;
            host.controlsDependOnPageScaleFactor = this.layoutTraits.controlsDependOnPageScaleFactor();
            this.container.insertBefore(host.textTrackContainer, this.controls.element);
            if (host.isInMediaDocument)
                this.mediaDocumentController = new MediaDocumentController(this);
        }

        scheduler.flushScheduledLayoutCallbacks();

        shadowRoot.addEventListener("resize", this);
        shadowRoot.addEventListener("fullscreenchange", this);

        media.videoTracks.addEventListener("addtrack", this);
        media.videoTracks.addEventListener("removetrack", this);

        media.addEventListener("play", this);
        media.addEventListener(this.fullscreenChangeEventType, this);
        media.addEventListener("keydown", this);
        media.addEventListener("keyup", this);
        media.addEventListener("click", this);

        window.addEventListener("keydown", this);

        new MutationObserver(this._updateControlsAvailability.bind(this)).observe(this.media, { attributes: true, attributeFilter: ["controls"] });
    }

    // Public
    get media()
    {
        return this.mediaWeakRef.deref() ?? MediaController.NullMedia;
    }

    get shadowRoot()
    {

        return this.shadowRootWeakRef ? this.shadowRootWeakRef.deref() : null;
    }

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
        if (!this.media)
            return false;

        return this.media.webkitSupportsPresentationMode ? this.media.webkitPresentationMode === "fullscreen" || (this.host && this.host.inWindowFullscreen) : this.media.webkitDisplayingFullscreen;
    }

    get layoutTraits()
    {
        let mode = LayoutTraits.Mode.Inline;
        if (this.isFullscreen) {
            if (this.host && this.host.inWindowFullscreen && this.media.offsetWidth < MaximumNarrowViewerControlsBarWidth)
                mode = LayoutTraits.Mode.NarrowViewer;
            else
                mode = LayoutTraits.Mode.Fullscreen;
        }
    
        if (this.host) {
            let LayoutTraitsClass = window.layoutTraitsClasses[this.host.layoutTraitsClassName];
            return new LayoutTraitsClass(mode);
        }
        
        // Default for when host is not defined.
        // FIXME: Always require a host and add a JS implemented TestMediaControlsHost for unit tests.
        return new MacOSLayoutTraits(mode);
    }

    togglePlayback()
    {
        if (this.media.paused || !this.hasPlayed)
            this.media.play().catch(e => {});
        else
            this.media.pause();
    }

    get canShowMediaControlsContextMenu()
    {
        return !!this.host?.showMediaControlsContextMenu;
    }

    showMediaControlsContextMenu(button, options = {})
    {
        if (!this.canShowMediaControlsContextMenu)
            return false;

        let autoHideController = this.controls.autoHideController;

        let willShowContextMenu = this.host.showMediaControlsContextMenu(button.element, {...button.contextMenuOptions, ...options}, () => {
            button.on = false;
            autoHideController.hasSecondaryUIAttached = false;
        });
        if (willShowContextMenu) {
            button.on = true;
            autoHideController.hasSecondaryUIAttached = true;
        }
        return willShowContextMenu;
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
        if (this.media.controls || (this.host && this.host.shouldForceControlsDisplay))
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
        } else if (event.type === "fullscreenchange" && event.currentTarget === this.shadowRoot) {
            this._updateControlsAvailability();
        } else if (event.type === "keydown" && this.isFullscreen && event.key === " ") {
            this.togglePlayback();
            event.preventDefault();
            event.stopPropagation();
        } else if (event.type === "keyup" && this.isFullscreen && event.key === " ") {
            event.preventDefault();
            event.stopPropagation();
        } else if (event.type === "dragstart" && this.isFullscreen)
            event.preventDefault();
        else if (event.type === this.fullscreenChangeEventType)
            this.host?.presentationModeChanged?.();
        else if (event.type === "click" && event.target === this.media) {
            // If the <video> receives a click event, and the <video> is also the target
            // of this event, this means we have clicked outside the border of the
            // media controls <div>, which covers the video content.
            if (this.host && this.host.inWindowFullscreen) {
                this.media.webkitExitFullscreen();
                event.stopPropagation();
            }
        }

        if (event.currentTarget === this.media) {
            if (event.type === "play")
                this.hasPlayed = true;
            this._updateControlsIfNeeded();
            this._updateControlsAvailability();
        }
    }

    // HTMLMediaElement

    setShowingStats(shouldShowStats)
    {
        if (!(this.media instanceof HTMLVideoElement))
            return false;

        if (!shouldShowStats) {
            this._statsContainer?.remove();
            this._statsContainer = null;
            return false;
        }

        if (this._statsContainer)
            return true;

        this._statsContainer = this.container.appendChild(document.createElement("div"))
        this._statsContainer.className = "stats-container";

        let table = this._statsContainer.appendChild(document.createElement("table"));

        function createRow(label) {
            let rowElement = table.appendChild(document.createElement("tr"));

            let labelElement = rowElement.appendChild(document.createElement("th"));
            labelElement.textContent = label;

            let valueElement = rowElement.appendChild(document.createElement("td"));
            return valueElement;
        }
        let sourceValueElement = createRow(UIString("Source"));
        let viewportValueElement = createRow(UIString("Viewport"));
        let framesValueElement = createRow(UIString("Frames"));
        let resolutionValueElement = createRow(UIString("Resolution"));
        let codecsValueElement = createRow(UIString("Codecs"));
        let colorValueElement = createRow(UIString("Color"));

        let update = () => {
            if (!this._statsContainer)
                return;

            let quality = this.media.getVideoPlaybackQuality();
            let videoTrack = this.media.videoTracks.item(this.media.videoTracks.selectedIndex);
            let videoTrackConfiguration = videoTrack.configuration;
            let videoColorSpace = videoTrackConfiguration?.colorSpace;

            sourceValueElement.textContent = UIString(this.host.sourceType ?? "none");
            viewportValueElement.textContent = UIString("%s\u00d7%s", this.controls.width, this.controls.height) + (window.devicePixelRatio !== 1 ? " " + UIString("(%s)", UIString("%s\u00d7", window.devicePixelRatio)) : "");
            framesValueElement.textContent = UIString("%s dropped of %s", quality.droppedVideoFrames, quality.totalVideoFrames);
            resolutionValueElement.textContent = UIString("%s\u00d7%s", videoTrackConfiguration?.width, videoTrackConfiguration?.height) + " " + UIString("(%s)", UIString("%sfps", Math.round(videoTrackConfiguration?.framerate * 1000) / 1000));
            codecsValueElement.textContent = videoTrackConfiguration?.codec;
            colorValueElement.textContent = UIString("%s / %s / %s", videoColorSpace?.primaries, videoColorSpace?.transfer, videoColorSpace?.matrix);

            window.requestAnimationFrame(update);
        };
        update();

        return true;
    }

    deinitialize()
    {
        this.shadowRoot.removeChild(this.container);
        window.removeEventListener("keydown", this);
        if (this.controls)
            this.controls.disable();
        return true;
    }

    reinitialize(shadowRoot, media, host)
    {
        iconService.shadowRoot = shadowRoot;
        this.shadowRootWeakRef = new WeakRef(shadowRoot);
        this.mediaWeakRef = new WeakRef(media);
        this.host = host;
        shadowRoot.appendChild(this.container);
        window.addEventListener("keydown", this);
        if (this.controls)
            this.controls.reenable();
        return true;
    }

    // Private

    _updateControlsIfNeeded()
    {
        const layoutTraits = this.layoutTraits;
        const previousControls = this.controls;
        const ControlsClass = layoutTraits.mediaControlsClass();
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

        if (previousControls)
            previousControls.disable();

        this.controls = new ControlsClass({ mode: layoutTraits.mode });
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

        this._supportingObjects = layoutTraits.supportingObjectClasses().map(SupportClass => new SupportClass(this), this);

        this.controls.shouldUseSingleBarLayout = this.controls instanceof InlineMediaControls && this.isYouTubeEmbedWithTitle;

        if (this.controls instanceof MacOSFullscreenMediaControls)
            window.addEventListener("dragstart", this, true);
        else
            window.removeEventListener("dragstart", this, true);

        if (this.host && this.host.inWindowFullscreen) {
            this._stopPropagationOnInteractionEvents();
            if (!this.host.supportsRewind)
                this.controls.rewindButton.dropped = true;
        }

        this._updateControlsAvailability();

        if (this.host?.needsChromeMediaControlsPseudoElement)
            this.controls.element.setAttribute('useragentpart', '-webkit-media-controls');
    }

    _stopPropagationOnInteractionEvents()
    {
        let clickEvents = ["click", "mousedown", "mouseup", "pointerdown", "pointerup", "touchstart", "touchmove", "touchend"];
        for (let clickEvent of clickEvents) {
            this.controls.element.addEventListener(clickEvent, (event) => {
                event.stopPropagation();
            });
        }
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
    }

    _updateTextTracksClassList()
    {
        if (!this.host)
            return;

        if (this.layoutTraits.isFullscreen)
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
        if (this.layoutTraits.controlsNeverAvailable())
            return false;
        
        if (this.controls instanceof IOSInlineMediaControls && this.media.webkitPresentationMode === "picture-in-picture")
            return false;

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

    // Testing

    set maximumRightContainerButtonCountOverride(count)
    {
        this.controls.maximumRightContainerButtonCountOverride = count;
    }

}
