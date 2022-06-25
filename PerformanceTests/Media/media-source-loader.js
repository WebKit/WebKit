class MediaSourceLoader {
    constructor(url)
    {
        this._url = url;
        this.onload = null;
        this.onerror = null;
    }

    loadManifest()
    {
        return new Promise((resolve, reject) => {
            if (this._manifest) {
                resolve();
                return;
            }

            var request = new XMLHttpRequest();
            request.open('GET', this._url, true);
            request.responseType = 'json';
            request.onload = (event) => {
                this.loadManifestSucceeded(event);
                resolve();
            }
            request.onerror = (event) => {
                this.loadManifestFailed(event);
                reject(event);
            }
            request.send();
        })
    }

    loadManifestSucceeded(event)
    {
        this._manifest = event.target.response;

        if (!this._manifest || !this._manifest.url) {
            if (this.onerror)
                this.onerror();
            return;
        }
    }

    loadManifestFailed()
    {
        if (this.onerror)
            this.onerror();
    }

    loadMediaData()
    {
        return new Promise((resolve, reject) => {
            this.loadManifest().then(() => {
                var request = new XMLHttpRequest();
                request.open('GET', this._manifest.url, true);
                request.responseType = 'arraybuffer';
                request.onload = (event) => {
                    this.loadMediaDataSucceeded(event);
                    resolve();
                }
                request.onerror = (event) => {
                    this.loadMediaDataFailed(event);
                    reject(event);
                }
                request.send();
            });
        });
    }

    loadMediaDataSucceeded(event)
    {
        this._mediaData = event.target.response;

        if (this.onload)
            this.onload();
    }

    loadMediaDataFailed()
    {
        if (this.onerror)
            this.onerror();
    }

    get type()
    {
        return this._manifest ? this._manifest.type : "";
    }

    get duration()
    {
        if (!this._manifest)
            return 0;
        return this._manifest.media.reduce((duration, media) => { return duration + media.duration }, 0);
    }

    get initSegment()
    {
        if (!this._manifest || !this._manifest.init || !this._mediaData)
            return null;
        var init = this._manifest.init;
        return this._mediaData.slice(init.offset, init.offset + init.size);
    }

    get mediaSegmentsLength()
    {
        if (!this._manifest || !this._manifest.media)
            return 0;
        return this._manifest.media.length;   
    }

    *mediaSegments()
    {
        if (!this._manifest || !this._manifest.media || !this._mediaData)
            return;

        for (var media of this._manifest.media)
            yield this._mediaData.slice(media.offset, media.offset + media.size);
    }

    get everyMediaSegment()
    {
        if (!this._manifest || !this._manifest.media || !this._mediaData)
            return null;

        return this._mediaData.slice(this._manifest.media[0].offset);
    }
};