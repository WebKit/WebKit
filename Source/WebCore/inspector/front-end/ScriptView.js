/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.ScriptView = function(script)
{
    WebInspector.View.call(this);

    this.element.addStyleClass("script-view");

    var contentProvider = new WebInspector.SourceFrameContentProviderForScript(script);
    this.sourceFrame = new WebInspector.SourceFrame(this.element, contentProvider, "", true);
}

WebInspector.ScriptView.prototype = {
    // The following methods are pulled from SourceView, since they are
    // generic and work with ScriptView just fine.

    show: WebInspector.SourceView.prototype.show,
    hide: WebInspector.SourceView.prototype.hide,
    revealLine: WebInspector.SourceView.prototype.revealLine,
    highlightLine: WebInspector.SourceView.prototype.highlightLine,
    addMessage: WebInspector.SourceView.prototype.addMessage,
    clearMessages: WebInspector.SourceView.prototype.clearMessages,
    searchCanceled: WebInspector.SourceView.prototype.searchCanceled,
    performSearch: WebInspector.SourceView.prototype.performSearch,
    jumpToFirstSearchResult: WebInspector.SourceView.prototype.jumpToFirstSearchResult,
    jumpToLastSearchResult: WebInspector.SourceView.prototype.jumpToLastSearchResult,
    jumpToNextSearchResult: WebInspector.SourceView.prototype.jumpToNextSearchResult,
    jumpToPreviousSearchResult: WebInspector.SourceView.prototype.jumpToPreviousSearchResult,
    showingFirstSearchResult: WebInspector.SourceView.prototype.showingFirstSearchResult,
    showingLastSearchResult: WebInspector.SourceView.prototype.showingLastSearchResult,
    _jumpToSearchResult: WebInspector.SourceView.prototype._jumpToSearchResult,
    resize: WebInspector.SourceView.prototype.resize
}

WebInspector.ScriptView.prototype.__proto__ = WebInspector.View.prototype;


WebInspector.SourceFrameContentProviderForScript = function(script)
{
    WebInspector.SourceFrameContentProvider.call(this);
    this._script = script;
}

WebInspector.SourceFrameContentProviderForScript.prototype = {
    requestContent: function(callback)
    {
        if (this._script.source) {
            callback("text/javascript", this._script.source);
            return;
        }

        function didRequestSource(content)
        {
            var source;
            if (content) {
                var prefix = "";
                for (var i = 0; i < this._script.startingLine - 1; ++i)
                    prefix += "\n";
                source = prefix + content;
            } else
                source = WebInspector.UIString("<source is not available>");
            callback("text/javascript", source);
        }
        this._script.requestSource(didRequestSource.bind(this));
    },

    scripts: function()
    {
        return [this._script];
    }
}

WebInspector.SourceFrameContentProviderForScript.prototype.__proto__ = WebInspector.SourceFrameContentProvider.prototype;
