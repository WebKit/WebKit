function BufferLoader(context, urlList, callback) {
    this.context = context;
    this.urlList = urlList;
    this.onload = callback;
    this.bufferList = new Array();
    this.loadCount = 0;
}

BufferLoader.prototype.loadBuffer = function(url, index) {
    // Load buffer asynchronously
    var request = new XMLHttpRequest();
    request.open("GET", url, true);
    request.responseType = "arraybuffer";

    var loader = this;

    request.onload = function() {
        var buffer;
        try {
            buffer = loader.context.createBuffer(request.response, false);
        } catch(e) {
            alert('error decoding file data: ' + url);
        }

        try {
            loader.bufferList[index] = buffer;
            if (++loader.loadCount == loader.urlList.length)
                loader.onload(loader.bufferList);
        } catch(e) {
            alert('BufferLoader: callback problem');
        }
    }

    request.onerror = function() {
        alert('BufferLoader: XHR error');        
    }

    request.send();
}

BufferLoader.prototype.load = function() {
    for (var i = 0; i < this.urlList.length; ++i)
        this.loadBuffer(this.urlList[i], i);
}
