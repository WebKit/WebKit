function MediaSourceLoader(url)
{
    this._url = url;
    setTimeout(this.loadManifest.bind(this));

    this.onload = null;
    this.onerror = null;
}

MediaSourceLoader.prototype = {
    loadManifest: function()
    {
        var request = new XMLHttpRequest();
        request.open('GET', this._url, true);
        request.responseType = 'json';
        request.onload = this.loadManifestSucceeded.bind(this);
        request.onerror = this.loadManifestFailed.bind(this);
        request.send();
    },

    loadManifestSucceeded: function(e)
    {
        this._manifest = e.target.response;

        if (!this._manifest || !this._manifest.url) {
            if (this.onerror)
                this.onerror();
            return;
        }

        this.loadMediaData();
    },

    loadManifestFailed: function()
    {
        if (this.onerror)
            this.onerror();
    },

    loadMediaData: function()
    {
        var request = new XMLHttpRequest();
        request.open('GET', this._manifest.url, true);
        request.responseType = 'arraybuffer';
        request.onload = this.loadMediaDataSucceeded.bind(this);
        request.onerror = this.loadMediaDataFailed.bind(this);
        request.send();
    },

    loadMediaDataSucceeded: function(e)
    {
        this._mediaData = e.target.response;

        if (this.onload)
            this.onload();
    },

    loadMediaDataFailed: function()
    {
        if (this.onerror)
            this.onerror();
    },

    type: function()
    {
        return this._manifest ? this._manifest.type : "";
    },

    duration: function()
    {
        return this._manifest ? this._manifest.duration : 0
    },

    initSegment: function()
    {
        if (!this._manifest || !this._manifest.init || !this._mediaData)
            return null;
        var init = this._manifest.init;
        return this._mediaData.slice(init.offset, init.offset + init.size);
    },

    mediaSegmentsLength: function()
    {
        if (!this._manifest || !this._manifest.media)
            return 0;
        return this._manifest.media.length;   
    },

    mediaSegment: function(segmentNumber)
    {
        if (!this._manifest || !this._manifest.media || !this._mediaData || segmentNumber >= this._manifest.media.length)
            return null;
        var media = this._manifest.media[segmentNumber];
        return this._mediaData.slice(media.offset, media.offset + media.size);
    },
};