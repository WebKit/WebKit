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
 * @constructor
 * @implements {WebInspector.ResourceDomainModelBinding}
 * @param {WebInspector.UISourceCodeProvider} uiSourceCodeProvider
 */
WebInspector.DebuggerResourceBinding = function(uiSourceCodeProvider)
{
    this._uiSourceCodeProvider = uiSourceCodeProvider;
    WebInspector.Resource.registerDomainModelBinding(WebInspector.resourceTypes.Script, this);
}

/**
 * @param {WebInspector.UISourceCode} uiSourceCode
 * @param {string} newSource
 * @param {function(?Protocol.Error)} callback
 */
WebInspector.DebuggerResourceBinding.setScriptSource = function(uiSourceCode, newSource, callback)
{
    var rawLocation = uiSourceCode.uiLocationToRawLocation(0, 0);
    var script = WebInspector.debuggerModel.scriptForId(rawLocation.scriptId);

    /**
     * @this {WebInspector.DebuggerResourceBinding}
     * @param {?Protocol.Error} error
     */
    function didEditScriptSource(error)
    {
        if (error) {
            callback(error);
            return;
        }

        var resource = uiSourceCode.resource();
        if (resource)
            resource.addRevision(newSource);

        callback(null);
    }
    WebInspector.debuggerModel.setScriptSource(script.scriptId, newSource, didEditScriptSource.bind(this));
}

WebInspector.DebuggerResourceBinding.prototype = {
    /**
     * @param {WebInspector.Resource} resource
     * @return {boolean}
     */
    canSetContent: function(resource)
    {
        var uiSourceCode = WebInspector.JavaScriptSource.javaScriptSourceForResource.get(resource);
        return !!uiSourceCode && uiSourceCode.isEditable();
    },

    /**
     * @param {WebInspector.Resource} resource
     * @param {string} content
     * @param {boolean} majorChange
     * @param {function(?string)} userCallback
     */
    setContent: function(resource, content, majorChange, userCallback)
    {
        if (!majorChange)
            return;

        var uiSourceCode = WebInspector.JavaScriptSource.javaScriptSourceForResource.get(resource);
        if (!uiSourceCode) {
            userCallback("Resource is not editable");
            return;
        }

        resource.requestContent(this._setContentWithInitialContent.bind(this, uiSourceCode, content, userCallback));
    },

    /**
     * @param {WebInspector.UISourceCode} uiSourceCode
     * @param {string} content
     * @param {function(?string)} userCallback
     * @param {?string} oldContent
     * @param {boolean} oldContentEncoded
     * @param {string} mimeType
     */
    _setContentWithInitialContent: function(uiSourceCode, content, userCallback, oldContent, oldContentEncoded, mimeType)
    {
        /**
         * @this {WebInspector.DebuggerResourceBinding}
         * @param {?string} error
         */
        function callback(error)
        {
            if (userCallback)
                userCallback(error);
        }
        WebInspector.DebuggerResourceBinding.setScriptSource(uiSourceCode, content, callback.bind(this));
    }
}

WebInspector.DebuggerResourceBinding.prototype.__proto__ = WebInspector.ResourceDomainModelBinding.prototype;
