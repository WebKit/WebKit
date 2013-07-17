/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2013 Seokju Kwon (seokju.kwon@gmail.com)
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

if (!window.InspectorFrontendHost) {

    WebInspector.InspectorFrontendHostStub = function()
    {
        this._attachedWindowHeight = 0;
        this._fileBuffers = {};
    }

    WebInspector.InspectorFrontendHostStub.prototype = {
        platform: function()
        {
            var match = navigator.userAgent.match(/Windows NT/);
            if (match)
                return "windows";
            match = navigator.userAgent.match(/Mac OS X/);
            if (match)
                return "mac";
            return "linux";
        },

        port: function()
        {
            return "unknown";
        },

        bringToFront: function()
        {
            this._windowVisible = true;
        },

        closeWindow: function()
        {
            this._windowVisible = false;
        },

        requestSetDockSide: function(side)
        {
            InspectorFrontendAPI.setDockSide(side);
        },

        setAttachedWindowHeight: function(height)
        {
        },

        setAttachedWindowWidth: function(width)
        {
        },

        setToolbarHeight: function(width)
        {
        },

        moveWindowBy: function(x, y)
        {
        },

        loaded: function()
        {
        },

        localizedStringsURL: function()
        {
            return undefined;
        },

        inspectedURLChanged: function(title)
        {
            document.title = title;
        },

        copyText: function(text)
        {
            this._textToCopy = text;
            if (!document.execCommand("copy"))
                console.error("Clipboard access is denied");
        },

        openInNewTab: function(url)
        {
            window.open(url, "_blank");
        },

        canSave: function()
        {
            return true;
        },

        save: function(url, content, forceSaveAs)
        {
            if (this._fileBuffers[url])
                throw new Error("Concurrent file modification denied.");

            this._fileBuffers[url] = [content];
        },

        append: function(url, content)
        {
            var buffer = this._fileBuffers[url];
            if (!buffer)
                throw new Error("File is not open for write yet.");

            buffer.push(content);
        },

        close: function(url)
        {
            var content = this._fileBuffers[url];
            delete this._fileBuffers[url];

            if (!content)
                return;

            var lastSlashIndex = url.lastIndexOf("/");
            var fileNameSuffix = lastSlashIndex === -1 ? url : url.substring(lastSlashIndex + 1);

            var blob = new Blob(content, {type: "application/octet-stream"});
            var objectUrl = window.URL.createObjectURL(blob);
            window.location = objectUrl + "#" + fileNameSuffix;

            function cleanup()
            {
                window.URL.revokeObjectURL(objectUrl);
            }
            setTimeout(cleanup, 0);
        },

        sendMessageToBackend: function(message)
        {
        },

        loadResourceSynchronously: function(url)
        {
            var xhr = new XMLHttpRequest();
            xhr.open("GET", url, false);
            xhr.send(null);

            if (xhr.status === 200)
                return xhr.responseText;
            return null;
        }
    }

    InspectorFrontendHost = new WebInspector.InspectorFrontendHostStub();
}

