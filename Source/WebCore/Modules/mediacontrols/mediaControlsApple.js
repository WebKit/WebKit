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

    this.addVideoListeners();
    this.createBase();
    this.createControls();
    this.setControlsType(this.isFullScreen() ? Controller.FullScreenControls : Controller.InlineControls);

    this.updateBase();
    this.updateDuration();
    this.updateTime();
    this.updateReadyState();
    this.updatePlaying();
    this.updateThumbnail();
    this.updateCaptionButton();
    this.updateCaptionContainer();
    this.updateVolume();
    this.updateHasAudio();
    this.updateHasVideo();
};

/* Enums */
Controller.InlineControls = 0;
Controller.FullScreenControls = 1;

Controller.PlayAfterSeeking = 0;
Controller.PauseAfterSeeking = 1;

Controller.prototype = {

    /* Constants */
    HandledVideoEvents: {
        loadstart: 'handleLoadStart',
        error: 'handleError',
        abort: 'handleAbort',
        suspend: 'handleSuspend',
        stalled: 'handleStalled',
        waiting: 'handleWaiting',
        emptied: 'handleReadyStateChange',
        loadedmetadata: 'handleReadyStateChange',
        loadeddata: 'handleReadyStateChange',
        canplay: 'handleReadyStateChange',
        canplaythrough: 'handleReadyStateChange',
        timeupdate: 'handleTimeUpdate',
        durationchange: 'handleDurationChange',
        play: 'handlePlay',
        pause: 'handlePause',
        volumechange: 'handleVolumeChange',
        webkitfullscreenchange: 'handleFullscreenChange',
    },
    HideControlsDelay: 4 * 1000,
    RewindAmount: 30,
    MaximumSeekRate: 8,
    SeekDelay: 1500,
    ClassNames: {
        exit: 'exit',
        hidden: 'hidden',
        list: 'list',
        muteBox: 'mute-box',
        muted: 'muted',
        paused: 'paused',
        playing: 'playing',
        selected: 'selected',
        show: 'show',
        thumbnail: 'thumbnail',
        thumbnailImage: 'thumbnail-image',
        thumbnailTrack: 'thumbnail-track',
        volumeBox: 'volume-box',
        noVideo: 'no-video',
    },
    KeyCodes: {
        enter: 13,
        escape: 27,
        space: 32,
        pageUp: 33,
        pageDown: 34,
        end: 35,
        home: 36,
        left: 37,
        up: 38,
        right: 39,
        down: 40
    },

    // Localized string accessor
    UIString: function(s){
        if (this.localizedStrings[s])
            return this.localizedStrings[s];
        else
            return s; // FIXME: log something if string not localized.
    },
    localizedStrings: {
        // FIXME: Move localization to ext strings file <http://webkit.org/b/120956>
        'Aborted': 'Aborted',
        'Audio Playback': 'Audio Playback',
        'Captions': 'Captions',
        'Display Full Screen': 'Display Full Screen',
        'Duration': 'Duration',
        'Elapsed': 'Elapsed',
        'Error': 'Error',
        'Exit Full Screen': 'Exit Full Screen',
        'Fast Forward': 'Fast Forward',
        'Loading': 'Loading',
        'Maximum Volume': 'Maximum Volume',
        'Minimum Volume': 'Minimum Volume',
        'Mute': 'Mute',
        'Pause': 'Pause',
        'Play': 'Play',
        'Remaining': 'Remaining',
        'Rewind': 'Rewind',
        'Rewind %%sec%% Seconds': 'Rewind %%sec%% Seconds',
        'Stalled': 'Stalled',
        'Subtitles': 'Subtitles',
        'Suspended': 'Suspended',
        'Unmute': 'Unmute',
        'Video Playback': 'Video Playback',
        'Volume': 'Volume',
        'Waiting': 'Waiting'
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
        for (name in this.HandledVideoEvents) {
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
        for (name in this.HandledVideoEvents) {
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
        try {
            if (event.target === this.video) {
                var handlerName = this.HandledVideoEvents[event.type];
                var handler = this[handlerName];
                if (handler && handler instanceof Function)
                    handler.call(this, event);
            } else {
                if (!(this.listeners[event.type] instanceof Array))
                    return;

                this.listeners[event.type].forEach(function(entry) {
                    if (entry.element === event.currentTarget && entry.handler instanceof Function)
                        entry.handler.call(this, event);
                }, this);
            }
        } catch(e) {
            if (window.console)
                console.error(e);
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

    shouldHaveControls: function()
    {
        return this.video.controls || this.isFullScreen();
    },

    updateBase: function()
    {
        if (this.shouldHaveControls() || (this.video.textTracks && this.video.textTracks.length)) {
            if (!this.base.parentNode)
                this.root.appendChild(this.base);
        } else {
            if (this.base.parentNode)
                this.base.parentNode.removeChild(this.base);
        }
    },

    createControls: function()
    {
        var panel = this.controls.panel = document.createElement('div');
        panel.setAttribute('pseudo', '-webkit-media-controls-panel');
        panel.setAttribute('aria-label', (this.isAudio() ? this.UIString('Audio Playback') : this.UIString('Video Playback')));
        panel.setAttribute('role', 'toolbar');
        this.listenFor(panel, 'mousedown', this.handlePanelMouseDown);
        this.listenFor(panel, 'transitionend', this.handlePanelTransitionEnd);
        this.listenFor(panel, 'click', this.handlePanelClick);
        this.listenFor(panel, 'dblclick', this.handlePanelClick);

        var rewindButton = this.controls.rewindButton = document.createElement('button');
        rewindButton.setAttribute('pseudo', '-webkit-media-controls-rewind-button');
        rewindButton.setAttribute('aria-label', this.UIString('Rewind %%sec%% Seconds').replace('%%sec%%', this.RewindAmount));
        this.listenFor(rewindButton, 'click', this.handleRewindButtonClicked);

        var seekBackButton = this.controls.seekBackButton = document.createElement('button');
        seekBackButton.setAttribute('pseudo', '-webkit-media-controls-seek-back-button');
        seekBackButton.setAttribute('aria-label', this.UIString('Rewind'));
        this.listenFor(seekBackButton, 'mousedown', this.handleSeekBackMouseDown);
        this.listenFor(seekBackButton, 'mouseup', this.handleSeekBackMouseUp);

        var seekForwardButton = this.controls.seekForwardButton = document.createElement('button');
        seekForwardButton.setAttribute('pseudo', '-webkit-media-controls-seek-forward-button');
        seekForwardButton.setAttribute('aria-label', this.UIString('Fast Forward'));
        this.listenFor(seekForwardButton, 'mousedown', this.handleSeekForwardMouseDown);
        this.listenFor(seekForwardButton, 'mouseup', this.handleSeekForwardMouseUp);

        var playButton = this.controls.playButton = document.createElement('button');
        playButton.setAttribute('pseudo', '-webkit-media-controls-play-button');
        playButton.setAttribute('aria-label', this.UIString('Play'));
        this.listenFor(playButton, 'click', this.handlePlayButtonClicked);

        var statusDisplay = this.controls.statusDisplay = document.createElement('div');
        statusDisplay.setAttribute('pseudo', '-webkit-media-controls-status-display');
        statusDisplay.classList.add(this.ClassNames.hidden);

        var timelineBox = this.controls.timelineBox = document.createElement('div');
        timelineBox.setAttribute('pseudo', '-webkit-media-controls-timeline-container');

        var currentTime = this.controls.currentTime = document.createElement('div');
        currentTime.setAttribute('pseudo', '-webkit-media-controls-current-time-display');
        currentTime.setAttribute('aria-label', this.UIString('Elapsed'));
        currentTime.setAttribute('role', 'timer');

        var timeline = this.controls.timeline = document.createElement('input');
        timeline.setAttribute('pseudo', '-webkit-media-controls-timeline');
        timeline.setAttribute('aria-label', this.UIString('Duration'));
        timeline.type = 'range';
        this.listenFor(timeline, 'change', this.handleTimelineChange);
        this.listenFor(timeline, 'mouseover', this.handleTimelineMouseOver);
        this.listenFor(timeline, 'mouseout', this.handleTimelineMouseOut);
        this.listenFor(timeline, 'mousemove', this.handleTimelineMouseMove);
        this.listenFor(timeline, 'mousedown', this.handleTimelineMouseDown);
        this.listenFor(timeline, 'mouseup', this.handleTimelineMouseUp);
        timeline.step = .01;

        var thumbnailTrack = this.controls.thumbnailTrack = document.createElement('div');
        thumbnailTrack.classList.add(this.ClassNames.thumbnailTrack);

        var thumbnail = this.controls.thumbnail = document.createElement('div');
        thumbnail.classList.add(this.ClassNames.thumbnail);

        var thumbnailImage = this.controls.thumbnailImage = document.createElement('img');
        thumbnailImage.classList.add(this.ClassNames.thumbnailImage);

        var remainingTime = this.controls.remainingTime = document.createElement('div');
        remainingTime.setAttribute('pseudo', '-webkit-media-controls-time-remaining-display');
        remainingTime.setAttribute('aria-label', this.UIString('Remaining'));
        remainingTime.setAttribute('role', 'timer');

        var muteBox = this.controls.muteBox = document.createElement('div');
        muteBox.classList.add(this.ClassNames.muteBox);

        var muteButton = this.controls.muteButton = document.createElement('button');
        muteButton.setAttribute('pseudo', '-webkit-media-controls-mute-button');
        muteButton.setAttribute('aria-label', this.UIString('Mute'));
        this.listenFor(muteButton, 'click', this.handleMuteButtonClicked);

        var minButton = this.controls.minButton = document.createElement('button');
        minButton.setAttribute('pseudo', '-webkit-media-controls-volume-min-button');
        minButton.setAttribute('aria-label', this.UIString('Minimum Volume'));
        this.listenFor(minButton, 'click', this.handleMinButtonClicked);

        var maxButton = this.controls.maxButton = document.createElement('button');
        maxButton.setAttribute('pseudo', '-webkit-media-controls-volume-max-button');
        maxButton.setAttribute('aria-label', this.UIString('Maximum Volume'));
        this.listenFor(maxButton, 'click', this.handleMaxButtonClicked);

        var volumeBox = this.controls.volumeBox = document.createElement('div');
        volumeBox.classList.add(this.ClassNames.volumeBox);

        var volume = this.controls.volume = document.createElement('input');
        volume.setAttribute('pseudo', '-webkit-media-controls-volume-slider');
        volume.setAttribute('aria-label', this.UIString('Volume'));
        volume.type = 'range';
        volume.min = 0;
        volume.max = 1;
        volume.step = .01;
        this.listenFor(volume, 'change', this.handleVolumeSliderChange);

        var captionButton = this.controls.captionButton = document.createElement('button');
        captionButton.setAttribute('pseudo', '-webkit-media-controls-toggle-closed-captions-button');
        captionButton.setAttribute('aria-label', this.UIString('Captions'));
        captionButton.setAttribute('aria-haspopup', 'true');
        this.listenFor(captionButton, 'click', this.handleCaptionButtonClicked);

        var fullscreenButton = this.controls.fullscreenButton = document.createElement('button');
        fullscreenButton.setAttribute('pseudo', '-webkit-media-controls-fullscreen-button');
        fullscreenButton.setAttribute('aria-label', this.UIString('Display Full Screen'));
        this.listenFor(fullscreenButton, 'click', this.handleFullscreenButtonClicked);
    },

    setControlsType: function(type)
    {
        if (type === this.controlsType)
            return;

        this.disconnectControls();

        if (type === Controller.InlineControls)
            this.configureInlineControls();
        else
            this.configureFullScreenControls();

        if (this.shouldHaveControls())
            this.addControls();
    },

    disconnectControls: function(event)
    {
        for (item in this.controls) {
            var control = this.controls[item];
            if (control && control.parentNode)
                control.parentNode.removeChild(control);
       }
    },

    configureInlineControls: function()
    {
        this.controls.panel.appendChild(this.controls.rewindButton);
        this.controls.panel.appendChild(this.controls.playButton);
        this.controls.panel.appendChild(this.controls.statusDisplay);
        this.controls.panel.appendChild(this.controls.timelineBox);
        this.controls.timelineBox.appendChild(this.controls.currentTime);
        this.controls.timelineBox.appendChild(this.controls.thumbnailTrack);
        this.controls.thumbnailTrack.appendChild(this.controls.timeline);
        this.controls.thumbnailTrack.appendChild(this.controls.thumbnail);
        this.controls.thumbnail.appendChild(this.controls.thumbnailImage);
        this.controls.timelineBox.appendChild(this.controls.remainingTime);
        this.controls.panel.appendChild(this.controls.muteBox);
        this.controls.muteBox.appendChild(this.controls.volumeBox);
        this.controls.volumeBox.appendChild(this.controls.volume);
        this.controls.muteBox.appendChild(this.controls.muteButton);
        this.controls.panel.appendChild(this.controls.captionButton);
        if (!this.isAudio())
            this.controls.panel.appendChild(this.controls.fullscreenButton);

        this.controls.panel.style.removeProperty('left');
        this.controls.panel.style.removeProperty('top');
        this.controls.panel.style.removeProperty('bottom');
    },

    configureFullScreenControls: function()
    {
        this.controls.panel.appendChild(this.controls.volumeBox);
        this.controls.volumeBox.appendChild(this.controls.minButton);
        this.controls.volumeBox.appendChild(this.controls.volume);
        this.controls.volumeBox.appendChild(this.controls.maxButton);
        this.controls.panel.appendChild(this.controls.seekBackButton);
        this.controls.panel.appendChild(this.controls.playButton);
        this.controls.panel.appendChild(this.controls.seekForwardButton);
        this.controls.panel.appendChild(this.controls.captionButton);
        if (!this.isAudio())
            this.controls.panel.appendChild(this.controls.fullscreenButton);
        this.controls.panel.appendChild(this.controls.timelineBox);
        this.controls.timelineBox.appendChild(this.controls.currentTime);
        this.controls.timelineBox.appendChild(this.controls.thumbnailTrack);
        this.controls.thumbnailTrack.appendChild(this.controls.timeline);
        this.controls.thumbnailTrack.appendChild(this.controls.thumbnail);
        this.controls.thumbnail.appendChild(this.controls.thumbnailImage);
        this.controls.timelineBox.appendChild(this.controls.remainingTime);
    },

    handleLoadStart: function(event)
    {
        this.controls.statusDisplay.innerText = this.UIString('Loading');
    },

    handleError: function(event)
    {
        this.controls.statusDisplay.innerText = this.UIString('Error');
    },

    handleAbort: function(event)
    {
        this.controls.statusDisplay.innerText = this.UIString('Aborted');
    },

    handleSuspend: function(event)
    {
        this.controls.statusDisplay.innerText = this.UIString('Suspended');
    },

    handleStalled: function(event)
    {
        this.controls.statusDisplay.innerText = this.UIString('Stalled');
    },

    handleWaiting: function(event)
    {
        this.controls.statusDisplay.innerText = this.UIString('Waiting');
    },

    handleReadyStateChange: function(event)
    {
        this.updateReadyState();
        this.updateCaptionButton();
        this.updateCaptionContainer();
    },

    handleTimeUpdate: function(event)
    {
        if (!this.scrubbing)
            this.updateTime();
    },

    handleDurationChange: function(event)
    {
        this.updateDuration();
        this.updateTime();
    },

    handlePlay: function(event)
    {
        this.updatePlaying();
    },

    handlePause: function(event)
    {
        this.updatePlaying();
    },

    handleVolumeChange: function(event)
    {
        this.updateVolume();
    },

    handleTextTrackChange: function(event)
    {
        this.updateCaptionContainer();
    },

    handleTextTrackAdd: function(event)
    {
        var track = event.track;
        this.listenFor(track, 'cuechange', this.handleTextTrackCueChange);

        if (this.trackHasThumbnails(track) && track.mode === 'disabled')
            track.mode = 'hidden';

        this.updateThumbnail();
        this.updateCaptionButton();
        this.updateCaptionContainer();
    },

    handleTextTrackRemove: function(event)
    {
        var track = event.track;
        this.stopListeningFor(track, 'cuechange', this.handleTextTrackCueChange);
        this.updateThumbnail();
        this.updateCaptionButton();
        this.updateCaptionContainer();
    },

    handleTextTrackCueChange: function(event)
    {
        this.updateCaptionContainer();
    },

    isFullScreen: function()
    {
        return document.webkitCurrentFullScreenElement === this.video;
    },

    handleFullscreenChange: function(event)
    {
        this.updateBase();

        if (this.isFullScreen()) {
            this.controls.fullscreenButton.classList.add(this.ClassNames.exit);
            this.controls.fullscreenButton.setAttribute('aria-label', this.UIString('Exit Full Screen'));
            this.setControlsType(Controller.FullScreenControls);
        } else {
            this.controls.fullscreenButton.classList.remove(this.ClassNames.exit);
            this.controls.fullscreenButton.setAttribute('aria-label', this.UIString('Display Full Screen'));
            this.setControlsType(Controller.InlineControls);
        }
    },

    handleWrapperMouseMove: function(event)
    {
        this.showControls();
        if (this.hideTimer)
            clearTimeout(this.hideTimer);
        this.hideTimer = setTimeout(this.hideControls.bind(this), this.HideControlsDelay);

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
        if (this.hideTimer)
            clearTimeout(this.hideTimer);
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
        // Prevent clicks in the panel from playing or pausing the video in a MediaDocument.
        event.preventDefault();
    },

    handleRewindButtonClicked: function(event)
    {
        var newTime = Math.max(
                               this.video.currentTime - this.RewindAmount,
                               this.video.seekable.start(0));
        this.video.currentTime = newTime;
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
    },

    handleTimelineChange: function(event)
    {
        this.video.fastSeek(this.controls.timeline.value);
    },

    handleTimelineDown: function(event)
    {
        this.controls.thumbnail.classList.add(this.ClassNames.show);
    },

    handleTimelineUp: function(event)
    {
        this.controls.thumbnail.classList.remove(this.ClassNames.show);
    },

    handleTimelineMouseOver: function(event)
    {
        this.controls.thumbnail.classList.add(this.ClassNames.show);
    },

    handleTimelineMouseOut: function(event)
    {
        this.controls.thumbnail.classList.remove(this.ClassNames.show);
    },

    handleTimelineMouseMove: function(event)
    {
        if (this.controls.thumbnail.classList.contains(this.ClassNames.hidden))
            return;

        this.controls.thumbnail.classList.add(this.ClassNames.show);
        var localPoint = webkitConvertPointFromPageToNode(this.controls.timeline, new WebKitPoint(event.clientX, event.clientY));
        var percent = (localPoint.x - this.controls.timeline.offsetLeft) / this.controls.timeline.offsetWidth;
        percent = Math.max(Math.min(1, percent), 0);
        this.controls.thumbnail.style.left = percent * 100 + '%';

        var thumbnailTime = percent * this.video.duration;
        for (var i = 0; i < this.video.textTracks.length; ++i) {
            var track = this.video.textTracks[i];
            if (!this.trackHasThumbnails(track))
                continue;

            if (!track.cues)
                continue;

            for (var j = 0; j < track.cues.length; ++j) {
                var cue = track.cues[j];
                if (thumbnailTime >= cue.startTime && thumbnailTime < cue.endTime) {
                    this.controls.thumbnailImage.src = cue.text;
                    return;
                }
            }
        }
    },

    handleTimelineMouseDown: function(event)
    {
        this.scrubbing = true;
    },

    handleTimelineMouseUp: function(event)
    {
        this.scrubbing = false;

        // Do a precise seek when we lift the mouse:
        this.video.currentTime = this.controls.timeline.value;
    },

    handleMuteButtonClicked: function(event)
    {
        this.video.muted = !this.video.muted;
        if (this.video.muted)
            this.controls.muteButton.setAttribute('aria-label', this.UIString('Unmute'));
    },

    handleMinButtonClicked: function(event)
    {
        if (this.video.muted) {
            this.video.muted = false;
            this.controls.muteButton.setAttribute('aria-label', this.UIString('Mute'));
        }
        this.video.volume = 0;
    },

    handleMaxButtonClicked: function(event)
    {
        if (this.video.muted) {
            this.video.muted = false;
            this.controls.muteButton.setAttribute('aria-label', this.UIString('Mute'));
        }
        this.video.volume = 1;
    },

    handleVolumeSliderChange: function(event)
    {
        if (this.video.muted) {
            this.video.muted = false;
            this.controls.muteButton.setAttribute('aria-label', this.UIString('Mute'));
        }
        this.video.volume = this.controls.volume.value;
    },

    handleCaptionButtonClicked: function(event)
    {
        if (this.captionMenu)
            this.destroyCaptionMenu();
        else
            this.buildCaptionMenu();
    },

    handleFullscreenButtonClicked: function(event)
    {
        if (this.isFullScreen())
            document.webkitExitFullscreen();
        else
            this.video.webkitRequestFullscreen();
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

    nextRate: function()
    {
        return Math.min(this.MaximumSeekRate, Math.abs(this.video.playbackRate * 2));
    },

    handleSeekBackMouseDown: function(event)
    {
        this.actionAfterSeeking = (this.canPlay() ? Controller.PauseAfterSeeking : Controller.PlayAfterSeeking);
        this.video.play();
        this.video.playbackRate = this.nextRate() * -1;
        this.seekInterval = setInterval(this.seekBackFaster.bind(this), this.SeekDelay);
    },

    seekBackFaster: function()
    {
        this.video.playbackRate = this.nextRate() * -1;
    },

    handleSeekBackMouseUp: function(event)
    {
        this.video.playbackRate = this.video.defaultPlaybackRate;
        if (this.actionAfterSeeking === Controller.PauseAfterSeeking)
            this.video.pause();
        else if (this.actionAfterSeeking === Controller.PlayAfterSeeking)
            this.video.play();
        if (this.seekInterval)
            clearInterval(this.seekInterval);
    },

    handleSeekForwardMouseDown: function(event)
    {
        this.actionAfterSeeking = (this.canPlay() ? Controller.PauseAfterSeeking : Controller.PlayAfterSeeking);
        this.video.play();
        this.video.playbackRate = this.nextRate();
        this.seekInterval = setInterval(this.seekForwardFaster.bind(this), this.SeekDelay);
    },

    seekForwardFaster: function()
    {
        this.video.playbackRate = this.nextRate();
    },

    handleSeekForwardMouseUp: function(event)
    {
        this.video.playbackRate = this.video.defaultPlaybackRate;
        if (this.actionAfterSeeking === Controller.PauseAfterSeeking)
            this.video.pause();
        else if (this.actionAfterSeeking === Controller.PlayAfterSeeking)
            this.video.play();
        if (this.seekInterval)
            clearInterval(this.seekInterval);
    },

    updateDuration: function()
    {
        this.controls.timeline.min = 0;
        this.controls.timeline.max = this.video.duration;
    },

    formatTime: function(time)
    {
        if (isNaN(time))
            time = 0;
        var absTime = Math.abs(time);
        var intSeconds = Math.floor(absTime % 60).toFixed(0);
        var intMinutes = Math.floor(absTime / 60).toFixed(0);
        return (time < 0 ? '-' : String()) + String('00' + intMinutes).slice(-2) + ":" + String('00' + intSeconds).slice(-2)
    },

    updatePlaying: function()
    {
        if (this.canPlay()) {
            this.controls.panel.classList.add(this.ClassNames.paused);
            this.controls.playButton.classList.add(this.ClassNames.paused);
            this.controls.playButton.setAttribute('aria-label', this.UIString('Play'));
        } else {
            this.controls.panel.classList.remove(this.ClassNames.paused);
            this.controls.playButton.classList.remove(this.ClassNames.paused);
            this.controls.playButton.setAttribute('aria-label', this.UIString('Pause'));

            this.hideControls();
            if (this.hideTimer)
                clearTimeout(this.hideTimer);
            this.hideTimer = setTimeout(this.hideControls.bind(this), this.HideControlsDelay);
        }
    },

    showControls: function()
    {
        this.controls.panel.classList.add(this.ClassNames.show);
        this.controls.panel.classList.remove(this.ClassNames.hidden);
    },

    hideControls: function()
    {
        this.controls.panel.classList.remove(this.ClassNames.show);
    },

    controlsAreHidden: function()
    {
        return !this.controls.panel.classList.contains(this.ClassNames.show) || this.controls.panel.classList.contains(this.ClassNames.hidden);
    },

    removeControls: function()
    {
        if (this.controls.panel.parentNode)
            this.controls.panel.parentNode.removeChild(this.controls.panel);
        this.destroyCaptionMenu();
    },

    addControls: function()
    {
        this.base.appendChild(this.controls.panel);
    },

    updateTime: function()
    {
        var currentTime = this.video.currentTime;
        var timeRemaining = currentTime - this.video.duration;
        this.controls.currentTime.innerText = this.formatTime(currentTime);
        this.controls.timeline.value = this.video.currentTime;
        this.controls.remainingTime.innerText = this.formatTime(timeRemaining);
    },

    updateReadyState: function()
    {
        this.setStatusHidden(this.video.readyState > HTMLMediaElement.HAVE_NOTHING);
    },

    setStatusHidden: function(hidden)
    {
        if (hidden) {
            this.controls.statusDisplay.classList.add(this.ClassNames.hidden);
            this.controls.currentTime.classList.remove(this.ClassNames.hidden);
            this.controls.timeline.classList.remove(this.ClassNames.hidden);
            this.controls.remainingTime.classList.remove(this.ClassNames.hidden);
        } else {
            this.controls.statusDisplay.classList.remove(this.ClassNames.hidden);
            this.controls.currentTime.classList.add(this.ClassNames.hidden);
            this.controls.timeline.classList.add(this.ClassNames.hidden);
            this.controls.remainingTime.classList.add(this.ClassNames.hidden);
        }
    },

    trackHasThumbnails: function(track)
    {
        return track.kind === 'thumbnails' || (track.kind === 'metadata' && track.label === 'thumbnails');
    },

    updateThumbnail: function()
    {
        for (var i = 0; i < this.video.textTracks.length; ++i) {
            var track = this.video.textTracks[i];
            if (this.trackHasThumbnails(track)) {
                this.controls.thumbnail.classList.remove(this.ClassNames.hidden);
                return;
            }
        }

        this.controls.thumbnail.classList.add(this.ClassNames.hidden);
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

        if (this.video.webkitHasClosedCaptions)
            this.host.textTrackContainer.classList.remove(this.ClassNames.hidden);
        else
            this.host.textTrackContainer.classList.add(this.ClassNames.hidden);

        this.updateBase();
        this.host.updateTextTrackContainer();
    },

    buildCaptionMenu: function()
    {
        var tracks = this.host.sortedTrackListForMenu(this.video.textTracks);
        if (!tracks || !tracks.length)
            return;

        this.captionMenu = document.createElement('div');
        this.captionMenu.setAttribute('pseudo', '-webkit-media-controls-closed-captions-container');
        this.base.appendChild(this.captionMenu);
        this.captionMenuItems = [];

        var offItem = this.host.captionMenuOffItem;
        var automaticItem = this.host.captionMenuAutomaticItem;
        var displayMode = this.host.captionDisplayMode;

        var list = document.createElement('div');
        this.captionMenu.appendChild(list);
        list.classList.add(this.ClassNames.list);

        var heading = document.createElement('h3');
        heading.id = 'webkitMediaControlsClosedCaptionsHeading'; // for AX menu label
        list.appendChild(heading);
        heading.innerText = this.UIString('Subtitles');

        var ul = document.createElement('ul');
        ul.setAttribute('role', 'menu');
        ul.setAttribute('aria-labelledby', 'webkitMediaControlsClosedCaptionsHeading');
        list.appendChild(ul);

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
        
        // focus first selected menuitem
        for (var i = 0, c = this.captionMenuItems.length; i < c; i++) {
            var item = this.captionMenuItems[i];
            if (item.classList.contains(this.ClassNames.selected)) {
                item.focus();
                break;
            }
        }
        
    },

    captionItemSelected: function(event)
    {
        this.host.setSelectedTextTrack(event.target.track);
        this.destroyCaptionMenu();
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
            this.destroyCaptionMenu();
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
        // handled
        event.stopPropagation();
        event.preventDefault();
    },

    destroyCaptionMenu: function()
    {
        if (!this.captionMenu)
            return;

        this.captionMenuItems.forEach(function(item){
            this.stopListeningFor(item, 'click', this.captionItemSelected);
            this.stopListeningFor(item, 'keyup', this.handleCaptionItemKeyUp);
        }, this);

        // FKA and AX: focus the trigger before destroying the element with focus
        if (this.controls.captionButton)
            this.controls.captionButton.focus();

        if (this.captionMenu.parentNode)
            this.captionMenu.parentNode.removeChild(this.captionMenu);
        delete this.captionMenu;
        delete this.captionMenuItems;
    },

    updateHasAudio: function()
    {
        if (this.video.audioTracks.length)
            this.controls.muteBox.classList.remove(this.ClassNames.hidden);
        else
            this.controls.muteBox.classList.add(this.ClassNames.hidden);
    },

    updateHasVideo: function()
    {
        if (this.video.videoTracks.length)
            this.controls.panel.classList.remove(this.ClassNames.noVideo);
        else
            this.controls.panel.classList.add(this.ClassNames.noVideo);
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

    isAudio: function()
    {
        return this.video instanceof HTMLAudioElement;
    },

};
