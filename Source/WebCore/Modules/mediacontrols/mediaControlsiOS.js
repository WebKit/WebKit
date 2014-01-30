function createControls(root, video, host)
{
    return new ControllerIOS(root, video, host);
};

function ControllerIOS(root, video, host)
{
    Controller.call(this, root, video, host);
};

/* Enums */
ControllerIOS.StartPlaybackControls = 2;

ControllerIOS.prototype = {
    inheritFrom: function(parent) {
        for (var property in parent) {
            if (!this.hasOwnProperty(property))
                this[property] = parent[property];
        }
    },

    addVideoListeners: function() {
        Controller.prototype.addVideoListeners.call(this);

        this.listenFor(this.video, 'webkitbeginfullscreen', this.handleFullscreenChange);
        this.listenFor(this.video, 'webkitendfullscreen', this.handleFullscreenChange);
    },

    removeVideoListeners: function() {
        Controller.prototype.removeVideoListeners.call(this);

        this.stopListeningFor(this.video, 'webkitbeginfullscreen', this.handleFullscreenChange);
        this.stopListeningFor(this.video, 'webkitendfullscreen', this.handleFullscreenChange);
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

    shouldHaveStartPlaybackButton: function() {
        if (this.video.error)
            return true;

        if (this.isFullScreen())
            return false;

        if (!this.host.mediaPlaybackAllowsInline)
            return true;

        if (this.video.readyState <= HTMLMediaElement.HAVE_METADATA)
            return true;

        return false;
    },

    shouldHaveControls: function() {
        if (this.shouldHaveStartPlaybackButton())
            return false;

        return Controller.prototype.shouldHaveControls.call(this);
    },

    shouldHaveAnyUI: function() {
        return this.shouldHaveStartPlaybackButton() || Controller.prototype.shouldHaveAnyUI.call(this);
    },

    createControls: function() {
        Controller.prototype.createControls.call(this);

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
        this.controls.panel.appendChild(this.controls.playButton);
        this.controls.panel.appendChild(this.controls.statusDisplay);
        this.controls.panel.appendChild(this.controls.timelineBox);
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
        if (event.target != this.base)
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

    handleFullscreenButtonClicked: function(event)
    {
        console.trace();
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

};

ControllerIOS.prototype.inheritFrom(Object.create(Controller.prototype));
Object.defineProperty(ControllerIOS.prototype, 'constructor', { enumerable:false, value:ControllerIOS });
