function createControls(root, video, host)
{
    return new ControllerGtk(root, video, host);
};

function ControllerGtk(root, video, host)
{
    Controller.call(this, root, video, host);
};

function contains(list, obj)
{
    var i = list.length;
    while (i--)
        if (list[i] === obj)
            return true;
    return false;
};

ControllerGtk.prototype = {

    inheritFrom: function(parent) {
        for (var property in parent) {
            if (!this.hasOwnProperty(property))
                this[property] = parent[property];
        }
    },

    createControls: function()
    {
        Controller.prototype.createControls.apply(this);

        this.controls.currentTime.classList.add(this.ClassNames.hidden);

        this.controls.volumeBox.setAttribute('pseudo', '-webkit-media-controls-volume-slider-container');
        this.controls.volumeBox.classList.add(this.ClassNames.hidden);

        this.listenFor(this.controls.muteBox, 'mouseout', this.handleVolumeBoxMouseOut);
        this.listenFor(this.controls.muteButton, 'mouseover', this.handleMuteButtonMouseOver);
        this.listenFor(this.controls.volumeBox, 'mouseover', this.handleMuteButtonMouseOver);
        this.listenFor(this.controls.volume, 'mouseover', this.handleMuteButtonMouseOver);
        this.listenFor(this.controls.captionButton, 'mouseover', this.handleCaptionButtonMouseOver);
        this.listenFor(this.controls.captionButton, 'mouseout', this.handleCaptionButtonMouseOut);

        var enclosure = this.controls.enclosure = document.createElement('div');
        enclosure.setAttribute('pseudo', '-webkit-media-controls-enclosure');
    },

    configureInlineControls: function()
    {
        this.controls.panel.appendChild(this.controls.playButton);
        this.controls.panel.appendChild(this.controls.timeline);
        this.controls.panel.appendChild(this.controls.currentTime);
        this.controls.panel.appendChild(this.controls.remainingTime);
        this.controls.panel.appendChild(this.controls.captionButton);
        this.controls.panel.appendChild(this.controls.fullscreenButton);
        this.controls.panel.appendChild(this.controls.muteBox);
        this.controls.muteBox.appendChild(this.controls.muteButton);
        this.controls.muteBox.appendChild(this.controls.volumeBox);
        this.controls.volumeBox.appendChild(this.controls.volume);
        this.controls.enclosure.appendChild(this.controls.panel);
    },

    setStatusHidden: function(hidden)
    {
    },

    updateTime: function()
    {
        var currentTime = this.video.currentTime;
        var duration = this.video.duration;

        this.controls.currentTime.innerText = this.formatTime(currentTime);
        this.controls.timeline.value = currentTime;
        if (duration === Infinity || duration === NaN) {
            this.controls.remainingTime.classList.remove(this.ClassNames.show);
        } else {
            this.controls.currentTime.innerText += " / " + this.formatTime(duration);
            this.controls.remainingTime.innerText = this.formatTime(duration);
            this.controls.remainingTime.classList.add(this.ClassNames.show);
        }

        if (currentTime > 0)
            this.showCurrentTime();
    },

    showCurrentTime: function()
    {
        this.controls.currentTime.classList.remove(this.ClassNames.hidden);
        this.controls.remainingTime.classList.add(this.ClassNames.hidden);
    },

    handlePlay: function(event)
    {
        Controller.prototype.handlePlay.apply(this, arguments);
        this.showCurrentTime();
    },

    handleTimeUpdate: function(event)
    {
        this.updateTime();
    },

    handleMuteButtonMouseOver: function(event)
    {
        this.controls.volumeBox.classList.remove(this.ClassNames.hidden);
    },

    handleVolumeBoxMouseOut: function(event)
    {
        this.controls.volumeBox.classList.add(this.ClassNames.hidden);
    },

    addControls: function()
    {
        this.base.appendChild(this.controls.enclosure);
    },

    updateReadyState: function()
    {
        if (this.host.supportsFullscreen && this.video.videoTracks.length) {
            this.controls.fullscreenButton.classList.remove(this.ClassNames.hidden);
        } else {
            this.controls.fullscreenButton.classList.add(this.ClassNames.hidden);
        }
        if (this.video.offsetTop + this.controls.enclosure.offsetTop < 105)
            this.controls.volumeBox.classList.add(this.ClassNames.down);
        else
            this.controls.volumeBox.classList.remove(this.ClassNames.down);
        this.updateVolume();
    },

    setControlsType: function(type)
    {
        if (!this.controls.configured) {
            this.configureInlineControls();
            this.controls.configured = true;
            this.addControls();
        }
    },

    updatePlaying: function()
    {
        Controller.prototype.updatePlaying.apply(this, arguments);
        if (!this.canPlay()) {
            this.showControls();
        }
    },

    handleCaptionButtonClicked: function(event)
    {
        // Handled with mouseover and mouseout.
    },

    buildCaptionMenu: function()
    {
        Controller.prototype.buildCaptionMenu.apply(this, arguments);

        this.listenFor(this.captionMenu, 'mouseout', this.handleCaptionMouseOut);
        this.listenFor(this.captionMenu, 'transitionend', this.captionMenuTransitionEnd);

        this.captionMenu.captionMenuTreeElements = this.captionMenu.getElementsByTagName("*");

        // Caption menu has to be centered to the caption button.
        var captionButtonCenter =  this.controls.panel.offsetLeft + this.controls.captionButton.offsetLeft +
            this.controls.captionButton.offsetWidth / 2;
        this.captionMenu.style.left = (captionButtonCenter - this.captionMenu.offsetWidth / 2);
        // As height is not in the css, it needs to be specified to animate it.
        this.captionMenu.height = this.captionMenu.offsetHeight;
        this.captionMenu.style.height = 0;
    },

    destroyCaptionMenu: function()
    {
        this.hideCaptionMenu();
    },

    showCaptionMenu: function()
    {
        this.captionMenu.style.height = this.captionMenu.height;
    },

    hideCaptionMenu: function()
    {
        this.captionMenu.style.height = 0;
    },

    captionMenuTransitionEnd: function(event)
    {
        if (this.captionMenu.offsetHeight === 0)
            Controller.prototype.destroyCaptionMenu.apply(this, arguments);
    },

    handleCaptionButtonMouseOver: function(event)
    {
        if (!this.captionMenu)
            this.buildCaptionMenu();
        if (!contains(this.captionMenu.captionMenuTreeElements, event.relatedTarget))
            this.showCaptionMenu();
    },

    handleCaptionButtonMouseOut: function(event)
    {
        if (this.captionMenu && !contains(this.captionMenu.captionMenuTreeElements, event.relatedTarget))
            this.hideCaptionMenu();
    },

    handleCaptionMouseOut: function(event)
    {
        if (event.relatedTarget != this.controls.captionButton &&
            !contains(this.captionMenu.captionMenuTreeElements, event.relatedTarget))
            this.hideCaptionMenu();
    },
};

ControllerGtk.prototype.inheritFrom(Object.create(Controller.prototype));
Object.defineProperty(ControllerGtk.prototype, 'constructor', { enumerable:false, value:ControllerGtk });
