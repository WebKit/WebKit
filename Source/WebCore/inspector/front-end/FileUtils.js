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
 * @param {!number} chunkSize
 * @param {!WebInspector.OutputStreamDelegate} delegate
 */
WebInspector.ChunkedFileReader = function(file, chunkSize, delegate)
{
    this._file = file;
    this._fileSize = file.size;
    this._loadedSize = 0;
    this._chunkSize = chunkSize;
    this._delegate = delegate;
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
        if (event.target.readyState !== FileReader.DONE)
            return;

        var data = event.target.result;
        this._loadedSize += data.length;

        this._output.transferChunk(data);
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
 * @param {!function(File)} callback
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
