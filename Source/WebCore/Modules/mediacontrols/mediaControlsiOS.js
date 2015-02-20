function createControls(root, video, host)
{
    return new ControllerIOS(root, video, host);
};

function ControllerIOS(root, video, host)
{
    this.doingSetup = true;
    this.hasWirelessPlaybackTargets = false;
    this._pageScaleFactor = 1;
    this.isListeningForPlaybackTargetAvailabilityEvent = false;

    this.timelineContextName = "_webkit-media-controls-timeline-" + ControllerIOS.gLastTimelineId++;

    Controller.call(this, root, video, host);

    this.updateWirelessTargetAvailable();
    this.updateWirelessPlaybackStatus();
    this.setNeedsTimelineMetricsUpdate();

    this._timelineIsHidden = false;
    this._currentDisplayWidth = 0;
    this.scheduleUpdateLayoutForDisplayedWidth();

    host.controlsDependOnPageScaleFactor = true;
    this.doingSetup = false;
};

/* Constants */
ControllerIOS.MinimumTimelineWidth = 220;
ControllerIOS.ButtonWidth = 48;

/* Enums */
ControllerIOS.StartPlaybackControls = 2;

/* Globals */
ControllerIOS.gWirelessImage = 'data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 245"><g fill="#1060FE"><path d="M193.6,6.3v121.6H6.4V6.3H193.6 M199.1,0.7H0.9v132.7h198.2V0.7L199.1,0.7z"/><path d="M43.5,139.3c15.8,8,35.3,12.7,56.5,12.7s40.7-4.7,56.5-12.7H43.5z"/></g><g text-anchor="middle" font-family="Helvetica Neue"><text x="100" y="204" fill="white" font-size="24">##DEVICE_TYPE##</text><text x="100" y="234" fill="#5C5C5C" font-size="21">##DEVICE_NAME##</text></g></svg>';
ControllerIOS.gSimulateWirelessPlaybackTarget = false; // Used for testing when there are no wireless targets.
ControllerIOS.gLastTimelineId = 0;

ControllerIOS.prototype = {
    addVideoListeners: function() {
        Controller.prototype.addVideoListeners.call(this);

        this.listenFor(this.video, 'webkitbeginfullscreen', this.handleFullscreenChange);
        this.listenFor(this.video, 'webkitendfullscreen', this.handleFullscreenChange);
        this.listenFor(this.video, 'webkitcurrentplaybacktargetiswirelesschanged', this.handleWirelessPlaybackChange);
    },

    removeVideoListeners: function() {
        Controller.prototype.removeVideoListeners.call(this);

        this.stopListeningFor(this.video, 'webkitbeginfullscreen', this.handleFullscreenChange);
        this.stopListeningFor(this.video, 'webkitendfullscreen', this.handleFullscreenChange);
        this.stopListeningFor(this.video, 'webkitcurrentplaybacktargetiswirelesschanged', this.handleWirelessPlaybackChange);

        this.setShouldListenForPlaybackTargetAvailabilityEvent(false);
    },

    createBase: function() {
        Controller.prototype.createBase.call(this);

        var startPlaybackButton = this.controls.startPlaybackButton = document.createElement('button');
        startPlaybackButton.setAttribute('pseudo', '-webkit-media-controls-start-playback-button');
        startPlaybackButton.setAttribute('aria-label', this.UIString('Start Playback'));

        this.listenFor(this.base, 'gesturestart', this.handleBaseGestureStart);
        this.listenFor(this.base, 'gesturechange', this.handleBaseGestureChange);
        this.listenFor(this.base, 'gestureend', this.handleBaseGestureEnd);
        this.listenFor(this.base, 'touchstart', this.handleWrapperTouchStart);
        this.stopListeningFor(this.base, 'mousemove', this.handleWrapperMouseMove);
        this.stopListeningFor(this.base, 'mouseout', this.handleWrapperMouseOut);

        this.listenFor(document, 'visibilitychange', this.handleVisibilityChange);
    },

    shouldHaveStartPlaybackButton: function() {
        var allowsInline = this.host.mediaPlaybackAllowsInline;

        if (this.isPlaying || this.hasPlayed)
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

    currentPlaybackTargetIsWireless: function() {
        return ControllerIOS.gSimulateWirelessPlaybackTarget || (('webkitCurrentPlaybackTargetIsWireless' in this.video) && this.video.webkitCurrentPlaybackTargetIsWireless);
    },

    updateWirelessPlaybackStatus: function() {
        if (this.currentPlaybackTargetIsWireless()) {
            var backgroundImageSVG = "url('" + ControllerIOS.gWirelessImage + "')";

            var deviceName = "";
            var deviceType = "";
            var type = this.host.externalDeviceType;
            if (type == "airplay") {
                var externalDeviceDisplayName = this.host.externalDeviceDisplayName;
                if (!externalDeviceDisplayName || externalDeviceDisplayName == "")
                    externalDeviceDisplayName = "Apple TV";
                deviceType = this.UIString('##WIRELESS_PLAYBACK_DEVICE_TYPE##');
                deviceName = this.UIString('##WIRELESS_PLAYBACK_DEVICE_NAME##', '##DEVICE_NAME##', externalDeviceDisplayName);
            } else if (type == "tvout") {
                deviceType = this.UIString('##TVOUT_DEVICE_TYPE##');
                deviceName = this.UIString('##TVOUT_DEVICE_NAME##');
            }

            backgroundImageSVG = backgroundImageSVG.replace('##DEVICE_TYPE##', deviceType);
            backgroundImageSVG = backgroundImageSVG.replace('##DEVICE_NAME##', deviceName);

            this.controls.wirelessPlaybackStatus.style.backgroundImage = backgroundImageSVG;
            this.controls.wirelessPlaybackStatus.setAttribute('aria-label', deviceType + ", " + deviceName);

            this.controls.wirelessPlaybackStatus.classList.remove(this.ClassNames.hidden);
            this.controls.wirelessTargetPicker.classList.add(this.ClassNames.active);
         } else {
            this.controls.wirelessPlaybackStatus.classList.add(this.ClassNames.hidden);
            this.controls.wirelessTargetPicker.classList.remove(this.ClassNames.active);
         }
    },

    updateWirelessTargetAvailable: function() {
        if (ControllerIOS.gSimulateWirelessPlaybackTarget || this.hasWirelessPlaybackTargets)
            this.controls.wirelessTargetPicker.classList.remove(this.ClassNames.hidden);
        else
            this.controls.wirelessTargetPicker.classList.add(this.ClassNames.hidden);
    },

    createControls: function() {
        Controller.prototype.createControls.call(this);

        var wirelessPlaybackStatus = this.controls.wirelessPlaybackStatus = document.createElement('div');
        wirelessPlaybackStatus.setAttribute('pseudo', '-webkit-media-controls-wireless-playback-status');
        wirelessPlaybackStatus.classList.add(this.ClassNames.hidden);

        var panelCompositedParent = this.controls.panelCompositedParent = document.createElement('div');
        panelCompositedParent.setAttribute('pseudo', '-webkit-media-controls-panel-composited-parent');

        var spacer = this.controls.spacer = document.createElement('div');
        spacer.setAttribute('pseudo', '-webkit-media-controls-spacer');
        spacer.classList.add(this.ClassNames.hidden);

        var wirelessTargetPicker = this.controls.wirelessTargetPicker = document.createElement('button');
        wirelessTargetPicker.setAttribute('pseudo', '-webkit-media-controls-wireless-playback-picker-button');
        wirelessTargetPicker.setAttribute('aria-label', this.UIString('Choose Wireless Display'));
        this.listenFor(wirelessTargetPicker, 'touchstart', this.handleWirelessPickerButtonTouchStart);
        this.listenFor(wirelessTargetPicker, 'touchend', this.handleWirelessPickerButtonTouchEnd);
        this.listenFor(wirelessTargetPicker, 'touchcancel', this.handleWirelessPickerButtonTouchCancel);

        if (!ControllerIOS.gSimulateWirelessPlaybackTarget)
            wirelessTargetPicker.classList.add(this.ClassNames.hidden);

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

        this.updateShouldListenForPlaybackTargetAvailabilityEvent();
    },

    addStartPlaybackControls: function() {
        this.base.appendChild(this.controls.startPlaybackButton);
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
        this.base.appendChild(this.controls.wirelessPlaybackStatus);

        this.controls.panel.appendChild(this.controls.playButton);
        this.controls.panel.appendChild(this.controls.statusDisplay);
        this.controls.panel.appendChild(this.controls.spacer);
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
            this.controls.panel.appendChild(this.controls.fullscreenButton);
        }
    },

    configureFullScreenControls: function() {
        // Explicitly do nothing to override base-class behavior.
    },

    hideControls: function() {
        Controller.prototype.hideControls.call(this);
        this.updateShouldListenForPlaybackTargetAvailabilityEvent();
    },

    showControls: function() {
        this.updateLayoutForDisplayedWidth();
        this.updateTime(true);
        this.updateProgress(true);
        Controller.prototype.showControls.call(this);
        this.updateShouldListenForPlaybackTargetAvailabilityEvent();
    },

    addControls: function() {
        this.base.appendChild(this.controls.panelCompositedParent);
        this.controls.panelCompositedParent.appendChild(this.controls.panel);
        this.setNeedsTimelineMetricsUpdate();
    },

    updateControls: function() {
        if (this.shouldHaveStartPlaybackButton())
            this.setControlsType(ControllerIOS.StartPlaybackControls);
        else
            this.setControlsType(Controller.InlineControls);

        this.updateLayoutForDisplayedWidth();
        this.setNeedsTimelineMetricsUpdate();
    },

    updateTime: function(forceUpdate) {
        Controller.prototype.updateTime.call(this, forceUpdate);
        this.updateProgress();
    },

    addRoundedRect: function(ctx, x, y, width, height, radius) {
        ctx.moveTo(x + radius, y);
        ctx.arcTo(x + width, y, x + width, y + radius, radius);
        ctx.lineTo(x + width, y + height - radius);
        ctx.arcTo(x + width, y + height, x + width - radius, y + height, radius);
        ctx.lineTo(x + radius, y + height);
        ctx.arcTo(x, y + height, x, y + height - radius, radius);
        ctx.lineTo(x, y + radius);
        ctx.arcTo(x, y, x + radius, y, radius);
    },

    drawTimelineBackground: function() {
        var width = this.timelineWidth * window.devicePixelRatio;
        var height = this.timelineHeight * window.devicePixelRatio;

        if (!width || !height)
            return;

        var played = this.video.currentTime / this.video.duration;
        var buffered = 0;
        for (var i = 0, end = this.video.buffered.length; i < end; ++i)
            buffered = Math.max(this.video.buffered.end(i), buffered);

        buffered /= this.video.duration;

        var ctx = document.getCSSCanvasContext('2d', this.timelineContextName, width, height);

        ctx.clearRect(0, 0, width, height);

        var midY = height / 2;

        // 1. Draw the outline with a clip path that subtracts the
        // middle of a lozenge. This produces a better result than
        // stroking when we come to filling the parts below.
        ctx.save();
        ctx.beginPath();
        this.addRoundedRect(ctx, 1, midY - 3, width - 2, 6, 3);
        this.addRoundedRect(ctx, 2, midY - 2, width - 4, 4, 2);
        ctx.closePath();
        ctx.clip("evenodd");
        ctx.fillStyle = "black";
        ctx.fillRect(0, 0, width, height);
        ctx.restore();

        // 2. Draw the buffered part and played parts, using
        // solid rectangles that are clipped to the outside of
        // the lozenge.
        ctx.save();
        ctx.beginPath();
        this.addRoundedRect(ctx, 1, midY - 3, width - 2, 6, 3);
        ctx.closePath();
        ctx.clip();
        ctx.fillStyle = "black";
        ctx.fillRect(0, 0, Math.round(width * buffered) + 2, height);
        ctx.fillStyle = "white";
        ctx.fillRect(0, 0, Math.round(width * played) + 2, height);
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
            return sign + intHours + ':' + String('00' + intMinutes).slice(-2) + ":" + String('00' + intSeconds).slice(-2);

        return sign + String('00' + intMinutes).slice(-2) + ":" + String('00' + intSeconds).slice(-2);
    },

    handleTimelineChange: function(event) {
        Controller.prototype.handleTimelineChange.call(this);
        this.updateProgress();
    },

    handlePlayButtonTouchStart: function() {
        this.controls.playButton.classList.add('active');
    },

    handlePlayButtonTouchEnd: function(event) {
        this.controls.playButton.classList.remove('active');

        if (this.canPlay())
            this.video.play();
        else
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
        if (event.target != this.base && event.target != this.controls.wirelessPlaybackStatus)
            return;

        this.mostRecentNumberOfTargettedTouches = event.targetTouches.length;

        if (this.controlsAreHidden()) {
            this.showControls();
            if (this.hideTimer)
                clearTimeout(this.hideTimer);
            this.hideTimer = setTimeout(this.hideControls.bind(this), this.HideControlsDelay);
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

    isFullScreen: function()
    {
        return this.video.webkitDisplayingFullscreen;
    },

    handleFullscreenButtonClicked: function(event) {
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

    handleStartPlaybackButtonTouchStart: function(event) {
        this.controls.startPlaybackButton.classList.add('active');
    },

    handleStartPlaybackButtonTouchEnd: function(event) {
        this.controls.startPlaybackButton.classList.remove('active');
        if (this.video.error)
            return true;

        this.video.play();
        this.updateControls();

        return true;
    },

    handleStartPlaybackButtonTouchCancel: function(event) {
        this.controls.startPlaybackButton.classList.remove('active');
        return true;
    },

    handleReadyStateChange: function(event) {
        Controller.prototype.handleReadyStateChange.call(this, event);
        this.updateControls();
    },

    handleWirelessPlaybackChange: function(event) {
        this.updateWirelessPlaybackStatus();
        this.setNeedsTimelineMetricsUpdate();
    },

    handleWirelessTargetAvailableChange: function(event) {
        var wirelessPlaybackTargetsAvailable = event.availability == "available";
        if (this.hasWirelessPlaybackTargets === wirelessPlaybackTargetsAvailable)
            return;

        this.hasWirelessPlaybackTargets = wirelessPlaybackTargetsAvailable;
        this.updateWirelessTargetAvailable();
        this.setNeedsTimelineMetricsUpdate();
    },

    handleWirelessPickerButtonTouchStart: function() {
        if (!this.video.error)
            this.controls.wirelessTargetPicker.classList.add('active');
    },

    handleWirelessPickerButtonTouchEnd: function(event) {
        this.controls.wirelessTargetPicker.classList.remove('active');
        this.video.webkitShowPlaybackTargetPicker();
        return true;
    },

    handleWirelessPickerButtonTouchCancel: function(event) {
        this.controls.wirelessTargetPicker.classList.remove('active');
        return true;
    },

    updateShouldListenForPlaybackTargetAvailabilityEvent: function() {
        var shouldListen = true;
        if (this.video.error)
            shouldListen = false;
        if (this.controlsType === ControllerIOS.StartPlaybackControls)
            shouldListen = false;
        if (!this.isAudio() && !this.video.paused && this.controlsAreHidden())
            shouldListen = false;
        if (document.hidden)
            shouldListen = false;

        this.setShouldListenForPlaybackTargetAvailabilityEvent(shouldListen);
    },

    updateStatusDisplay: function(event)
    {
        this.updateShouldListenForPlaybackTargetAvailabilityEvent();
        this.controls.startPlaybackButton.classList.toggle(this.ClassNames.failed, this.video.error !== null);
        Controller.prototype.updateStatusDisplay.call(this, event);
    },

    setPlaying: function(isPlaying)
    {
        Controller.prototype.setPlaying.call(this, isPlaying);

        this.updateControls();

        if (isPlaying && this.isAudio() && !this._timelineIsHidden) {
            this.controls.timelineBox.classList.remove(this.ClassNames.hidden);
            this.controls.spacer.classList.add(this.ClassNames.hidden);
        }

        if (isPlaying)
            this.hasPlayed = true;
        else
            this.showControls();
    },

    setShouldListenForPlaybackTargetAvailabilityEvent: function(shouldListen)
    {
        if (!window.WebKitPlaybackTargetAvailabilityEvent || this.isListeningForPlaybackTargetAvailabilityEvent == shouldListen)
            return;

        if (shouldListen && (this.shouldHaveStartPlaybackButton() || this.video.error))
            return;

        this.isListeningForPlaybackTargetAvailabilityEvent = shouldListen;
        if (shouldListen)
            this.listenFor(this.video, 'webkitplaybacktargetavailabilitychanged', this.handleWirelessTargetAvailableChange);
        else
            this.stopListeningFor(this.video, 'webkitplaybacktargetavailabilitychanged', this.handleWirelessTargetAvailableChange);
    },

    get pageScaleFactor()
    {
        return this._pageScaleFactor;
    },

    set pageScaleFactor(newScaleFactor)
    {
        if (this._pageScaleFactor === newScaleFactor)
            return;

        this._pageScaleFactor = newScaleFactor;

        if (newScaleFactor) {
            var scaleValue = 1 / newScaleFactor;
            var scaleTransform = "scale(" + scaleValue + ")";
            if (this.controls.startPlaybackButton)
                this.controls.startPlaybackButton.style.webkitTransform = scaleTransform;
            if (this.controls.panel) {
                var bottomAligment = -2 * scaleValue;
                this.controls.panel.style.bottom = bottomAligment + "px";
                this.controls.panel.style.paddingBottom = -(newScaleFactor * bottomAligment) + "px";
                this.controls.panel.style.width = Math.ceil(newScaleFactor * 100) + "%";
                this.controls.panel.style.webkitTransform = scaleTransform;
                this.setNeedsTimelineMetricsUpdate();
                this.updateProgress();
                this.scheduleUpdateLayoutForDisplayedWidth();
            }
        }
    },

    scheduleUpdateLayoutForDisplayedWidth: function ()
    {
        setTimeout(function () {
            this.updateLayoutForDisplayedWidth();
        }.bind(this), 0);
    },

    updateLayoutForDisplayedWidth: function()
    {
        if (!this.controls || !this.controls.panel)
            return;

        var visibleWidth = this.controls.panel.getBoundingClientRect().width * this._pageScaleFactor;
        if (visibleWidth <= 0 || visibleWidth == this._currentDisplayWidth)
            return;

        this._currentDisplayWidth = visibleWidth;

        // We need to work out how many right-hand side buttons are available.
        this.updateWirelessTargetAvailable();
        this.updateFullscreenButton();

        var visibleButtonWidth = ControllerIOS.ButtonWidth; // We always try to show the fullscreen button.

        if (!this.controls.wirelessTargetPicker.classList.contains(this.ClassNames.hidden))
            visibleButtonWidth += ControllerIOS.ButtonWidth;

        // Check if there is enough room for the scrubber.
        if ((visibleWidth - visibleButtonWidth) < ControllerIOS.MinimumTimelineWidth) {
            this.controls.timelineBox.classList.add(this.ClassNames.hidden);
            this.controls.spacer.classList.remove(this.ClassNames.hidden);
            this._timelineIsHidden = true;
        } else {
            if (!this.isAudio() || this.hasPlayed) {
                this.controls.timelineBox.classList.remove(this.ClassNames.hidden);
                this.controls.spacer.classList.add(this.ClassNames.hidden);
                this._timelineIsHidden = false;
            } else
                this.controls.spacer.classList.remove(this.ClassNames.hidden);
        }

        // Drop the airplay button if there isn't enough space.
        if (visibleWidth < visibleButtonWidth) {
            this.controls.wirelessTargetPicker.classList.add(this.ClassNames.hidden);
            visibleButtonWidth -= ControllerIOS.ButtonWidth;
        }

        // And finally, drop the fullscreen button as a last resort.
        if (visibleWidth < visibleButtonWidth) {
            this.controls.fullscreenButton.classList.add(this.ClassNames.hidden);
            visibleButtonWidth -= ControllerIOS.ButtonWidth;
        } else
            this.controls.fullscreenButton.classList.remove(this.ClassNames.hidden);
    },

};

Object.create(Controller.prototype).extend(ControllerIOS.prototype);
Object.defineProperty(ControllerIOS.prototype, 'constructor', { enumerable: false, value: ControllerIOS });
