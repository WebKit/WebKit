function MSE(parameters) {
    this.video = parameters.video;
    this.mediaURL = parameters.mediaURL;
    this.length = parameters.length;
    this.source = new MediaSource();
    this.source.addEventListener('sourceopen', this.handleSourceOpen.bind(this), false);
    this.onupdatestart = null;
    this.onupdateend = null;
    this.onfetchfailed = null;
}

function formatRangeHeader( start, end ) {
    return 'bytes=' + start + '-' + end;
}

MSE.prototype = {
    start: function() {
        this.video.src = URL.createObjectURL(this.source);
    },

    handleSourceOpen: function(event) {
        this.sourceBuffer = this.source.addSourceBuffer('video/mp4; codecs="avc1.4D401F"');
        this.sourceBuffer.addEventListener('updatestart', this.handleSourceBufferUpdateStart.bind(this), false);
        this.sourceBuffer.addEventListener('updateend', this.handleSourceBufferUpdateEnd.bind(this), false);

        this.fetchChunk(0, this.length - 1);
    },

    fetchNext: function(length) {
        var oldLength = this.length;
        this.length += length;

        this.fetchChunk(oldLength, this.length - 1);
    },

    fetchChunk: function(from, to) {
        var request = new XMLHttpRequest();
        request.open('GET', this.mediaURL, true);
        request.setRequestHeader('Range', formatRangeHeader(from, to));
        request.setRequestHeader('Cache-control', 'no-cache');
        request.responseType = 'arraybuffer';
        request.onload = this.handleChunkLoaded.bind(this);
        request.onerror = this.handleChunkError.bind(this);
        request.send();
    },

    handleChunkLoaded: function(event) {
        var request = event.target;
        var buffer = request.response;
        this.sourceBuffer.appendBuffer(buffer);
    },

    handleChunkError: function(event) {
        consoleWrite('FAIL: error downloading media data:' + event.target.statusText);

        if (this.onfetchfailed)
            this.onfetchfailed();
    },

    handleSourceBufferUpdateStart: function(event) {
        if (this.onupdatestart)
            this.onupdatestart();
    },

    handleSourceBufferUpdateEnd: function(event) {
        if (this.onupdateend)
            this.onupdateend();
    },

    removeAllSamples: function() {
        this.sourceBuffer.remove(0, this.source.duration);
    },
};
