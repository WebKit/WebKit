/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * @constructor
 * @param {string} sourceMappingURL
 */
WebInspector.CompilerSourceMappingProvider = function(sourceMappingURL)
{
    if (!this._initialized) {
        window.addEventListener("message", this._onMessage, true);
        WebInspector.CompilerSourceMappingProvider.prototype._initialized = true;
    }
    this._sourceMappingURL = sourceMappingURL;
    this._frameURL = this._sourceMappingURL + ".html";
}

WebInspector.CompilerSourceMappingProvider.prototype = {
    /**
     * @param {function(WebInspector.CompilerSourceMapping)} callback
     */
    loadSourceMapping: function(callback)
    {
        this._frame = document.createElement("iframe");
        this._frame.src = this._frameURL;
        function frameLoaded()
        {
            function didLoadData(error, result)
            {
                if (error) {
                    console.log(error);
                    callback(null);
                    return;
                }

                var payload;
                try {
                    payload = JSON.parse(result);
                } catch (e) {
                    console.log("Failed to parse JSON.");
                }

                if (payload)
                    callback(new WebInspector.ClosureCompilerSourceMapping(payload));
                else
                    callback(null);
            }
            this._sendRequest("loadData", [this._sourceMappingURL], didLoadData);
        }
        this._frame.addEventListener("load", frameLoaded.bind(this), true);
        // FIXME: remove iframe from the document when it is not needed anymore.
        document.body.appendChild(this._frame);
    },

    /**
     * @param {string} sourceURL
     * @param {function(string)} callback
     * @param {number} timeout
     */
    loadSourceCode: function(sourceURL, callback, timeout)
    {
        function didSendRequest(error, result)
        {
            if (error) {
                console.log(error);
                callback("");
                return;
            }
            callback(result);
        }
        this._sendRequest("loadData", [sourceURL], didSendRequest, timeout);
    },

    _sendRequest: function(method, parameters, callback, timeout)
    {
        var requestId = this._requestId++;
        var timerId = setTimeout(this._cancelRequest.bind(this, requestId), timeout || 50);
        this._requests[requestId] = { callback: callback, timerId: timerId };
        var requestData = { id: requestId, method: method, params: parameters };
        this._frame.contentWindow.postMessage(requestData, this._frameURL);
    },

    _onMessage: function(event)
    {
        var requestId = event.data.id;
        var requests = WebInspector.CompilerSourceMappingProvider.prototype._requests;
        var request = requests[requestId];
        if (!request)
            return;
        delete requests[requestId];
        clearTimeout(request.timerId);
        request.callback(event.data.error, event.data.result);
    },

    _cancelRequest: function(requestId)
    {
        var request = this._requests[requestId];
        delete this._requests[requestId];
        request.callback("Request timed out.", null);
    },

    _requestId: 0,
    _requests: {}
}
