/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Saam Barati <saambarati1@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.Annotator = function(sourceCodeTextEditor)
{
    WebInspector.Object.call(this);

    console.assert(sourceCodeTextEditor instanceof WebInspector.SourceCodeTextEditor, sourceCodeTextEditor);

    this._sourceCodeTextEditor = sourceCodeTextEditor;
    this._timeoutIdentifier = null;
    this._isActive = false;
};

WebInspector.Annotator.prototype = {
    constructor: WebInspector.Annotator,
    __proto__: WebInspector.Object.prototype,

    // Public

    get sourceCodeTextEditor()
    {
        return this._sourceCodeTextEditor;
    },

    isActive: function()
    {
        return this._isActive;
    },

    pause: function()
    {
        this._clearTimeoutIfNeeded();
        this._isActive = false;
    },

    resume: function()
    {
        this._clearTimeoutIfNeeded();
        this._isActive = true;
        this.insertAnnotations();
    },

    refresh: function()
    {
        console.assert(this._isActive);
        if (!this._isActive)
            return;

        this._clearTimeoutIfNeeded();
        this.insertAnnotations();
    },

    reset: function()
    {
        this._clearTimeoutIfNeeded();
        this._isActive = true;
        this.clearAnnotations();
        this.insertAnnotations();
    },

    clear: function()
    {
        this.pause();
        this.clearAnnotations();
    },

    // Protected

    insertAnnotations: function()
    {
        // Implemented by subclasses.
    },

    clearAnnotations: function()
    {
        // Implemented by subclasses.
    },

    // Private

    _clearTimeoutIfNeeded: function()
    {
        if (this._timeoutIdentifier) {
            clearTimeout(this._timeoutIdentifier);
            this._timeoutIdentifier = null;
        }
    }
};
