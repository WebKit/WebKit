function SourceBufferLoaderSimple(sb, ms, media, nbSeg)
{
    this.ms = ms;
    this.sb = sb;
    this.name = name;
    this.indexSeg = 0;
    this.nbSeg = nbSeg ? nbSeg : media.segments.length;
    if (this.nbSeg > media.segments.length)
        this.nbSeg = media.segments.length;
    this.segments = media.segments;
    this.promise = new Promise(function(resolve, reject) {
        this.promiseResolve = resolve;
        this.promiseReject = reject;
    }.bind(this));
};

SourceBufferLoaderSimple.prototype = {

    appendSegment : function(index)
    {
        const sourcebuff = this.sb;

        // If you want to concatenate the same file several times, as they have the same PTS, they will overwrite what is in the SourceBuffer already. With this
        // trick you can add an object with a timestampOffset property (for example { timestampOffset: 10.1 }) that will be applied at that moment for the following appends.
        if (this.segments[index].timestampOffset != undefined) {
            const timestampOffset = this.segments[index].timestampOffset;
            sourcebuff.timestampOffset = timestampOffset;
            this.onupdateend();
            return;
        }

        const segmentFile = this.segments[index];
        const request = new XMLHttpRequest();
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
        if (this.ms.readyState == "closed") {
            this.promiseReject(new Error("MediaSource is closed"));
            return;
        }

        if (this.indexSeg >= this.nbSeg) {
            this.promiseResolve();
            return;
        }

        this.appendSegment(this.indexSeg++);
    },

    start : function()
    {
        if (this.ms.readyState == "closed") {
            this.promiseReject(new Error("MediaSource is closed"));
            return this.promise;
        }

        if (this.indexSeg >= this.nbSeg) {
            this.promiseReject(new Error("No more segments"));
            return this.promise;
        }

        this.sb.addEventListener("updateend", this.onupdateend.bind(this));
        this.appendSegment(this.indexSeg++);
        return this.promise;
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

    // We are returning a promise that will be fulfilled when all appends are done.
    createSourceBuffer : function(media, maxSeg)
    {
        const sb = this.ms.addSourceBuffer(media.mimeType);
        const sbBase = new SourceBufferLoaderSimple(sb, this.ms, media, maxSeg);
        return sbBase.start();
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
