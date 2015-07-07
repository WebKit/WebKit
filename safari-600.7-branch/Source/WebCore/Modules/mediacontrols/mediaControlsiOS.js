function createControls(root, video, host)
{
    return new ControllerIOS(root, video, host);
};

function ControllerIOS(root, video, host)
{
    this.hasWirelessPlaybackTargets = false;
    this._pageScaleFactor = 1;
    Controller.call(this, root, video, host);

    this.updateWirelessTargetAvailable();
    this.updateWirelessPlaybackStatus();
    this.setNeedsTimelineMetricsUpdate();

    host.controlsDependOnPageScaleFactor = true;
};

/* Enums */
ControllerIOS.StartPlaybackControls = 2;

/* Globals */
ControllerIOS.gWirelessImage = 'data:image/svg+xml;utf8,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 245"><g fill="#1060FE"><path d="M193.6,6.3v121.6H6.4V6.3H193.6 M199.1,0.7H0.9v132.7h198.2V0.7L199.1,0.7z"/><path d="M43.5,139.3c15.8,8,35.3,12.7,56.5,12.7s40.7-4.7,56.5-12.7H43.5z"/></g><g text-anchor="middle" font-family="Helvetica Neue"><text x="100" y="204" fill="white" font-size="24">##DEVICE_TYPE##</text><text x="100" y="234" fill="#5C5C5C" font-size="21">##DEVICE_NAME##</text></g></svg>';
ControllerIOS.gSimulateWirelessPlaybackTarget = false; // Used for testing when there are no wireless targets.

ControllerIOS.prototype = {
    addVideoListeners: function() {
        Controller.prototype.addVideoListeners.call(this);

        this.listenFor(this.video, 'webkitbeginfullscreen', this.handleFullscreenChange);
        this.listenFor(this.video, 'webkitendfullscreen', this.handleFullscreenChange);

        if (window.WebKitPlaybackTargetAvailabilityEvent) {
            this.listenFor(this.video, 'webkitcurrentplaybacktargetiswirelesschanged', this.handleWirelessPlaybackChange);
            this.listenFor(this.video, 'webkitplaybacktargetavailabilitychanged', this.handleWirelessTargetAvailableChange);
        }
    },

    removeVideoListeners: function() {
        Controller.prototype.removeVideoListeners.call(this);

        this.stopListeningFor(this.video, 'webkitbeginfullscreen', this.handleFullscreenChange);
        this.stopListeningFor(this.video, 'webkitendfullscreen', this.handleFullscreenChange);

        if (window.WebKitPlaybackTargetAvailabilityEvent) {
            this.stopListeningFor(this.video, 'webkitcurrentplaybacktargetiswirelesschanged', this.handleWirelessPlaybackChange);
            this.stopListeningFor(this.video, 'webkitplaybacktargetavailabilitychanged', this.handleWirelessTargetAvailableChange);
        }
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
    },

    shouldHaveStartPlaybackButton: function() {
        var allowsInline = this.host.mediaPlaybackAllowsInline;

        if (this.isAudio() && allowsInline)
            return false;

        if (this.isFullScreen())
            return false;

        if (!this.video.currentSrc && this.video.error)
            return false;

        if (!this.video.controls && allowsInline)
            return false;

        if (this.video.currentSrc && this.video.error)
            return true;

        if (!this.host.userGestureRequired && allowsInline)
            return false;

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
                deviceType = this.UIString('##WIRELESS_PLAYBACK_DEVICE_TYPE##');
                deviceName = this.UIString('##WIRELESS_PLAYBACK_DEVICE_NAME##', '##DEVICE_NAME##', this.host.externalDeviceDisplayName || "Apple TV");
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

        var panelCompositedParent = this.controls.panelCompositedParent = document.createElement('div');
        panelCompositedParent.setAttribute('pseudo', '-webkit-media-controls-panel-composited-parent');

        var wirelessPlaybackStatus = this.controls.wirelessPlaybackStatus = document.createElement('div');
        wirelessPlaybackStatus.setAttribute('pseudo', '-webkit-media-controls-wireless-playback-status');
        wirelessPlaybackStatus.classList.add(this.ClassNames.hidden);

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
    },

    removeStartPlaybackControls: function() {
        if (this.controls.startPlaybackButton.parentNode)
            this.controls.startPlaybackButton.parentNode.removeChild(this.controls.startPlaybackButton);
    },

    configureInlineControls: function() {
        this.base.appendChild(this.controls.wirelessPlaybackStatus);

        this.controls.panel.appendChild(this.controls.playButton);
        this.controls.panel.appendChild(this.controls.statusDisplay);
        this.controls.panel.appendChild(this.controls.timelineBox);
        this.controls.panel.appendChild(this.controls.wirelessTargetPicker);
        if (!this.isLive) {
            this.controls.timelineBox.appendChild(this.controls.currentTime);
            this.controls.timelineBox.appendChild(this.controls.timeline);
            this.controls.timelineBox.appendChild(this.controls.remainingTime);
        }
        if (!this.isAudio())
            this.controls.panel.appendChild(this.controls.fullscreenButton);
    },

    configureFullScreenControls: function() {
        // Do nothing
    },

    addControls: function() {
        this.base.appendChild(this.controls.panelCompositedParent);
        this.controls.panelCompositedParent.appendChild(this.controls.panel);
        this.setNeedsTimelineMetricsUpdate();
    },

    updateControls: function() {
        if (this.shouldHaveStartPlaybackButton())
            this.setControlsType(ControllerIOS.StartPlaybackControls);
        else if (this.isFullScreen())
            this.setControlsType(Controller.FullScreenControls);
        else
            this.setControlsType(Controller.InlineControls);

        this.setNeedsTimelineMetricsUpdate();
    },

    updateTime: function() {
        Controller.prototype.updateTime.call(this);
        this.updateProgress();
    },

    progressFillStyle: function() {
        return 'rgba(0, 0, 0, 0.5)';
    },

    updateProgress: function(forceUpdate) {
        Controller.prototype.updateProgress.call(this, forceUpdate);

        if (!forceUpdate && this.controlsAreHidden())
            return;

        var width = this.timelineWidth;
        var height = this.timelineHeight;

        // Magic number, matching the value for ::-webkit-media-controls-timeline::-webkit-slider-thumb
        // in mediaControlsiOS.css. Since we cannot ask the thumb for its offsetWidth as it's in its own
        // shadow dom, just hard-code the value.
        var thumbWidth = 16;
        var endX = thumbWidth / 2 + (width - thumbWidth) * this.video.currentTime / this.video.duration;

        var context = document.getCSSCanvasContext('2d', 'timeline-' + this.timelineID, width, height);
        context.fillStyle = 'white';
        context.fillRect(0, 0, endX, height);
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

        return sign + String('00' + intMinutes).slice(intMinutes >= 10 ? -2 : -1) + ":" + String('00' + intSeconds).slice(-2);
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

        if (this.mostRecentNumberOfTargettedTouches == 2 && event.scale >= 1.0)
            event.preventDefault();

        var currentGestureTime = new Date();
        var duration = (currentGestureTime - this.gestureStartTime) / 1000;
        if (!duration)
            return;

        var velocity = Math.abs(event.scale - 1) / duration;

        if (event.scale < 1.25 || velocity < 2)
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
        this.controls.fullscreenButton.classList.add('active');
    },

    handleStartPlaybackButtonTouchEnd: function(event) {
        this.controls.fullscreenButton.classList.remove('active');
        if (this.video.error)
            return true;

        this.video.play();

        return true;
    },

    handleStartPlaybackButtonTouchCancel: function(event) {
        this.controls.fullscreenButton.classList.remove('active');
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

    updateStatusDisplay: function(event)
    {
        this.controls.startPlaybackButton.classList.toggle(this.ClassNames.failed, this.video.error !== null);
        Controller.prototype.updateStatusDisplay.call(this, event);
    },

    setPlaying: function(isPlaying)
    {
        this.updateControls();
        Controller.prototype.setPlaying.call(this, isPlaying);
    },

    get pageScaleFactor() {
        return this._pageScaleFactor;
    },

    set pageScaleFactor(newScaleFactor) {
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
            }
        }
    }
};

Object.create(Controller.prototype).extend(ControllerIOS.prototype);
Object.defineProperty(ControllerIOS.prototype, 'constructor', { enumerable: false, value: ControllerIOS });
