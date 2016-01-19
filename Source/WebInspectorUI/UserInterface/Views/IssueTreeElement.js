/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.IssueTreeElement = class IssueTreeElement extends WebInspector.DebuggerTreeElement
{
    constructor(issueMessage)
    {
        var levelStyleClassName;
        switch (issueMessage.level) {
        case "error":
            levelStyleClassName = WebInspector.IssueTreeElement.ErrorStyleClassName;
            break;
        case "warning":
            levelStyleClassName = WebInspector.IssueTreeElement.WarningStyleClassName;
            break;
        }

        super([WebInspector.IssueTreeElement.StyleClassName, levelStyleClassName], null, null, issueMessage, false);

        this._issueMessage = issueMessage;
        this._updateTitles();

        this._issueMessage.addEventListener(WebInspector.IssueMessage.Event.DisplayLocationDidChange, this._updateTitles, this);
    }

    // Public

    get issueMessage()
    {
        return this._issueMessage;
    }

    // Private

    _updateTitles()
    {
        var displayLineNumber = this._issueMessage.displayLineNumber;
        var displayColumnNumber = this._issueMessage.displayColumnNumber;
        var title;
        if (displayColumnNumber > 0)
            title = WebInspector.UIString("Line %d:%d").format(displayLineNumber + 1, displayColumnNumber + 1); // The user visible line and column numbers are 1-based.
        else
            title = WebInspector.UIString("Line %d").format(displayLineNumber + 1); // The user visible line number is 1-based.

        this.mainTitle = title + " " + this._issueMessage.text;
    }
};

WebInspector.IssueTreeElement.StyleClassName = "issue";
WebInspector.IssueTreeElement.ErrorStyleClassName = "error";
WebInspector.IssueTreeElement.WarningStyleClassName = "warning";
