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

WebInspector.AnnotatorManager = function()
{
    WebInspector.Object.call(this);

    this._annotators = new Map;
};

WebInspector.AnnotatorManager.prototype = {
    constructor: WebInspector.AnnotatorManager,
    __proto__: WebInspector.Object.prototype,

    // Public

    addAnnotator: function(key, annotator)
    {
        console.assert(annotator instanceof WebInspector.Annotator);
        this._annotators.set(key, annotator);
    },

    getAnnotator: function(key)
    {
        return this._annotators.get(key) || null;
    },

    isAnnotatorActive: function(key)
    {
        var annotator = this.getAnnotator(key);
        if (!annotator)
            return false;
        return annotator.isActive;
    },

    pauseAll: function()
    {
        for (var annotator of this._annotators.values())
            annotator.pause();
    },

    resumeAll: function()
    {
        for (var annotator of this._annotators.values())
            annotator.resume();
    },

    refreshAllIfActive: function()
    {
        for (var annotator of this._annotators.values()) {
            if (annotator.isActive)
                annotator.refresh();
        }
    },

    resetAllIfActive: function()
    {
        for (var annotator of this._annotators.values()) {
            if (annotator.isActive)
                annotator.reset();
        }
    },

    clearAll: function()
    {
        for (var annotator of this._annotators.values())
            annotator.clear();
    },

    removeAllAnnotators: function()
    {
        this._annotators.clear();
    }
};
