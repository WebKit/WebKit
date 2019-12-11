function createControls(root, video, host)
{
    return new ControllerIOS(root, video, host);
};

function ControllerIOS(root, video, host)
{
    this.doingSetup = true;
    this._pageScaleFactor = 1;

    this.timelineContextName = "_webkit-media-controls-timeline-" + host.generateUUID();

    Controller.call(this, root, video, host);

    this.setNeedsTimelineMetricsUpdate();

    this._timelineIsHidden = false;
    this._currentDisplayWidth = 0;
    this.scheduleUpdateLayoutForDisplayedWidth();

    host.controlsDependOnPageScaleFactor = true;
    this.doingSetup = false;
};

/* Enums */
ControllerIOS.StartPlaybackControls = 2;

ControllerIOS.prototype = {
    /* Constants */
    MinimumTimelineWidth: 150,
    ButtonWidth: 42,

    get idiom()
    {
        return "ios";
    },

    createBase: function() {
        Controller.prototype.createBase.call(this);

        var startPlaybackButton = this.controls.startPlaybackButton = document.createElement('div');
        startPlaybackButton.setAttribute('pseudo', '-webkit-media-controls-start-playback-button');
        startPlaybackButton.setAttribute('aria-label', this.UIString('Start Playback'));
        startPlaybackButton.setAttribute('role', 'button');

        var startPlaybackBackground = document.createElement('div');
        startPlaybackBackground.setAttribute('pseudo', '-webkit-media-controls-start-playback-background');
        startPlaybackBackground.classList.add('webkit-media-controls-start-playback-background');
        startPlaybackButton.appendChild(startPlaybackBackground);

        var startPlaybackGlyph = document.createElement('div');
        startPlaybackGlyph.setAttribute('pseudo', '-webkit-media-controls-start-playback-glyph');
        startPlaybackGlyph.classList.add('webkit-media-controls-start-playback-glyph');
        startPlaybackButton.appendChild(startPlaybackGlyph);

        this.listenFor(this.base, 'gesturestart', this.handleBaseGestureStart);
        this.listenFor(this.base, 'gesturechange', this.handleBaseGestureChange);
        this.listenFor(this.base, 'gestureend', this.handleBaseGestureEnd);
        this.listenFor(this.base, 'touchstart', this.handleWrapperTouchStart);
        this.stopListeningFor(this.base, 'mousemove', this.handleWrapperMouseMove);
        this.stopListeningFor(this.base, 'mouseout', this.handleWrapperMouseOut);

        this.listenFor(document, 'visibilitychange', this.handleVisibilityChange);
    },

    shouldHaveStartPlaybackButton: function() {
        var allowsInline = this.host.allowsInlineMediaPlayback;

        if (this.isPlaying || (this.hasPlayed && allowsInline))
            return false;

        if (this.isAudio() && allowsInline)
            return false;

        if (this.doingSetup)
            return true;

        if (this.isFullScreen())
            return false;

        if (!this.video.currentSrc && this.video.error)
            return false;

        if (!this.video.controls && allowsInline)
            return false;

        if (this.video.currentSrc && this.video.error)
            return true;

        return true;
    },

    shouldHaveControls: function() {
        if (this.shouldHaveStartPlaybackButton())
            return false;

        return Controller.prototype.shouldHaveControls.call(this);
    },

    shouldHaveAnyUI: function() {
        return this.shouldHaveStartPlaybackButton() || Controller.prototype.shouldHaveAnyUI.call(this) || this.currentPlaybackTargetIsWireless();
    },

    createControls: function() {
        Controller.prototype.createControls.call(this);

        var panelContainer = this.controls.panelContainer = document.createElement('div');
        panelContainer.setAttribute('pseudo', '-webkit-media-controls-panel-container');

        var wirelessTargetPicker = this.controls.wirelessTargetPicker;
        this.listenFor(wirelessTargetPicker, 'touchstart', this.handleWirelessPickerButtonTouchStart);
        this.listenFor(wirelessTargetPicker, 'touchend', this.handleWirelessPickerButtonTouchEnd);
        this.listenFor(wirelessTargetPicker, 'touchcancel', this.handleWirelessPickerButtonTouchCancel);

        this.listenFor(this.controls.startPlaybackButton, 'touchstart', this.handleStartPlaybackButtonTouchStart);
        this.listenFor(this.controls.startPlaybackButton, 'touchend', this.handleStartPlaybackButtonTouchEnd);
        this.listenFor(this.controls.startPlaybackButton, 'touchcancel', this.handleStartPlaybackButtonTouchCancel);

        this.listenFor(this.controls.panel, 'touchstart', this.handlePanelTouchStart);
        this.listenFor(this.controls.panel, 'touchend', this.handlePanelTouchEnd);
        this.listenFor(this.controls.panel, 'touchcancel', this.handlePanelTouchCancel);
        this.listenFor(this.controls.playButton, 'touchstart', this.handlePlayButtonTouchStart);
        this.listenFor(this.controls.playButton, 'touchend', this.handlePlayButtonTouchEnd);
        this.listenFor(this.controls.playButton, 'touchcancel', this.handlePlayButtonTouchCancel);
        this.listenFor(this.controls.fullscreenButton, 'touchstart', this.handleFullscreenTouchStart);
        this.listenFor(this.controls.fullscreenButton, 'touchend', this.handleFullscreenTouchEnd);
        this.listenFor(this.controls.fullscreenButton, 'touchcancel', this.handleFullscreenTouchCancel);
        this.listenFor(this.controls.pictureInPictureButton, 'touchstart', this.handlePictureInPictureTouchStart);
        this.listenFor(this.controls.pictureInPictureButton, 'touchend', this.handlePictureInPictureTouchEnd);
        this.listenFor(this.controls.pictureInPictureButton, 'touchcancel', this.handlePictureInPictureTouchCancel);
        this.listenFor(this.controls.timeline, 'touchstart', this.handleTimelineTouchStart);
        this.stopListeningFor(this.controls.playButton, 'click', this.handlePlayButtonClicked);

        this.controls.timeline.style.backgroundImage = '-webkit-canvas(' + this.timelineContextName + ')';
    },

    setControlsType: function(type) {
        if (type === this.controlsType)
            return;
        Controller.prototype.setControlsType.call(this, type);

        if (type === ControllerIOS.StartPlaybackControls)
            this.addStartPlaybackControls();
        else
            this.removeStartPlaybackControls();
    },

    addStartPlaybackControls: function() {
        this.base.appendChild(this.controls.startPlaybackButton);
        this.showShowControlsButton(false);
    },

    removeStartPlaybackControls: function() {
        if (this.controls.startPlaybackButton.parentNode)
            this.controls.startPlaybackButton.parentNode.removeChild(this.controls.startPlaybackButton);
    },

    reconnectControls: function()
    {
        Controller.prototype.reconnectControls.call(this);

        if (this.controlsType === ControllerIOS.StartPlaybackControls)
            this.addStartPlaybackControls();
    },

    configureInlineControls: function() {
        this.controls.inlinePlaybackPlaceholder.appendChild(this.controls.inlinePlaybackPlaceholderText);
        this.controls.inlinePlaybackPlaceholderText.appendChild(this.controls.inlinePlaybackPlaceholderTextTop);
        this.controls.inlinePlaybackPlaceholderText.appendChild(this.controls.inlinePlaybackPlaceholderTextBottom);
        this.controls.panel.appendChild(this.controls.playButton);
        this.controls.panel.appendChild(this.controls.statusDisplay);
        this.controls.panel.appendChild(this.controls.timelineBox);
        this.controls.panel.appendChild(this.controls.wirelessTargetPicker);
        if (!this.isLive) {
            this.controls.timelineBox.appendChild(this.controls.currentTime);
            this.controls.timelineBox.appendChild(this.controls.timeline);
            this.controls.timelineBox.appendChild(this.controls.remainingTime);
        }
        if (this.isAudio()) {
            // Hide the scrubber on audio until the user starts playing.
            this.controls.timelineBox.classList.add(this.ClassNames.hidden);
        } else {
            this.updatePictureInPictureButton();
            this.controls.panel.appendChild(this.controls.fullscreenButton);
        }
    },

    configureFullScreenControls: function() {
        // Explicitly do nothing to override base-class behavior.
    },

    controlsAreHidden: function()
    {
        // Controls are only ever actually hidden when they are removed from the tree
        return !this.controls.panelContainer.parentElement;
    },

    addControls: function() {
        this.base.appendChild(this.controls.inlinePlaybackPlaceholder);
        this.base.appendChild(this.controls.panelContainer);
        this.controls.panelContainer.appendChild(this.controls.panelBackground);
        this.controls.panelContainer.appendChild(this.controls.panel);
        this.setNeedsTimelineMetricsUpdate();
    },

    updateControls: function() {
        if (this.shouldHaveStartPlaybackButton())
            this.setControlsType(ControllerIOS.StartPlaybackControls);
        else if (this.presentationMode() === "fullscreen")
            this.setControlsType(Controller.FullScreenControls);
        else
            this.setControlsType(Controller.InlineControls);

        this.updateLayoutForDisplayedWidth();
        this.setNeedsTimelineMetricsUpdate();
    },

    drawTimelineBackground: function() {
        var width = this.timelineWidth * window.devicePixelRatio;
        var height = this.timelineHeight * window.devicePixelRatio;

        if (!width || !height)
            return;

        var played = this.video.currentTime / this.video.duration;
        var buffered = 0;
        var bufferedRanges = this.video.buffered;
        if (bufferedRanges && bufferedRanges.length)
            buffered = Math.max(bufferedRanges.end(bufferedRanges.length - 1), buffered);

        buffered /= this.video.duration;
        buffered = Math.max(buffered, played);

        var ctx = document.getCSSCanvasContext('2d', this.timelineContextName, width, height);

        ctx.clearRect(0, 0, width, height);

        var midY = height / 2;

        // 1. Draw the buffered part and played parts, using
        // solid rectangles that are clipped to the outside of
        // the lozenge.
        ctx.save();
        ctx.beginPath();
        this.addRoundedRect(ctx, 1, midY - 3, width - 2, 6, 3);
        ctx.closePath();
        ctx.clip();
        ctx.fillStyle = "white";
        ctx.fillRect(0, 0, Math.round(width * played) + 2, height);
        ctx.fillStyle = "rgba(0, 0, 0, 0.55)";
        ctx.fillRect(Math.round(width * played) + 2, 0, Math.round(width * (buffered - played)) + 2, height);
        ctx.restore();

        // 2. Draw the outline with a clip path that subtracts the
        // middle of a lozenge. This produces a better result than
        // stroking.
        ctx.save();
        ctx.beginPath();
        this.addRoundedRect(ctx, 1, midY - 3, width - 2, 6, 3);
        this.addRoundedRect(ctx, 2, midY - 2, width - 4, 4, 2);
        ctx.closePath();
        ctx.clip("evenodd");
        ctx.fillStyle = "rgba(0, 0, 0, 0.55)";
        ctx.fillRect(Math.round(width * buffered) + 2, 0, width, height);
        ctx.restore();
    },

    formatTime: function(time) {
        if (isNaN(time))
            time = 0;
        var absTime = Math.abs(time);
        var intSeconds = Math.floor(absTime % 60).toFixed(0);
        var intMinutes = Math.floor((absTime / 60) % 60).toFixed(0);
        var intHours = Math.floor(absTime / (60 * 60)).toFixed(0);
        var sign = time < 0 ? '-' : String();

        if (intHours > 0)
            return sign + intHours + ':' + String('0' + intMinutes).slice(-2) + ":" + String('0' + intSeconds).slice(-2);

        return sign + String('0' + intMinutes).slice(intMinutes >= 10 ? -2 : -1) + ":" + String('0' + intSeconds).slice(-2);
    },

    handlePlayButtonTouchStart: function() {
        this.controls.playButton.classList.add('active');
    },

    handlePlayButtonTouchEnd: function(event) {
        this.controls.playButton.classList.remove('active');

        if (this.canPlay()) {
            this.video.play();
            this.showControls();
        } else
            this.video.pause();

        return true;
    },

    handlePlayButtonTouchCancel: function(event) {
        this.controls.playButton.classList.remove('active');
        return true;
    },

    handleBaseGestureStart: function(event) {
        this.gestureStartTime = new Date();
        // If this gesture started with two fingers inside the video, then
        // don't treat it as a potential zoom, unless we're still waiting
        // to play.
        if (this.mostRecentNumberOfTargettedTouches == 2 && this.controlsType != ControllerIOS.StartPlaybackControls)
            event.preventDefault();
    },

    handleBaseGestureChange: function(event) {
        if (!this.video.controls || this.isAudio() || this.isFullScreen() || this.gestureStartTime === undefined || this.controlsType == ControllerIOS.StartPlaybackControls)
            return;

        var scaleDetectionThreshold = 0.2;
        if (event.scale > 1 + scaleDetectionThreshold || event.scale < 1 - scaleDetectionThreshold)
            delete this.lastDoubleTouchTime;

        if (this.mostRecentNumberOfTargettedTouches == 2 && event.scale >= 1.0)
            event.preventDefault();

        var currentGestureTime = new Date();
        var duration = (currentGestureTime - this.gestureStartTime) / 1000;
        if (!duration)
            return;

        var velocity = Math.abs(event.scale - 1) / duration;

        var pinchOutVelocityThreshold = 2;
        var pinchOutGestureScaleThreshold = 1.25;
        if (velocity < pinchOutVelocityThreshold || event.scale < pinchOutGestureScaleThreshold)
            return;

        delete this.gestureStartTime;
        this.video.webkitEnterFullscreen();
    },

    handleBaseGestureEnd: function(event) {
        delete this.gestureStartTime;
    },

    handleWrapperTouchStart: function(event) {
        if (event.target != this.base && event.target != this.controls.inlinePlaybackPlaceholder)
            return;

        this.mostRecentNumberOfTargettedTouches = event.targetTouches.length;

        if (this.controlsAreHidden() || !this.controls.panel.classList.contains(this.ClassNames.show)) {
            this.showControls();
            this.resetHideControlsTimer();
        } else if (!this.canPlay())
            this.hideControls();
    },

    handlePanelTouchStart: function(event) {
        this.video.style.webkitUserSelect = 'none';
    },

    handlePanelTouchEnd: function(event) {
        this.video.style.removeProperty('-webkit-user-select');
    },

    handlePanelTouchCancel: function(event) {
        this.video.style.removeProperty('-webkit-user-select');
    },

    handleVisibilityChange: function(event) {
        this.updateShouldListenForPlaybackTargetAvailabilityEvent();
    },

    handlePanelTransitionEnd: function(event)
    {
        var opacity = window.getComputedStyle(this.controls.panel).opacity;
        if (!parseInt(opacity) && !this.controlsAlwaysVisible()) {
            this.base.removeChild(this.controls.inlinePlaybackPlaceholder);
            this.base.removeChild(this.controls.panelContainer);
        }
    },

    handleFullscreenButtonClicked: function(event) {
        if ('webkitSetPresentationMode' in this.video) {
            if (this.presentationMode() === 'fullscreen')
                this.video.webkitSetPresentationMode('inline');
            else
                this.video.webkitSetPresentationMode('fullscreen');

            return;
        }

        if (this.isFullScreen())
            this.video.webkitExitFullscreen();
        else
            this.video.webkitEnterFullscreen();
    },

    handleFullscreenTouchStart: function() {
        this.controls.fullscreenButton.classList.add('active');
    },

    handleFullscreenTouchEnd: function(event) {
        this.controls.fullscreenButton.classList.remove('active');

        this.handleFullscreenButtonClicked();

        return true;
    },

    handleFullscreenTouchCancel: function(event) {
        this.controls.fullscreenButton.classList.remove('active');
        return true;
    },

    handlePictureInPictureTouchStart: function() {
        this.controls.pictureInPictureButton.classList.add('active');
    },

    handlePictureInPictureTouchEnd: function(event) {
        this.controls.pictureInPictureButton.classList.remove('active');

        this.handlePictureInPictureButtonClicked();

        return true;
    },

    handlePictureInPictureTouchCancel: function(event) {
        this.controls.pictureInPictureButton.classList.remove('active');
        return true;
    },

    handleStartPlaybackButtonTouchStart: function(event) {
        this.controls.startPlaybackButton.classList.add('active');
        this.controls.startPlaybackButton.querySelector('.webkit-media-controls-start-playback-glyph').classList.add('active');
    },

    handleStartPlaybackButtonTouchEnd: function(event) {
        this.controls.startPlaybackButton.classList.remove('active');
        this.controls.startPlaybackButton.querySelector('.webkit-media-controls-start-playback-glyph').classList.remove('active');

        if (this.video.error)
            return true;

        this.video.play();
        this.canToggleShowControlsButton = true;
        this.updateControls();

        return true;
    },

    handleStartPlaybackButtonTouchCancel: function(event) {
        this.controls.startPlaybackButton.classList.remove('active');
        return true;
    },

    handleTimelineTouchStart: function(event) {
        this.scrubbing = true;
        this.listenFor(this.controls.timeline, 'touchend', this.handleTimelineTouchEnd);
        this.listenFor(this.controls.timeline, 'touchcancel', this.handleTimelineTouchEnd);
    },

    handleTimelineTouchEnd: function(event) {
        this.stopListeningFor(this.controls.timeline, 'touchend', this.handleTimelineTouchEnd);
        this.stopListeningFor(this.controls.timeline, 'touchcancel', this.handleTimelineTouchEnd);
        this.scrubbing = false;
    },

    handleWirelessPickerButtonTouchStart: function() {
        if (!this.video.error)
            this.controls.wirelessTargetPicker.classList.add('active');
    },

    handleWirelessPickerButtonTouchEnd: function(event) {
        this.controls.wirelessTargetPicker.classList.remove('active');
        return this.handleWirelessPickerButtonClicked();
    },

    handleWirelessPickerButtonTouchCancel: function(event) {
        this.controls.wirelessTargetPicker.classList.remove('active');
        return true;
    },

    updateShouldListenForPlaybackTargetAvailabilityEvent: function() {
        if (this.controlsType === ControllerIOS.StartPlaybackControls) {
            this.setShouldListenForPlaybackTargetAvailabilityEvent(false);
            return;
        }

        Controller.prototype.updateShouldListenForPlaybackTargetAvailabilityEvent.call(this);
    },

    updateWirelessTargetPickerButton: function() {
    },

    updateStatusDisplay: function(event)
    {
        this.controls.startPlaybackButton.classList.toggle(this.ClassNames.failed, this.video.error !== null);
        this.controls.startPlaybackButton.querySelector(".webkit-media-controls-start-playback-glyph").classList.toggle(this.ClassNames.failed, this.video.error !== null);
        Controller.prototype.updateStatusDisplay.call(this, event);
    },

    setPlaying: function(isPlaying)
    {
        Controller.prototype.setPlaying.call(this, isPlaying);

        this.updateControls();

        if (isPlaying && this.isAudio())
            this.controls.timelineBox.classList.remove(this.ClassNames.hidden);

        if (isPlaying)
            this.hasPlayed = true;
        else
            this.showControls();
    },

    showControls: function()
    {
        this.updateShouldListenForPlaybackTargetAvailabilityEvent();
        if (!this.video.controls)
            return;

        this.updateForShowingControls();
        if (this.shouldHaveControls() && !this.controls.panelContainer.parentElement) {
            this.base.appendChild(this.controls.inlinePlaybackPlaceholder);
            this.base.appendChild(this.controls.panelContainer);
            this.showShowControlsButton(false);
        }
    },

    setShouldListenForPlaybackTargetAvailabilityEvent: function(shouldListen)
    {
        if (shouldListen && (this.shouldHaveStartPlaybackButton() || this.video.error))
            return;

        Controller.prototype.setShouldListenForPlaybackTargetAvailabilityEvent.call(this, shouldListen);
    },

    shouldReturnVideoLayerToInline: function()
    {
        return this.presentationMode() === 'inline';
    },

    updatePictureInPicturePlaceholder: function(event)
    {
        var presentationMode = this.presentationMode();

        switch (presentationMode) {
            case 'inline':
                this.controls.panelContainer.classList.remove(this.ClassNames.pictureInPicture);
                break;
            case 'picture-in-picture':
                this.controls.panelContainer.classList.add(this.ClassNames.pictureInPicture);
                break;
            default:
                this.controls.panelContainer.classList.remove(this.ClassNames.pictureInPicture);
                break;
        }

        Controller.prototype.updatePictureInPicturePlaceholder.call(this, event);
    },

    // Due to the bad way we are faking inheritance here, in particular the extends method
    // on Controller.prototype, we don't copy getters and setters from the prototype. This
    // means we have to implement them again, here in the subclass.
    // FIXME: Use ES6 classes!

    get scrubbing()
    {
        return Object.getOwnPropertyDescriptor(Controller.prototype, "scrubbing").get.call(this);
    },

    set scrubbing(flag)
    {
        Object.getOwnPropertyDescriptor(Controller.prototype, "scrubbing").set.call(this, flag);
    },

    get pageScaleFactor()
    {
        return this._pageScaleFactor;
    },

    set pageScaleFactor(newScaleFactor)
    {
        if (!newScaleFactor || this._pageScaleFactor === newScaleFactor)
            return;

        this._pageScaleFactor = newScaleFactor;

        var scaleValue = 1 / newScaleFactor;
        var scaleTransform = "scale(" + scaleValue + ")";

        function applyScaleFactorToElement(element) {
            if (scaleValue > 1) {
                element.style.zoom = scaleValue;
                element.style.webkitTransform = "scale(1)";
            } else {
                element.style.zoom = 1;
                element.style.webkitTransform = scaleTransform;
            }
        }

        if (this.controls.startPlaybackButton)
            applyScaleFactorToElement(this.controls.startPlaybackButton);
        if (this.controls.panel) {
            applyScaleFactorToElement(this.controls.panel);
            if (scaleValue > 1) {
                this.controls.panel.style.width = "100%";
                this.controls.timelineBox.style.webkitTextSizeAdjust = (100 * scaleValue) + "%";
            } else {
                var bottomAligment = -2 * scaleValue;
                this.controls.panel.style.bottom = bottomAligment + "px";
                this.controls.panel.style.paddingBottom = -(newScaleFactor * bottomAligment) + "px";
                this.controls.panel.style.width = Math.round(newScaleFactor * 100) + "%";
                this.controls.timelineBox.style.webkitTextSizeAdjust = "auto";
            }
            this.controls.panelBackground.style.height = (50 * scaleValue) + "px";

            this.setNeedsTimelineMetricsUpdate();
            this.updateProgress();
            this.scheduleUpdateLayoutForDisplayedWidth();
        }
    },

};

Object.create(Controller.prototype).extend(ControllerIOS.prototype);
Object.defineProperty(ControllerIOS.prototype, 'constructor', { enumerable: false, value: ControllerIOS });
