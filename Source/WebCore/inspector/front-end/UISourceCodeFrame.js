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
 * @extends {WebInspector.SourceFrame}
 * @param {WebInspector.UISourceCode} uiSourceCode
 */
WebInspector.UISourceCodeFrame = function(uiSourceCode)
{
    this._uiSourceCode = uiSourceCode;
    WebInspector.SourceFrame.call(this, this._uiSourceCode);
    this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ContentChanged, this._onContentChanged, this);
}

WebInspector.UISourceCodeFrame.prototype = {
    /**
     * @return {boolean}
     */
    canEditSource: function()
    {
        return true;
    },

    /**
     * @param {string} text
     */
    commitEditing: function(text)
    {
        if (!this._uiSourceCode.isDirty())
            return;

        this._isCommittingEditing = true;
        this._uiSourceCode.commitWorkingCopy(this._didEditContent.bind(this));
    },

    onTextChanged: function(oldRange, newRange)
    {
        this._uiSourceCode.setWorkingCopy(this._textEditor.text());
    },

    _didEditContent: function(error)
    {
        if (error) {
            WebInspector.log(error, WebInspector.ConsoleMessage.MessageLevel.Error, true);
            return;
        }
        delete this._isCommittingEditing;
    },

    /**
     * @param {WebInspector.Event} event
     */
    _onContentChanged: function(event)
    {
        if (!this._isCommittingEditing)
            this.setContent(this._uiSourceCode.content() || "", false, this._uiSourceCode.contentType().canonicalMimeType());
    },

    populateTextAreaContextMenu: function(contextMenu, lineNumber)
    {
        WebInspector.SourceFrame.prototype.populateTextAreaContextMenu.call(this, contextMenu, lineNumber);
        contextMenu.appendApplicableItems(this._uiSourceCode);
        contextMenu.appendSeparator();
    }
}

WebInspector.UISourceCodeFrame.prototype.__proto__ = WebInspector.SourceFrame.prototype;
