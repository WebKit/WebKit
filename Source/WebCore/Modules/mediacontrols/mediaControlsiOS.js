function createControls(root, video, host)
{
    return new ControllerIOS(root, video, host);
};

function ControllerIOS(root, video, host)
{
    this.hasWirelessPlaybackTargets = false;
    Controller.call(this, root, video, host);

    this.updateWirelessTargetAvailable();
    this.updateWirelessPlaybackStatus();
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

        this.listenFor(this.base, 'touchstart', this.handleWrapperTouchStart);
        this.stopListeningFor(this.base, 'mousemove', this.handleWrapperMouseMove);
        this.stopListeningFor(this.base, 'mouseout', this.handleWrapperMouseOut);
    },

    UIString: function(s){
        var string = Controller.prototype.UIString.call(this, s);
        if (string)
            return string;

        if (this.localizedStrings[s])
            return this.localizedStrings[s];
        else
            return s; // FIXME: LOG something if string not localized.
    },
    localizedStrings: {
        // FIXME: Move localization to ext strings file <http://webkit.org/b/120956>
        '##AIRPLAY_DEVICE_TYPE##': 'AirPlay',
        '##AIRPLAY_DEVICE_NAME##': 'This video is playing on "##DEVICE_NAME##".',

        '##TVOUT_DEVICE_TYPE##': 'TV Connected',
        '##TVOUT_DEVICE_NAME##': 'This video is playing on the TV.',
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
                deviceType = this.UIString('##AIRPLAY_DEVICE_TYPE##');
                deviceName = this.UIString('##AIRPLAY_DEVICE_NAME##').replace('##DEVICE_NAME##', this.host.externalDeviceDisplayName);
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

        var wirelessTargetPicker = this.controls.wirelessTargetPicker = document.createElement('button');
        wirelessTargetPicker.setAttribute('pseudo', '-webkit-media-controls-wireless-playback-picker-button');
        wirelessTargetPicker.setAttribute('aria-label', this.UIString('Choose Wireless Display'));
        this.listenFor(wirelessTargetPicker, 'click', this.handleWirelessPickerButtonClicked);
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
        this.controls.panel.appendChild(this.controls.timelineBox);
        this.controls.panel.appendChild(this.controls.wirelessTargetPicker);
        this.controls.timelineBox.appendChild(this.controls.currentTime);
        this.controls.timelineBox.appendChild(this.controls.timeline);
        this.controls.timelineBox.appendChild(this.controls.remainingTime);
        if (!this.isAudio())
            this.controls.panel.appendChild(this.controls.fullscreenButton);
    },

    configureFullScreenControls: function() {
        // Do nothing
    },

    updateControls: function() {
        if (this.shouldHaveStartPlaybackButton())
            this.setControlsType(ControllerIOS.StartPlaybackControls);
        else if (this.isFullScreen())
            this.setControlsType(Controller.FullScreenControls);
        else
            this.setControlsType(Controller.InlineControls);

    },

    updateTime: function() {
        Controller.prototype.updateTime.call(this);
        this.updateProgress();
    },

    progressFillStyle: function() {
        return 'rgba(0, 0, 0, 0.5)';
    },

    updateProgress: function() {
        Controller.prototype.updateProgress.call(this);

        var width = this.controls.timeline.offsetWidth;
        var height = this.controls.timeline.offsetHeight;

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
        var intMinutes = Math.floor(absTime / 60).toFixed(0);
        return (time < 0 ? '-' : String()) + String('0' + intMinutes).slice(-1) + ":" + String('00' + intSeconds).slice(-2)
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

        event.stopPropagation();
    },

    handlePlayButtonTouchCancel: function(event) {
        this.controls.playButton.classList.remove('active');
        event.stopPropagation();
    },

    handleWrapperTouchStart: function(event) {
        if (event.target != this.base && event.target != this.controls.wirelessPlaybackStatus)
            return;

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

        event.stopPropagation();
    },

    handleFullscreenTouchCancel: function(event) {
        this.controls.fullscreenButton.classList.remove('active');
        event.stopPropagation();
    },

    handleStartPlaybackButtonTouchStart: function(event) {
        this.controls.fullscreenButton.classList.add('active');
    },

    handleStartPlaybackButtonTouchEnd: function(event) {
        this.controls.fullscreenButton.classList.remove('active');
        if (this.video.error)
            return;

        this.video.play();
        event.stopPropagation();
    },

    handleStartPlaybackButtonTouchCancel: function(event) {
        this.controls.fullscreenButton.classList.remove('active');
        event.stopPropagation();
    },

    handleReadyStateChange: function(event) {
        Controller.prototype.handleReadyStateChange.call(this, event);
        this.updateControls();
    },

    handleWirelessPlaybackChange: function(event) {
        this.updateWirelessPlaybackStatus();
    },

    handleWirelessTargetAvailableChange: function(event) {
        this.hasWirelessPlaybackTargets = event.availability == "available";
        this.updateWirelessTargetAvailable();
    },

    handleWirelessPickerButtonClicked: function(event) {
        this.video.webkitShowPlaybackTargetPicker();
        event.stopPropagation();
    },
};

Object.create(Controller.prototype).extend(ControllerIOS.prototype);
Object.defineProperty(ControllerIOS.prototype, 'constructor', { enumerable:false, value:ControllerIOS });
