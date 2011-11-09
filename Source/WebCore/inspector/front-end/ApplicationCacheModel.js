/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 * @extends {WebInspector.Object}
 */
WebInspector.ApplicationCacheModel = function()
{
    InspectorBackend.registerApplicationCacheDispatcher(new WebInspector.ApplicationCacheDispatcher(this));
}

WebInspector.ApplicationCacheModel.prototype = {
    getApplicationCachesAsync: function(callback)
    {
        function mycallback(error, applicationCaches)
        {
            // FIXME: Currently, this list only returns a single application cache.
            if (!error && applicationCaches)
                callback(applicationCaches);
        }

        ApplicationCacheAgent.getApplicationCaches(mycallback);
    }
}

WebInspector.ApplicationCacheModel.prototype.__proto__ = WebInspector.Object.prototype;

/**
 * @constructor
 * @implements {ApplicationCacheAgent.Dispatcher}
 */
WebInspector.ApplicationCacheDispatcher = function(applicationCacheModel)
{
    this._applicationCacheModel = applicationCacheModel;
}

WebInspector.ApplicationCacheDispatcher.prototype = {
    /**
     * @param {number} status
     */
    updateApplicationCacheStatus: function(status)
    {
        WebInspector.panels.resources.updateApplicationCacheStatus(status);
    },

    /**
     * @param {boolean} isNowOnline
     */
    updateNetworkState: function(isNowOnline)
    {
        WebInspector.panels.resources.updateNetworkState(isNowOnline);
    }
}
