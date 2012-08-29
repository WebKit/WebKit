/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @interface
 */
WebInspector.OutputStreamDelegate = function()
{
}

WebInspector.OutputStreamDelegate.prototype = {
    onTransferStarted: function(source) { },

    onChunkTransferred: function(source) { },

    onTransferFinished: function(source) { },

    onError: function(source, event) { }
}

/**
 * @interface
 */
WebInspector.OutputStream = function()
{
}

WebInspector.OutputStream.prototype = {
    startTransfer: function() { },

    /**
     * @param {string} chunk
     */
    transferChunk: function(chunk) { },

    finishTransfer: function() { },

    dispose: function() { }
}

/**
 * @constructor
 * @param {!File} file
 * @param {number} chunkSize
 * @param {!WebInspector.OutputStreamDelegate} delegate
 */
WebInspector.ChunkedFileReader = function(file, chunkSize, delegate)
{
    this._file = file;
    this._fileSize = file.size;
    this._loadedSize = 0;
    this._chunkSize = chunkSize;
    this._delegate = delegate;
    this._isCanceled = false;
}

WebInspector.ChunkedFileReader.prototype = {
    /**
     * @param {!WebInspector.OutputStream} output
     */
    start: function(output)
    {
        this._output = output;

        this._reader = new FileReader();
        this._reader.onload = this._onChunkLoaded.bind(this);
        this._reader.onerror = this._delegate.onError.bind(this._delegate, this);
        this._output.startTransfer();
        this._delegate.onTransferStarted(this);
        this._loadChunk();
    },

    cancel: function()
    {
        this._isCanceled = true;
    },

    loadedSize: function()
    {
        return this._loadedSize;
    },

    fileSize: function()
    {
        return this._fileSize;
    },

    fileName: function()
    {
        return this._file.name;
    },

    /**
     * @param {Event} event
     */
    _onChunkLoaded: function(event)
    {
        if (this._isCanceled)
            return;

        if (event.target.readyState !== FileReader.DONE)
            return;

        var data = event.target.result;
        this._loadedSize += data.length;

        this._output.transferChunk(data);
        if (this._isCanceled)
            return;
        this._delegate.onChunkTransferred(this);

        if (this._loadedSize === this._fileSize) {
            this._file = null;
            this._reader = null;
            this._output.finishTransfer();
            this._delegate.onTransferFinished(this);
            return;
        }

        this._loadChunk();
    },

    _loadChunk: function()
    {
        var chunkStart = this._loadedSize;
        var chunkEnd = Math.min(this._fileSize, chunkStart + this._chunkSize)
        var nextPart = this._file.webkitSlice(chunkStart, chunkEnd);
        this._reader.readAsText(nextPart);
    }
}

/**
 * @constructor
 * @param {string} url
 * @param {!WebInspector.OutputStreamDelegate} delegate
 */
WebInspector.ChunkedXHRReader = function(url, delegate)
{
    this._url = url;
    this._delegate = delegate;
    this._fileSize = 0;
    this._loadedSize = 0;
    this._isCanceled = false;
}

WebInspector.ChunkedXHRReader.prototype = {
    /**
     * @param {!WebInspector.OutputStream} output
     */
    start: function(output)
    {
        this._output = output;

        this._xhr = new XMLHttpRequest();
        this._xhr.open("GET", this._url, true);
        this._xhr.onload = this._onLoad.bind(this);
        this._xhr.onprogress = this._onProgress.bind(this);
        this._xhr.onerror = this._delegate.onError.bind(this._delegate, this);
        this._xhr.send(null);

        this._output.startTransfer();
        this._delegate.onTransferStarted(this);
    },

    cancel: function()
    {
        this._isCanceled = true;
        this._xhr.abort();
    },

    loadedSize: function()
    {
        return this._loadedSize;
    },

    fileSize: function()
    {
        return this._fileSize;
    },

    fileName: function()
    {
        return this._url;
    },

    /**
     * @param {Event} event
     */
    _onProgress: function(event)
    {
        if (this._isCanceled)
            return;

        if (event.lengthComputable)
            this._fileSize = event.total;

        var data = this._xhr.responseText.substring(this._loadedSize);
        if (!data.length)
            return;

        this._loadedSize += data.length;
        this._output.transferChunk(data);
        if (this._isCanceled)
            return;
        this._delegate.onChunkTransferred(this);
    },

    /**
     * @param {Event} event
     */
    _onLoad: function(event)
    {
        this._onProgress(event);

        if (this._isCanceled)
            return;

        this._output.finishTransfer();
        this._delegate.onTransferFinished(this);
    }
}

/**
 * @param {function(File)} callback
 * @return {Node}
 */
WebInspector.createFileSelectorElement = function(callback) {
    var fileSelectorElement = document.createElement("input");
    fileSelectorElement.type = "file";
    fileSelectorElement.style.zIndex = -1;
    fileSelectorElement.style.position = "absolute";
    fileSelectorElement.onchange = function(event) {
        callback(fileSelectorElement.files[0]);
    };
    return fileSelectorElement;
}

/**
 * @param {string} source
 * @param {number=} startIndex
 * @param {number=} lastIndex
 */
WebInspector.findBalancedCurlyBrackets = function(source, startIndex, lastIndex) {
    lastIndex = lastIndex || source.length;
    startIndex = startIndex || 0;
    var counter = 0;
    var inString = false;

    for (var index = startIndex; index < lastIndex; ++index) {
        var character = source[index];
        if (inString) {
            if (character === "\\")
                ++index;
            else if (character === "\"")
                inString = false;
        } else {
            if (character === "\"")
                inString = true;
            else if (character === "{")
                ++counter;
            else if (character === "}") {
                if (--counter === 0)
                    return index + 1;
            }
        }
    }
    return -1;
}

/**
 * @constructor
 * @implements {WebInspector.OutputStream}
 * @param {string} fileName
 * @param {!WebInspector.OutputStreamDelegate} delegate
 */
WebInspector.FileOutputStream = function(fileName, delegate)
{
    this._fileName = fileName;
    this._delegate = delegate;
}

WebInspector.FileOutputStream.prototype = {
    /**
     * @override
     */
    startTransfer: function()
    {
        WebInspector.fileManager.addEventListener(WebInspector.FileManager.EventTypes.SavedURL, this._onTransferStarted, this);
        WebInspector.fileManager.save(this._fileName, "", true);
    },

    /**
     * @override
     * @param {string} chunk
     */
    transferChunk: function(chunk)
    {
        WebInspector.fileManager.append(this._fileName, chunk);
    },

    /**
     * @override
     */
    finishTransfer: function()
    {
        WebInspector.fileManager.removeEventListener(WebInspector.FileManager.EventTypes.AppendedToURL, this._onChunkTransferred, this);
        this._delegate.onTransferFinished(this);
    },

    dispose: function()
    {
    },

    /**
     * @param {WebInspector.Event} event
     */
    _onTransferStarted: function(event)
    {
        if (event.data !== this._fileName)
            return;
        this._delegate.onTransferStarted(this);
        WebInspector.fileManager.removeEventListener(WebInspector.FileManager.EventTypes.SavedURL, this._onTransferStarted, this);
        WebInspector.fileManager.addEventListener(WebInspector.FileManager.EventTypes.AppendedToURL, this._onChunkTransferred, this);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _onChunkTransferred: function(event)
    {
        if (event.data !== this._fileName)
            return;
        this._delegate.onChunkTransferred(this);
    }
}
