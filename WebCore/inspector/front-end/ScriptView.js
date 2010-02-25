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

    this.script = script;

    this._frameNeedsSetup = true;
    this._sourceFrameSetup = false;
    this.sourceFrame = new WebInspector.SourceFrame(this.element, this._addBreakpoint.bind(this), this._removeBreakpoint.bind(this));
}

WebInspector.ScriptView.prototype = {
    show: function(parentElement)
    {
        WebInspector.View.prototype.show.call(this, parentElement);
        this.setupSourceFrameIfNeeded();
        this.sourceFrame.visible = true;
        this.resize();
    },

    setupSourceFrameIfNeeded: function()
    {
        if (!this._frameNeedsSetup)
            return;

        this.attach();

        var prefix = "";
        for (var i = 0; i < this.script.startingLine - 1; ++i)
            prefix += "\n";

        this.sourceFrame.setContent("text/javascript", prefix + this.script.source);
        this._sourceFrameSetup = true;
        delete this._frameNeedsSetup;
    },

    attach: function()
    {
        if (!this.element.parentNode)
            document.getElementById("script-resource-views").appendChild(this.element);
    },

    _addBreakpoint: function(line)
    {
        var breakpoint = new WebInspector.Breakpoint(this.script.sourceURL, line, this.script.sourceID);
        WebInspector.panels.scripts.addBreakpoint(breakpoint);
    },

    // The follow methods are pulled from SourceView, since they are
    // generic and work with ScriptView just fine.

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
    _sourceFrameSetupFinished: WebInspector.SourceView.prototype._sourceFrameSetupFinished,
    _removeBreakpoint: WebInspector.SourceView.prototype._removeBreakpoint,
    resize: WebInspector.SourceView.prototype.resize
}

WebInspector.ScriptView.prototype.__proto__ = WebInspector.View.prototype;
