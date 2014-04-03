
function createPluginReplacement(root, parent, host, attributeNames, attributeValues)
{
    return new Replacement(root, parent, host, attributeNames, attributeValues);
};

function Replacement(root, parent, host, attributeNames, attributeValues)
{
    this.root = root;
    this.parent = parent;
    this.host = host;
    this.listeners = {};
    this.scriptObject = {};

    this.autoExitFullScreen = true;
    this.postEvents = false;
    this.height = 0;
    this.width = 0;
    this.src = "";
    this.autohref = false;
    this.href = "";
    this.qtsrc = "";
    this.baseUrl = "";
    this.target = "";

    this.createVideoElement(attributeNames, attributeValues);
    this.createScriptInterface();
};

Replacement.prototype = {

    HandledVideoEvents: {
        loadstart: 'handleLoadStart',
        error: 'handleError',
        loadedmetadata: 'handleLoadedMetaData',
        canplay: 'qt_canplay',
        canplaythrough: 'qt_canplaythrough',
        play: 'qt_play',
        pause: 'qt_pause',
        ended: 'handleEnded',
        webkitfullscreenchange: 'handleFullscreenChange',
    },

    AttributeMap: {
        autoexitfullscreen: 'autoExitFullScreen',
        postdomevents: 'postEvents',
        height: 'height',
        width: 'width',
        qtsrc: 'qtsrc',
        src: 'src',
        airplay: 'x-webkit-airplay=',
        href: 'href',
        target: 'target',
    },
    
    MethodMap: {
        SetURL : 'setURL',
        GetURL : 'url',
        Play : 'play',
        Stop : 'pause',
        GetRate : 'rate',
        SetRate : 'setRate',
        IsFullScreen : 'isFullScreen',
        ExitFullScreen : 'exitFullScreen',
        GetPluginStatus : 'pluginStatus',
        GetTime : 'currentTime',
        SetTime : 'setCurrentTime',
        SeekToDate : 'seekToDate',
        GetDate : 'date',
        GetDuration : 'duration',
        GetTimeScale : 'timeScale',
        GetMaxTimeLoaded : 'maxTimeLoaded',
        GetMaxBytesLoaded : 'maxBytesLoaded',
        GetMovieSize : 'movieSize',
        GetTimedMetadataUpdates : 'timedMetadataUpdates',
        GetAccessLog : 'accessLog',
        GetErrorLog : 'errorLog',
    },

    TimeScale: 30000,

    createVideoElement: function(attributeNames, attributeValues)
    {
        var video = this.video = document.createElement('video');

        for (name in this.HandledVideoEvents)
            video.addEventListener(name, this, false);
        
        for (i = 0; i < attributeNames.length; i++) {
            var property = this.AttributeMap[attributeNames[i]];
            if (this[property] != undefined)
                this[property] = attributeValues[i];
        }

        video.setAttribute('pseudo', '-webkit-plugin-replacement');
        video.setAttribute('controls', 'controls');
        this.setStatus('Waiting');

        var src = this.resolveRelativeToUrl(this.src, "");
        this.baseUrl = src;

        // The 'qtsrc' attribute is used when a page author wanted to always use the QuickTime plug-in
        // to load a media type even if another plug-in was registered for that type. It tells the
        // plug-in to ignore the 'src' url, and to load the 'qtsrc' url instead.
        if (this.qtsrc)
            src = this.resolveRelativeToUrl(this.qtsrc, this.src);
        if (this.href && this.target) {
            src = this.resolveRelativeToUrl(this.href, this.src);
            video.poster = this.src;
            video.setAttribute('preload', 'none');
        }
        
        if (src.length) {
            this.setStatus('Validating');
            this.video.src = src;
        }

        this.root.appendChild(video);
    },

    resolveRelativeToUrl: function(url, baseUrl)
    {
        if (url.indexOf('://') != -1)
            return url;
        if (baseUrl.indexOf('://') == -1)
            baseUrl = this.resolveRelativeToUrl(baseUrl, document.baseURI);

        var base = document.createElement('base');
        base.href = baseUrl;
        document.head.appendChild(base);

        var resolver = document.createElement('a');
        resolver.href = url;
        url = resolver.href;

        document.head.removeChild(base);
        base = null;

        return url;
    },
 
    createScriptInterface: function()
    {
        for (name in this.MethodMap) {
            var methodName = this.MethodMap[name];
            this.scriptObject[name] = this[methodName].bind(this);
        }
    },

    handleEvent: function(event)
    {
        if (event.target !== this.video)
            return;

        try {
            var eventData = this.HandledVideoEvents[event.type];
            if (!eventData)
                return;

            if (this[eventData] && this[eventData] instanceof Function)
                this[eventData].call(this, event);
            else
                this.postEvent(eventData);
        } catch(e) {
            if (window.console)
                console.error(e);
        }
    },

    postEvent: function(eventName)
    {
        try {
            if (this.postEvents)
                this.host.postEvent(eventName);
        } catch(e) { }
    },

    setStatus: function(status)
    {
        this.status = status;
    },

    handleLoadedMetaData: function(event)
    {
        this.setStatus('Playable');
        this.postEvent('qt_validated');
        this.postEvent('qt_loadedfirstframe');
        this.postEvent('qt_loadedmetadata');
    },

    handleFullscreenChange: function(event)
    {
        this.postEvent(this.isFullScreen() ? 'qt_enterfullscreen' : 'qt_exitfullscreen');
    },

    handleError: function(event)
    {
        this.setStatus('Error');
        this.postEvent('qt_error');
    },

    handleLoadStart:function(event)
    {
        if (this.video.poster)
            this.setStatus('Waiting');
        else
            this.setStatus('Loading');
        this.postEvent('qt_begin');
    },

    handleEnded: function(event)
    {
        this.postEvent('qt_ended');
        if (this.isFullScreen() && this.autoExitFullScreen)
            document.webkitExitFullscreen();
    },

    isFullScreen: function()
    {
        return document.webkitCurrentFullScreenElement === this.video;
    },

    setURL: function(url)
    {
        this.setStatus('Validating');
        if (url.length)
            url = this.resolveRelativeToUrl(url, this.baseUrl);
        this.video.src = url;
    },
    
    url: function()
    {
        return this.video.currentSrc;
    },
    
    play: function()
    {
        this.video.play();
    },
    
    pause: function()
    {
        this.video.playbackRate = 0;
        this.video.pause();
    },
    
    rate: function()
    {
        return this.video.paused ? 0 : 1;
    },
    
    setRate: function(rate)
    {
        if (rate)
            this.video.play();
        else
            this.video.pause();
    },
    
    exitFullScreen: function()
    {
        document.webkitExitFullscreen();
    },
    
    pluginStatus: function()
    {
        return this.status;
    },

    currentTime: function()
    {
        return this.video.currentTime * this.TimeScale;
    },

    setCurrentTime: function(time)
    {
        this.video.currentTime = time / this.TimeScale;
    },

    seekToDate: function()
    {
        // FIXME: not implemented yet.
    },
    
    date: function()
    {
        return new Date();
    },
    
    duration: function()
    {
        return this.video.duration * this.TimeScale;
    },
    
    timeScale: function()
    {
        // Note: QuickTime movies and MPEG-4 files have a timescale, but it is not exposed by all media engines.
        // 30000 works well with common frame rates, eg. 29.97 NTSC can be represented accurately as a time
        // scale of 30000 and frame duration of 1001.
        return 30000;
    },
    
    maxTimeLoaded: function()
    {
        return this.video.duration * this.TimeScale;
    },
    
    maxBytesLoaded: function()
    {
        var percentLoaded = this.video.buffered.end(0) / this.video.duration;
        return percentLoaded * this.movieSize();
    },
    
    movieSize: function()
    {
        try {
            return this.host.movieSize;
        } catch(e) { }

        return 0;
    },
    
    timedMetadataUpdates: function()
    {
        try {
            return this.host.timedMetaData;
        } catch(e) { }

        return null;
    },
    
    accessLog: function()
    {
        try {
            return this.host.accessLog;
        } catch(e) { }

        return null;
    },
    
    errorLog: function()
    {
        try {
            return this.host.errorLog;
        } catch(e) { }

        return null;
    },
};

