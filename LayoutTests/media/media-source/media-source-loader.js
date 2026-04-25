function MediaSourceLoader(url, prefix)
{
    this._url = url;
    this._prefix = prefix;
    this._manifestTimeout = setTimeout(this.loadManifest.bind(this));

    this.onload = null;
    this.onerror = null;
}

MediaSourceLoader.prototype = {
    load: async function()
    {
        if (this._manifestTimeout) {
            clearTimeout(this._manifestTimeout);
            this._manifestTimeout = null;
        }
        return this.loadManifest();
    },

    loadManifest: async function()
    {
        try {
            let fetchResult = await fetch(this._url);
            let manifest = await fetchResult.json();

            if (manifest && manifest.url) {
                this._manifest = manifest;
                return this.loadMediaData();
            }
        } catch(e) {
        }

        if (this.onerror)
            this.onerror();
        return Promise.reject("Failed loading manifest");
    },

    loadMediaData: async function()
    {
        try {
            let url = (this._prefix ? this._prefix : '') + this._manifest.url;
            let fetchResult = await fetch(url);
            let arrayBuffer = await fetchResult.arrayBuffer();
            this._mediaData = arrayBuffer;
            if (this.onload)
                this.onload();
            return;
        } catch(e) {
        }

        if(this.onerror)
            this.onerror();
        return Promise.reject("Failed loading media data");
    },

    type: function()
    {
        return this._manifest ? this._manifest.type : "";
    },

    duration: function()
    {
        return this._manifest ? this._manifest.duration : 0
    },

    initSegmentSize: function()
    {
        if (!this._manifest || !this._manifest.init || !this._mediaData)
            return null;
        var init = this._manifest.init;
        return init.size;
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

    mediaSegmentSize: function(segmentNumber)
    {
        if (!this._manifest || !this._manifest.media || !this._mediaData || segmentNumber >= this._manifest.media.length)
            return 0;
        var media = this._manifest.media[segmentNumber];
        return media.size;
    },

    mediaSegmentEndTime: function(segmentNumber)
    {
        if (!this._manifest || !this._manifest.media || !this._mediaData || segmentNumber >= this._manifest.media.length)
            return 0;
        var media = this._manifest.media[segmentNumber];
        return media.timestamp + media.duration;
    },

    concatenateMediaSegments: function(segmentDataList)
    {
        var totalLength = 0;
        segmentDataList.forEach(segment => totalLength += segment.byteLength);
        var view = new Uint8Array(totalLength);
        var offset = 0;
        segmentDataList.forEach(segment => {
            view.set(new Uint8Array(segment), offset);
            offset += segment.byteLength;
        });
        return view.buffer;
    },
};
