/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * @extends {WebInspector.UISourceCode}
 * @param {WebInspector.Resource} resource
 */
WebInspector.StyleSource = function(resource)
{
    WebInspector.UISourceCode.call(this, resource.url, resource, resource);
}

WebInspector.StyleSource.updateTimeout = 200;

WebInspector.StyleSource.prototype = {
    /**
     * @return {boolean}
     */
    isEditable: function()
    {
        return true;
    },

    /**
     * @param {function(?string)} callback
     */
    workingCopyCommitted: function(callback)
    {  
        this._commitIncrementalEdit(true, callback);
    },

    workingCopyChanged: function()
    {
        this._callOrSetTimeout(this._commitIncrementalEdit.bind(this, false, function() {}));
    },

    /**
     * @param {function(?string)} callback
     */
    _callOrSetTimeout: function(callback)
    {
        // FIXME: Extensions tests override updateTimeout because extensions don't have any control over applying changes to domain specific bindings.   
        if (WebInspector.StyleSource.updateTimeout >= 0)
            this._incrementalUpdateTimer = setTimeout(callback, WebInspector.StyleSource.updateTimeout);
        else
            callback(null);
    },

    /**
     * @param {boolean} majorChange
     * @param {function(?string)} callback
     */
    _commitIncrementalEdit: function(majorChange, callback)
    {
        this._clearIncrementalUpdateTimer();
        WebInspector.cssModel.resourceBinding().setStyleContent(this, this.workingCopy(), majorChange, callback);
    },

    _clearIncrementalUpdateTimer: function()
    {
        if (this._incrementalUpdateTimer)
            clearTimeout(this._incrementalUpdateTimer);
        delete this._incrementalUpdateTimer;
    }
}

WebInspector.StyleSource.prototype.__proto__ = WebInspector.UISourceCode.prototype;
