/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WI.ResourceContentView = class ResourceContentView extends WI.ContentView
{
    constructor(resource, styleClassName)
    {
        console.assert(resource instanceof WI.Resource || resource instanceof WI.CSSStyleSheet, resource);
        console.assert(typeof styleClassName === "string");

        super(resource);

        this._resource = resource;

        this.element.classList.add(styleClassName, "resource");

        this._spinnerTimeout = setTimeout(() => {
            // Append a spinner while waiting for contentAvailable. Subclasses are responsible for
            // removing the spinner before showing the resource content by calling removeLoadingIndicator.
            let spinner = new WI.IndeterminateProgressSpinner;
            this.element.appendChild(spinner.element);

            this._spinnerTimeout = undefined;
        }, 100);

        this.element.addEventListener("click", this._mouseWasClicked.bind(this), false);

        // Request content last so the spinner will always be removed in case the content is immediately available.
        resource.requestContent().then(this._contentAvailable.bind(this)).catch(this.showGenericErrorMessage.bind(this));

        if (!this.managesOwnIssues) {
            WI.consoleManager.addEventListener(WI.ConsoleManager.Event.IssueAdded, this._issueWasAdded, this);

            var issues = WI.consoleManager.issuesForSourceCode(resource);
            for (var i = 0; i < issues.length; ++i)
                this.addIssue(issues[i]);
        }
    }

    // Public

    get resource()
    {
        return this._resource;
    }

    get supportsSave()
    {
        return this._resource.finished;
    }

    get saveData()
    {
        return {url: this._resource.url, content: this._resource.content};
    }

    contentAvailable(content, base64Encoded)
    {
        throw WI.NotImplementedError.subclassMustOverride();
    }

    showGenericNoContentMessage()
    {
        this.showMessage(WI.UIString("Resource has no content"));

        this.dispatchEventToListeners(WI.ResourceContentView.Event.ContentError);
    }

    showGenericErrorMessage()
    {
        this._contentError(WI.UIString("An error occurred trying to load the resource."));
    }

    showMessage(message)
    {
        this.element.removeChildren();
        this.element.appendChild(WI.createMessageTextView(message));
    }

    addIssue(issue)
    {
        // This generically shows only the last issue, subclasses can override for better handling.
        this.element.removeChildren();
        this.element.appendChild(WI.createMessageTextView(issue.text, issue.level === WI.IssueMessage.Level.Error));
    }

    closed()
    {
        super.closed();

        if (!this.managesOwnIssues)
            WI.consoleManager.removeEventListener(null, null, this);
    }

    // Protected

    removeLoadingIndicator()
    {
        if (this._spinnerTimeout) {
            clearTimeout(this._spinnerTimeout);
            this._spinnerTimeout = undefined;
        }

        this.element.removeChildren();
    }

    // Private

    _contentAvailable(parameters)
    {
        if (parameters.error) {
            this._contentError(parameters.error);
            return;
        }

        if (parameters.message) {
            this.showMessage(parameters.message);
            return;
        }

        // Content is ready to show, call the public method now.
        console.assert(!this._hasContent());
        console.assert(parameters.sourceCode === this._resource);
        this.contentAvailable(parameters.sourceCode.content, parameters.base64Encoded);
    }

    _contentError(error)
    {
        if (this._hasContent())
            return;

        this.removeLoadingIndicator();

        this.element.appendChild(WI.createMessageTextView(error, true));

        this.dispatchEventToListeners(WI.ResourceContentView.Event.ContentError);
    }

    _hasContent()
    {
        return this.element.hasChildNodes() && !this.element.querySelector(".indeterminate-progress-spinner");
    }

    _issueWasAdded(event)
    {
        console.assert(!this.managesOwnIssues);

        var issue = event.data.issue;
        if (!WI.ConsoleManager.issueMatchSourceCode(issue, this.resource))
            return;

        this.addIssue(issue);
    }

    _mouseWasClicked(event)
    {
        WI.handlePossibleLinkClick(event, this.resource.parentFrame);
    }
};

WI.ResourceContentView.Event = {
    ContentError: "resource-content-view-content-error",
};
