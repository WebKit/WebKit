/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

WebInspector.ResourceContentView = function(resource, styleClassName)
{
    console.assert(resource);
    console.assert(styleClassName);

    WebInspector.ContentView.call(this, resource);

    this._resource = resource;

    this.element.classList.add(styleClassName, "resource");

    // Append a spinner while waiting for contentAvailable. The subclasses are responsible for removing
    // the spinner before showing the resource content.
    var spinner = new WebInspector.IndeterminateProgressSpinner;
    this.element.appendChild(spinner.element);

    this.element.addEventListener("click", this._mouseWasClicked.bind(this), false);

    // Request content last so the spinner will always be removed in case the content is immediately available.
    resource.requestContent().then(this._contentAvailable.bind(this)).catch(this._protocolError.bind(this));

    if (!this.managesOwnIssues) {
        WebInspector.issueManager.addEventListener(WebInspector.IssueManager.Event.IssueWasAdded, this._issueWasAdded, this);

        var issues = WebInspector.issueManager.issuesForSourceCode(resource);
        for (var i = 0; i < issues.length; ++i)
            this.addIssue(issues[i]);
    }
};

WebInspector.ResourceContentView.prototype = {
    constructor: WebInspector.ResourceContentView,
    __proto__: WebInspector.ContentView.prototype,

    // Public

    get resource()
    {
        return this._resource;
    },

    contentAvailable: function(content, base64Encoded)
    {
        // Implemented by subclasses.
    },

    addIssue: function(issue)
    {
        // This generically shows only the last issue, subclasses can override for better handling.
        this.element.removeChildren();
        this.element.appendChild(WebInspector.createMessageTextView(issue.text, issue.level === WebInspector.IssueMessage.Level.Error));
    },

    closed: function()
    {
        if (!this.managesOwnIssues)
            WebInspector.issueManager.removeEventListener(null, null, this);
    },

    // Private

    _contentAvailable: function(parameters)
    {
        if (parameters.error) {
            this._contentError(parameters.error);
            return;
        }

        // Content is ready to show, call the public method now.
        console.assert(!this._hasContent());
        console.assert(parameters.sourceCode === this._resource);
        this.contentAvailable(parameters.sourceCode.content, parameters.base64Encoded);
    },

    _contentError: function(error)
    {
        if (this._hasContent())
            return;

        this.element.removeChildren();
        this.element.appendChild(WebInspector.createMessageTextView(error, true));
    },

    _protocolError: function(error)
    {
        this._contentError(WebInspector.UIString("An error occurred trying to load the resource."));
    },

    _hasContent: function()
    {
        return !this.element.querySelector("." + WebInspector.IndeterminateProgressSpinner.StyleClassName);
    },

    _issueWasAdded: function(event)
    {
        console.assert(!this.managesOwnIssues);

        var issue = event.data.issue;

        // FIXME: Check more than just the issue URL (the document could have multiple resources with the same URL).
        if (issue.url !== this.resource.url)
            return;

        this.addIssue(issue);
    },

    _mouseWasClicked: function(event)
    {
        WebInspector.handlePossibleLinkClick(event, this.resource.parentFrame);
    }
};
