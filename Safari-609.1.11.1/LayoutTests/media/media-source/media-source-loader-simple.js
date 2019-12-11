function SourceBufferLoaderSimple(sb, ms, media, nbSeg)
{
    this.ms = ms;
    this.sb = sb;
    this.name = name;
    this.indexSeg = 0;
    this.nbSeg = nbSeg ? nbSeg : media.segments.length;
    this.segments = media.segments;

};

SourceBufferLoaderSimple.prototype = {

    appendSegment : function(index)
    {
        const segmentFile = this.segments[index];
        const request = new XMLHttpRequest();
        const sourcebuff = this.sb;
        request.open("GET", segmentFile);
        request.responseType = "arraybuffer";
        request.addEventListener("load", function() {
            sourcebuff.appendBuffer(new Uint8Array(request.response));
        });
        request.addEventListener("error", function() {
            logResult(false, "Error request " + segmentFile);
            endTest();
        });
        request.addEventListener("abort", function() {
            logResult(false, "Aborted request" + segmentFile);
            endTest();
        });
        request.send(null);
    },

    onupdateend : function()
    {
        if (this.ms.readyState == "closed")
            return;

        if (this.indexSeg >= this.nbSeg)
            return;

        this.appendSegment(this.indexSeg++);
    },

    start : function()
    {
        if (this.ms.readyState == "closed")
            return;

        if (this.indexSeg >= this.nbSeg)
            return;

        this.sb.addEventListener("updateend", this.onupdateend.bind(this));
        this.appendSegment(this.indexSeg++);
    }
};

function MediaSourceLoaderSimple(video)
{
    this.video = video;
    this.ms = new MediaSource();
    this.onSourceOpenBind = this.onsourceopen.bind(this);
    this.ms.addEventListener("sourceopen", this.onSourceOpenBind);
    this.video.addEventListener("canplay", this.oncanplay.bind(this));
    this.video.src = window.URL.createObjectURL(this.ms);
    this.onready = function() {
    };
};

MediaSourceLoaderSimple.prototype = {
    createSourceBuffer : function(media, maxSeg)
    {
        const sb = this.ms.addSourceBuffer(media.mimeType);
        const sbBase = new SourceBufferLoaderSimple(sb, this.ms, media, maxSeg);
        sbBase.start();
    },

    onsourceopen : function()
    {
        this.ms.removeEventListener("sourceopen", this.onSourceOpenBind);
        this.onSourceOpenBind = undefined;
        this.onready();
    },

    oncanplay : function()
    {
        this.video.play();
    }
};
