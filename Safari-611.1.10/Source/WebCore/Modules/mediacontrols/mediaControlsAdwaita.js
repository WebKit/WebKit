function createControls(root, video, host)
{
    return new Controller(root, video, host);
};

function Controller(root, video, host)
{
    this.video = video;
    this.root = root;
    this.host = host;
    this.controls = {};
    this.listeners = {};
    this.isLive = false;
    this.hasVisualMedia = false;

    this.addVideoListeners();
    this.createBase();
    this.createControls();
    this.updateBase();
    this.updateControls();
    this.updateDuration();
    this.updateTime();
    this.updatePlaying();
    this.updateCaptionButton();
    this.updateCaptionContainer();
    this.updateFullscreenButton();
    this.updateVolume();
    this.updateHasAudio();
    this.updateHasVideo();
};

Controller.prototype = {

    /* Constants */
    HandledVideoEvents: {
        emptied: 'handleReadyStateChange',
        loadedmetadata: 'handleReadyStateChange',
        loadeddata: 'handleReadyStateChange',
        canplay: 'handleReadyStateChange',
        canplaythrough: 'handleReadyStateChange',
        timeupdate: 'handleTimeUpdate',
        durationchange: 'handleDurationChange',
        playing: 'handlePlay',
        pause: 'handlePause',
        volumechange: 'handleVolumeChange',
        webkitfullscreenchange: 'handleFullscreenChange',
        webkitbeginfullscreen: 'handleFullscreenChange',
        webkitendfullscreen: 'handleFullscreenChange',
    },
    HideControlsDelay: 4 * 1000,
    ClassNames: {
        exit: 'exit',
        hidden: 'hidden',
        hiding: 'hiding',
        muteBox: 'mute-box',
        muted: 'muted',
        paused: 'paused',
        selected: 'selected',
        show: 'show',
        noVideo: 'no-video',
        noDuration: 'no-duration',
        down: 'down',
        out: 'out',
    },
    KeyCodes: {
        enter: 13,
        escape: 27,
        space: 32,
        left: 37,
        up: 38,
        right: 39,
        down: 40
    },

    listenFor: function(element, eventName, handler, useCapture)
    {
        if (typeof useCapture === 'undefined')
            useCapture = false;

        if (!(this.listeners[eventName] instanceof Array))
            this.listeners[eventName] = [];
        this.listeners[eventName].push({element:element, handler:handler, useCapture:useCapture});
        element.addEventListener(eventName, this, useCapture);
    },

    stopListeningFor: function(element, eventName, handler, useCapture)
    {
        if (typeof useCapture === 'undefined')
            useCapture = false;

        if (!(this.listeners[eventName] instanceof Array))
            return;

        this.listeners[eventName] = this.listeners[eventName].filter(function(entry) {
            return !(entry.element === element && entry.handler === handler && entry.useCapture === useCapture);
        });
        element.removeEventListener(eventName, this, useCapture);
    },

    addVideoListeners: function()
    {
        for (var name in this.HandledVideoEvents) {
            this.listenFor(this.video, name, this.HandledVideoEvents[name]);
        };

        /* text tracks */
        this.listenFor(this.video.textTracks, 'change', this.handleTextTrackChange);
        this.listenFor(this.video.textTracks, 'addtrack', this.handleTextTrackAdd);
        this.listenFor(this.video.textTracks, 'removetrack', this.handleTextTrackRemove);

        /* audio tracks */
        this.listenFor(this.video.audioTracks, 'change', this.updateHasAudio);
        this.listenFor(this.video.audioTracks, 'addtrack', this.updateHasAudio);
        this.listenFor(this.video.audioTracks, 'removetrack', this.updateHasAudio);

        /* video tracks */
        this.listenFor(this.video.videoTracks, 'change', this.updateHasVideo);
        this.listenFor(this.video.videoTracks, 'addtrack', this.updateHasVideo);
        this.listenFor(this.video.videoTracks, 'removetrack', this.updateHasVideo);

        /* controls attribute */
        this.controlsObserver = new MutationObserver(this.handleControlsChange.bind(this));
        this.controlsObserver.observe(this.video, { attributes: true, attributeFilter: ['controls'] });
    },

    removeVideoListeners: function()
    {
        for (var name in this.HandledVideoEvents) {
            this.stopListeningFor(this.video, name, this.HandledVideoEvents[name]);
        };

        /* text tracks */
        this.stopListeningFor(this.video.textTracks, 'change', this.handleTextTrackChange);
        this.stopListeningFor(this.video.textTracks, 'addtrack', this.handleTextTrackAdd);
        this.stopListeningFor(this.video.textTracks, 'removetrack', this.handleTextTrackRemove);

        /* audio tracks */
        this.stopListeningFor(this.video.audioTracks, 'change', this.updateHasAudio);
        this.stopListeningFor(this.video.audioTracks, 'addtrack', this.updateHasAudio);
        this.stopListeningFor(this.video.audioTracks, 'removetrack', this.updateHasAudio);

        /* video tracks */
        this.stopListeningFor(this.video.videoTracks, 'change', this.updateHasVideo);
        this.stopListeningFor(this.video.videoTracks, 'addtrack', this.updateHasVideo);
        this.stopListeningFor(this.video.videoTracks, 'removetrack', this.updateHasVideo);

        /* controls attribute */
        this.controlsObserver.disconnect();
        delete(this.controlsObserver);
    },

    handleEvent: function(event)
    {
        var preventDefault = false;

        try {
            if (event.target === this.video) {
                var handlerName = this.HandledVideoEvents[event.type];
                var handler = this[handlerName];
                if (handler && handler instanceof Function)
                    handler.call(this, event);
            }

            if (!(this.listeners[event.type] instanceof Array))
                return;

            this.listeners[event.type].forEach(function(entry) {
                if (entry.element === event.currentTarget && entry.handler instanceof Function)
                    preventDefault |= entry.handler.call(this, event);
            }, this);
        } catch(e) {
            if (window.console)
                console.error(e);
        }

        if (preventDefault) {
            event.stopPropagation();
            event.preventDefault();
        }
    },

    createBase: function()
    {
        var base = this.base = document.createElement('div');
        base.setAttribute('pseudo', '-webkit-media-controls');
        this.listenFor(base, 'mousemove', this.handleWrapperMouseMove);
        this.listenFor(base, 'mouseout', this.handleWrapperMouseOut);
        if (this.host.textTrackContainer)
            base.appendChild(this.host.textTrackContainer);
    },

    isAudio: function()
    {
        return this.video instanceof HTMLAudioElement;
    },

    isFullScreen: function()
    {
        return this.video.webkitDisplayingFullscreen;
    },

    shouldHaveControls: function()
    {
        if (!this.isAudio() && !this.host.allowsInlineMediaPlayback)
            return true;

        return this.video.controls || this.isFullScreen();
    },

    updateBase: function()
    {
        if (this.shouldHaveControls() || (this.video.textTracks && this.video.textTracks.length)) {
            if (!this.base.parentNode) {
                this.root.appendChild(this.base);
            }
        } else {
            if (this.base.parentNode) {
                this.base.parentNode.removeChild(this.base);
            }
        }
    },

    createControls: function()
    {
        var enclosure = this.controls.enclosure = document.createElement('div');
        enclosure.setAttribute('pseudo', '-webkit-media-controls-enclosure');

        var panel = this.controls.panel = document.createElement('div');
        panel.setAttribute('pseudo', '-webkit-media-controls-panel');
        panel.setAttribute('aria-label', (this.isAudio() ? 'Audio Playback' : 'Video Playback'));
        panel.setAttribute('role', 'toolbar');
        this.listenFor(panel, 'mousedown', this.handlePanelMouseDown);
        this.listenFor(panel, 'transitionend', this.handlePanelTransitionEnd);
        this.listenFor(panel, 'click', this.handlePanelClick);
        this.listenFor(panel, 'dblclick', this.handlePanelClick);

        var playButton = this.controls.playButton = document.createElement('button');
        playButton.setAttribute('pseudo', '-webkit-media-controls-play-button');
        playButton.setAttribute('aria-label', 'Play');
        this.listenFor(playButton, 'click', this.handlePlayButtonClicked);

        var timelineBox = this.controls.timelineBox = document.createElement('div');
        timelineBox.setAttribute('pseudo', '-webkit-media-controls-timeline-container');

        var currentTime = this.controls.currentTime = document.createElement('div');
        currentTime.setAttribute('pseudo', '-webkit-media-controls-current-time-display');
        currentTime.setAttribute('aria-label', 'Elapsed');
        currentTime.setAttribute('role', 'timer');

        var timeline = this.controls.timeline = document.createElement('input');
        timeline.setAttribute('pseudo', '-webkit-media-controls-timeline');
        timeline.setAttribute('aria-label', 'Duration');
        timeline.type = 'range';
        timeline.value = 0;
        timeline.step = .01;
        this.listenFor(timeline, 'input', this.handleTimelineChange);
        this.listenFor(timeline, 'mouseup', this.handleTimelineMouseUp);

        var muteBox = this.controls.muteBox = document.createElement('div');
        muteBox.classList.add(this.ClassNames.muteBox);
        this.listenFor(muteBox, 'mouseout', this.handleVolumeBoxMouseOut);

        var muteButton = this.controls.muteButton = document.createElement('button');
        muteButton.setAttribute('pseudo', '-webkit-media-controls-mute-button');
        muteButton.setAttribute('aria-label', 'Mute');
        this.listenFor(muteButton, 'click', this.handleMuteButtonClicked);
        this.listenFor(muteButton, 'mouseover', this.handleMuteButtonMouseOver);

        var volumeBox = this.controls.volumeBox = document.createElement('div');
        volumeBox.setAttribute('pseudo', '-webkit-media-controls-volume-slider-container');
        volumeBox.classList.add(this.ClassNames.hiding);
        this.listenFor(volumeBox, 'mouseover', this.handleMuteButtonMouseOver);

        var volume = this.controls.volume = document.createElement('input');
        volume.setAttribute('pseudo', '-webkit-media-controls-volume-slider');
        volume.setAttribute('aria-label', 'Volume');
        volume.type = 'range';
        volume.min = 0;
        volume.max = 1;
        volume.step = .01;
        this.listenFor(volume, 'input', this.handleVolumeSliderInput);
        this.listenFor(volume, 'mouseover', this.handleMuteButtonMouseOver);

        var captionButton = this.controls.captionButton = document.createElement('button');
        captionButton.setAttribute('pseudo', '-webkit-media-controls-toggle-closed-captions-button');
        captionButton.setAttribute('aria-label', 'Captions');
        captionButton.setAttribute('aria-haspopup', 'true');
        captionButton.setAttribute('aria-owns', 'audioTrackMenu');
        this.listenFor(captionButton, 'click', this.handleCaptionButtonClicked);
        this.listenFor(captionButton, 'mouseover', this.handleCaptionButtonMouseOver);
        this.listenFor(captionButton, 'mouseout', this.handleCaptionButtonMouseOut);

        var fullscreenButton = this.controls.fullscreenButton = document.createElement('button');
        fullscreenButton.setAttribute('pseudo', '-webkit-media-controls-fullscreen-button');
        fullscreenButton.setAttribute('aria-label', 'Display Full Screen');
        fullscreenButton.disabled = true;
        this.listenFor(fullscreenButton, 'click', this.handleFullscreenButtonClicked);
    },

    configureControls: function()
    {
        this.controls.panel.appendChild(this.controls.playButton);
        this.controls.panel.appendChild(this.controls.timeline);
        this.controls.panel.appendChild(this.controls.currentTime);
        this.controls.panel.appendChild(this.controls.muteBox);
        this.controls.muteBox.appendChild(this.controls.muteButton);
        this.controls.muteBox.appendChild(this.controls.volumeBox);
        this.controls.volumeBox.appendChild(this.controls.volume);
        this.controls.panel.appendChild(this.controls.captionButton);
        if (!this.isAudio())
            this.controls.panel.appendChild(this.controls.fullscreenButton);
        this.controls.enclosure.appendChild(this.controls.panel);
    },

    disconnectControls: function(event)
    {
        for (var item in this.controls) {
            var control = this.controls[item];
            if (control && control.parentNode)
                control.parentNode.removeChild(control);
       }
    },

    reconnectControls: function()
    {
        this.disconnectControls();
        this.configureControls();

        if (this.shouldHaveControls())
            this.addControls();
    },

    showControls: function()
    {
        this.controls.panel.classList.remove(this.ClassNames.hidden);
        this.controls.panel.classList.add(this.ClassNames.show);

        this.updateTime();
    },

    hideControls: function()
    {
        this.controls.panel.classList.remove(this.ClassNames.show);
    },

    resetHideControlsTimer: function()
    {
        if (this.hideTimer)
            clearTimeout(this.hideTimer);
        this.hideTimer = setTimeout(this.hideControls.bind(this), this.HideControlsDelay);
    },

    clearHideControlsTimer: function()
    {
        if (this.hideTimer)
            clearTimeout(this.hideTimer);
        this.hideTimer = null;
    },

    controlsAreAlwaysVisible: function()
    {
        return this.isAudio() || this.controls.panel.classList.contains(this.ClassNames.noVideo);
    },

    controlsAreHidden: function()
    {
        if (this.controlsAreAlwaysVisible())
            return false;

        var panel = this.controls.panel;
        return (!panel.classList.contains(this.ClassNames.show) || panel.classList.contains(this.ClassNames.hidden))
            && (panel.parentElement.querySelector(':hover') !== panel);
    },

    addControls: function()
    {
        this.base.appendChild(this.controls.enclosure);
    },

    removeControls: function()
    {
        if (this.controls.enclosure.parentNode)
            this.controls.enclosure.parentNode.removeChild(this.controls.enclosure);
        this.hideCaptionMenu();
    },

    updateControls: function()
    {
        this.reconnectControls();
    },

    setIsLive: function(live)
    {
        if (live === this.isLive)
            return;
        this.isLive = live;

        this.reconnectControls();
        this.controls.timeline.disabled = this.isLive;
    },

    updateDuration: function()
    {
        var duration = this.video.duration;
        this.setIsLive(duration === Number.POSITIVE_INFINITY);
        this.controls.timeline.min = 0;
        this.controls.timeline.max = this.isLive ? 0 : duration;
        if (this.isLive || isNaN(duration))
            this.timeDigitsCount = 4;
        else if (duration < 10 * 60) /* Ten minutes */
            this.timeDigitsCount = 3;
        else if (duration < 60 * 60) /* One hour */
            this.timeDigitsCount = 4;
        else if (duration < 10 * 60 * 60) /* Ten hours */
            this.timeDigitsCount = 5;
        else
            this.timeDigitsCount = 6;
    },

    formatTime: function(time)
    {
        if (isNaN(time))
            return '00:00';

        const absTime = Math.abs(time);
        const seconds = Math.floor(absTime % 60).toFixed(0);
        const minutes = Math.floor((absTime / 60) % 60).toFixed(0);
        const hours = Math.floor(absTime / (60 * 60)).toFixed(0);

        function prependZeroIfNeeded(value) {
            if (value < 10)
                return `0${value}`;
            return `${value}`;
        }

        switch (this.timeDigitsCount) {
        case 3:
            return minutes + ':' + prependZeroIfNeeded(seconds);
        case 4:
            return prependZeroIfNeeded(minutes) + ':' + prependZeroIfNeeded(seconds);
        case 5:
            return hours + ':' + prependZeroIfNeeded(minutes) + ':' + prependZeroIfNeeded(seconds);
        case 6:
            return prependZeroIfNeeded(hours) + ':' + prependZeroIfNeeded(minutes) + ':' + prependZeroIfNeeded(seconds);
        }
    },

    updateTime: function(forceUpdate)
    {
        if (!forceUpdate && this.controlsAreHidden())
            return;

        var currentTime = this.video.currentTime;
        this.controls.timeline.value = currentTime;
        this.controls.currentTime.innerText = this.formatTime(currentTime);
        if (!this.isLive) {
            var duration = this.video.duration;
            this.controls.currentTime.innerText += " / " + this.formatTime(duration);
            this.controls.currentTime.classList.toggle(this.ClassNames.noDuration, !duration);
            this.controls.timeline.disabled = !duration;
        } else
            this.controls.currentTime.classList.remove(this.ClassNames.noDuration);
    },

    setPlaying: function(isPlaying)
    {
        if (this.isPlaying === isPlaying)
            return;
        this.isPlaying = isPlaying;

        if (!isPlaying) {
            this.controls.panel.classList.add(this.ClassNames.paused);
            this.controls.playButton.classList.add(this.ClassNames.paused);
            this.controls.playButton.setAttribute('aria-label', 'Play');
            this.showControls();
        } else {
            this.controls.panel.classList.remove(this.ClassNames.paused);
            this.controls.playButton.classList.remove(this.ClassNames.paused);
            this.controls.playButton.setAttribute('aria-label', 'Pause');

            this.hideControls();
            this.resetHideControlsTimer();
        }
    },

    updatePlaying: function()
    {
        this.setPlaying(!this.canPlay());
        if (!this.canPlay())
            this.showControls();
    },

    updateCaptionButton: function()
    {
        if (this.video.webkitHasClosedCaptions)
            this.controls.captionButton.classList.remove(this.ClassNames.hidden);
        else
            this.controls.captionButton.classList.add(this.ClassNames.hidden);
    },

    updateCaptionContainer: function()
    {
        if (!this.host.textTrackContainer)
            return;

        var hasClosedCaptions = this.video.webkitHasClosedCaptions;
        var hasHiddenClass = this.host.textTrackContainer.classList.contains(this.ClassNames.hidden);

        if (hasClosedCaptions && hasHiddenClass)
            this.host.textTrackContainer.classList.remove(this.ClassNames.hidden);
        else if (!hasClosedCaptions && !hasHiddenClass)
            this.host.textTrackContainer.classList.add(this.ClassNames.hidden);

        this.updateBase();
        this.host.updateTextTrackContainer();
    },

    updateFullscreenButton: function()
    {
        if (this.video.readyState > HTMLMediaElement.HAVE_NOTHING && !this.hasVisualMedia) {
            this.controls.fullscreenButton.classList.add(this.ClassNames.hidden);
            return;
        }

        this.controls.fullscreenButton.disabled = !this.host.supportsFullscreen;
    },

    updateVolume: function()
    {
        if (this.video.muted || !this.video.volume) {
            this.controls.muteButton.classList.add(this.ClassNames.muted);
            this.controls.volume.value = 0;
        } else {
            this.controls.muteButton.classList.remove(this.ClassNames.muted);
            this.controls.volume.value = this.video.volume;
        }
    },

    updateHasAudio: function()
    {
        this.controls.muteButton.disabled = this.video.audioTracks.length == 0;
    },

    updateHasVideo: function()
    {
        if (this.video.videoTracks.length)
            this.controls.panel.classList.remove(this.ClassNames.noVideo);
        else
            this.controls.panel.classList.add(this.ClassNames.noVideo);
    },

    handleReadyStateChange: function(event)
    {
        this.hasVisualMedia = this.video.videoTracks && this.video.videoTracks.length > 0;
        this.updateVolume();
        this.updateDuration();
        this.updateCaptionButton();
        this.updateCaptionContainer();
        this.updateFullscreenButton();
    },

    handleTimeUpdate: function(event)
    {
        this.updateTime();
    },

    handleDurationChange: function(event)
    {
        this.updateDuration();
        this.updateTime(true);
    },

    handlePlay: function(event)
    {
        this.setPlaying(true);
    },

    handlePause: function(event)
    {
        this.setPlaying(false);
    },

    handleVolumeChange: function(event)
    {
        this.updateVolume();
    },

    handleFullscreenChange: function(event)
    {
        this.updateBase();
        this.updateControls();

        if (this.isFullScreen()) {
            this.controls.fullscreenButton.classList.add(this.ClassNames.exit);
            this.controls.fullscreenButton.setAttribute('aria-label', 'Exit Full Screen');
            this.host.enteredFullscreen();
        } else {
            this.controls.fullscreenButton.classList.remove(this.ClassNames.exit);
            this.controls.fullscreenButton.setAttribute('aria-label', 'Display Full Screen');
            this.host.exitedFullscreen();
        }
    },

    handleTextTrackChange: function(event)
    {
        this.updateCaptionContainer();
    },

    handleTextTrackAdd: function(event)
    {
        this.updateCaptionButton();
        this.updateCaptionContainer();
    },

    handleTextTrackRemove: function(event)
    {
        this.updateCaptionButton();
        this.updateCaptionContainer();
    },

    handleControlsChange: function()
    {
        try {
            this.updateBase();

            if (this.shouldHaveControls())
                this.addControls();
            else
                this.removeControls();
        } catch(e) {
            if (window.console)
                console.error(e);
        }
    },

    handleWrapperMouseMove: function(event)
    {
        this.showControls();
        this.resetHideControlsTimer();

        if (!this.isDragging)
            return;
        var delta = new WebKitPoint(event.clientX - this.initialDragLocation.x, event.clientY - this.initialDragLocation.y);
        this.controls.panel.style.left = this.initialOffset.x + delta.x + 'px';
        this.controls.panel.style.top = this.initialOffset.y + delta.y + 'px';
        event.stopPropagation()
    },

    handleWrapperMouseOut: function(event)
    {
        this.hideControls();
        this.clearHideControlsTimer();
    },

    handleWrapperMouseUp: function(event)
    {
        this.isDragging = false;
        this.stopListeningFor(this.base, 'mouseup', 'handleWrapperMouseUp', true);
    },

    handlePanelMouseDown: function(event)
    {
        if (event.target != this.controls.panel)
            return;

        if (!this.isFullScreen())
            return;

        this.listenFor(this.base, 'mouseup', this.handleWrapperMouseUp, true);
        this.isDragging = true;
        this.initialDragLocation = new WebKitPoint(event.clientX, event.clientY);
        this.initialOffset = new WebKitPoint(
            parseInt(this.controls.panel.style.left) | 0,
            parseInt(this.controls.panel.style.top) | 0
        );
    },

    handlePanelTransitionEnd: function(event)
    {
        var opacity = window.getComputedStyle(this.controls.panel).opacity;
        if (parseInt(opacity) > 0)
            this.controls.panel.classList.remove(this.ClassNames.hidden);
        else
            this.controls.panel.classList.add(this.ClassNames.hidden);
    },

    handlePanelClick: function(event)
    {
        /* Prevent clicks in the panel from playing or pausing the video in a MediaDocument. */
        event.preventDefault();
    },

    canPlay: function()
    {
        return this.video.paused || this.video.ended || this.video.readyState < HTMLMediaElement.HAVE_METADATA;
    },

    handlePlayButtonClicked: function(event)
    {
        if (this.canPlay())
            this.video.play();
        else
            this.video.pause();
        return true;
    },

    handleTimelineChange: function(event)
    {
        this.video.fastSeek(this.controls.timeline.value);
    },

    handleTimelineMouseUp: function(event)
    {
        /* Do a precise seek when we lift the mouse. */
        this.video.currentTime = this.controls.timeline.value;
    },

    handleMuteButtonClicked: function(event)
    {
        this.video.muted = !this.video.muted;
        if (this.video.muted)
            this.controls.muteButton.setAttribute('aria-label', 'Unmute');
        return true;
    },

    handleMuteButtonMouseOver: function(event)
    {
        if (this.video.offsetTop + this.controls.enclosure.offsetTop < 105) {
            this.controls.volumeBox.classList.add(this.ClassNames.down);
            this.controls.panel.classList.add(this.ClassNames.down);
        } else {
            this.controls.volumeBox.classList.remove(this.ClassNames.down);
            this.controls.panel.classList.remove(this.ClassNames.down);
        }
        this.controls.volumeBox.classList.remove(this.ClassNames.hiding);

        return true;
    },

    handleVolumeBoxMouseOut: function(event)
    {
        this.controls.volumeBox.classList.add(this.ClassNames.hiding);
        return true;
    },

    handleVolumeSliderInput: function(event)
    {
        if (this.video.muted) {
            this.video.muted = false;
            this.controls.muteButton.setAttribute('aria-label', 'Mute');
        }
        this.video.volume = this.controls.volume.value;
    },

    handleFullscreenButtonClicked: function(event)
    {
        if (this.isFullScreen())
            this.video.webkitExitFullscreen();
        else
            this.video.webkitEnterFullscreen();
        return true;
    },

    buildCaptionMenu: function()
    {
        var tracks = this.host.sortedTrackListForMenu(this.video.textTracks);
        if (!tracks || !tracks.length)
            return;

        this.captionMenu = document.createElement('div');
        this.captionMenu.setAttribute('pseudo', '-webkit-media-controls-closed-captions-container');
        this.captionMenu.setAttribute('id', 'audioTrackMenu');
        this.listenFor(this.captionMenu, 'mouseout', this.handleCaptionMenuMouseOut);
        this.listenFor(this.captionMenu, 'transitionend', this.captionMenuTransitionEnd);
        this.base.appendChild(this.captionMenu);
        this.captionMenu.captionMenuTreeElements = [];
        this.captionMenuItems = [];

        var offItem = this.host.captionMenuOffItem;
        var automaticItem = this.host.captionMenuAutomaticItem;
        var displayMode = this.host.captionDisplayMode;

        var list = document.createElement('div');
        this.captionMenu.appendChild(list);
        this.captionMenu.captionMenuTreeElements.push(list);

        var heading = document.createElement('h3');
        heading.id = 'webkitMediaControlsClosedCaptionsHeading';
        list.appendChild(heading);
        heading.innerText = 'Subtitles';
        this.captionMenu.captionMenuTreeElements.push(heading);

        var ul = document.createElement('ul');
        ul.setAttribute('role', 'menu');
        ul.setAttribute('aria-labelledby', 'webkitMediaControlsClosedCaptionsHeading');
        list.appendChild(ul);
        this.captionMenu.captionMenuTreeElements.push(ul);

        for (var i = 0; i < tracks.length; ++i) {
            var menuItem = document.createElement('li');
            menuItem.setAttribute('role', 'menuitemradio');
            menuItem.setAttribute('tabindex', '-1');
            this.captionMenuItems.push(menuItem);
            this.listenFor(menuItem, 'click', this.captionItemSelected);
            this.listenFor(menuItem, 'keyup', this.handleCaptionItemKeyUp);
            ul.appendChild(menuItem);

            var track = tracks[i];
            menuItem.innerText = this.host.displayNameForTrack(track);
            menuItem.track = track;

            if (track === offItem) {
                var offMenu = menuItem;
                continue;
            }

            if (track === automaticItem) {
                if (displayMode === 'automatic') {
                    menuItem.classList.add(this.ClassNames.selected);
                    menuItem.setAttribute('tabindex', '0');
                    menuItem.setAttribute('aria-checked', 'true');
                }
                continue;
            }

            if (displayMode != 'automatic' && track.mode === 'showing') {
                var trackMenuItemSelected = true;
                menuItem.classList.add(this.ClassNames.selected);
                menuItem.setAttribute('tabindex', '0');
                menuItem.setAttribute('aria-checked', 'true');
            }
        }

        if (offMenu && displayMode === 'forced-only' && !trackMenuItemSelected) {
            offMenu.classList.add(this.ClassNames.selected);
            menuItem.setAttribute('tabindex', '0');
            menuItem.setAttribute('aria-checked', 'true');
        }

        /* Focus first selected menuitem. */
        for (var i = 0, c = this.captionMenuItems.length; i < c; i++) {
            var item = this.captionMenuItems[i];
            if (item.classList.contains(this.ClassNames.selected)) {
                item.focus();
                break;
            }
        }

        /* Caption menu has to be centered to the caption button. */
        var captionButtonCenter =  this.controls.panel.offsetLeft + this.controls.captionButton.offsetLeft +
            this.controls.captionButton.offsetWidth / 2;
        var captionMenuLeft = (captionButtonCenter - this.captionMenu.offsetWidth / 2);
        if (captionMenuLeft + this.captionMenu.offsetWidth > this.controls.panel.offsetLeft + this.controls.panel.offsetWidth)
            this.captionMenu.classList.add(this.ClassNames.out);
        this.captionMenu.style.left = captionMenuLeft + 'px';
        /* As height is not in the css, it needs to be specified to animate it. */
        this.captionMenu.height = this.captionMenu.offsetHeight;
        this.captionMenu.style.height = 0;
    },

    captionItemSelected: function(event)
    {
        this.host.setSelectedTextTrack(event.target.track);
        this.hideCaptionMenu();
    },

    focusSiblingCaptionItem: function(event)
    {
        var currentItem = event.target;
        var pendingItem = false;
        switch(event.keyCode) {
        case this.KeyCodes.left:
        case this.KeyCodes.up:
            pendingItem = currentItem.previousSibling;
            break;
        case this.KeyCodes.right:
        case this.KeyCodes.down:
            pendingItem = currentItem.nextSibling;
            break;
        }
        if (pendingItem) {
            currentItem.setAttribute('tabindex', '-1');
            pendingItem.setAttribute('tabindex', '0');
            pendingItem.focus();
        }
    },

    handleCaptionItemKeyUp: function(event)
    {
        switch (event.keyCode) {
        case this.KeyCodes.enter:
        case this.KeyCodes.space:
            this.captionItemSelected(event);
            break;
        case this.KeyCodes.escape:
            this.hideCaptionMenu();
            break;
        case this.KeyCodes.left:
        case this.KeyCodes.up:
        case this.KeyCodes.right:
        case this.KeyCodes.down:
            this.focusSiblingCaptionItem(event);
            break;
        default:
            return;
        }

        event.stopPropagation();
        event.preventDefault();
    },

    showCaptionMenu: function()
    {
        if (!this.captionMenu)
            this.buildCaptionMenu();
        this.captionMenu.style.height = this.captionMenu.height + 'px';
    },

    hideCaptionMenu: function()
    {
        if (!this.captionMenu)
            return;
        this.captionMenu.style.height = 0;
    },

    captionMenuTransitionEnd: function(event)
    {
        if (this.captionMenu.offsetHeight !== 0)
            return;

        this.captionMenuItems.forEach(function(item) {
            this.stopListeningFor(item, 'click', this.captionItemSelected);
            this.stopListeningFor(item, 'keyup', this.handleCaptionItemKeyUp);
        }, this);

        /* FKA and AX: focus the trigger before destroying the element with focus. */
        if (this.controls.captionButton)
            this.controls.captionButton.focus();

        if (this.captionMenu.parentNode)
            this.captionMenu.parentNode.removeChild(this.captionMenu);
        delete this.captionMenu;
        delete this.captionMenuItems;
    },

    captionMenuContainsNode: function(node)
    {
        return this.captionMenu.captionMenuTreeElements.find((item) => item == node)
            || this.captionMenuItems.find((item) => item == node);
    },

    handleCaptionButtonClicked: function(event)
    {
        this.showCaptionMenu();
        return true;
    },

    handleCaptionButtonMouseOver: function(event)
    {
        this.showCaptionMenu();
        return true;
    },

    handleCaptionButtonMouseOut: function(event)
    {
        if (this.captionMenu && !this.captionMenuContainsNode(event.relatedTarget))
            this.hideCaptionMenu();
        return true;
    },

    handleCaptionMenuMouseOut: function(event)
    {
        if (event.relatedTarget != this.controls.captionButton && !this.captionMenuContainsNode(event.relatedTarget))
            this.hideCaptionMenu();
        return true;
    },
};
