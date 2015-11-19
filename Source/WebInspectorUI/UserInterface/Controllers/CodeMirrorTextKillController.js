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

WebInspector.CodeMirrorTextKillController = class CodeMirrorTextKillController extends WebInspector.Object
{
    constructor(codeMirror)
    {
        super();

        console.assert(codeMirror);

        this._codeMirror = codeMirror;
        this._expectingChangeEventForKill = false;
        this._nextKillStartsNewSequence = true;

        this._handleTextChangeListener = this._handleTextChange.bind(this);
        this._handleEditorBlurListener = this._handleEditorBlur.bind(this);
        this._handleSelectionOrCaretChangeListener = this._handleSelectionOrCaretChange.bind(this);

        this._codeMirror.addKeyMap({
            "Ctrl-K": this._handleKillLine.bind(this),
        });
    }

    _handleKillLine(codeMirror)
    {
        // Read-only mode is dynamic in some editors, so check every time
        // and ignore the shortcut if in read-only mode.
        if (this._codeMirror.getOption("readOnly"))
            return;

        // Don't add the listener if it's still registered because
        // a previous empty kill didn't generate change events.
        if (!this._expectingChangeEventForKill)
            codeMirror.on("changes", this._handleTextChangeListener);

        this._expectingChangeEventForKill = true;
        codeMirror.execCommand("killLine");
    }

    _handleTextChange(codeMirror, changes)
    {
        this._codeMirror.off("changes", this._handleTextChangeListener);

        // Sometimes a second change event fires after removing the listener
        // if you perform an "empty kill" and type after moving the caret.
        if (!this._expectingChangeEventForKill)
            return;

        this._expectingChangeEventForKill = false;

        // It doesn't make sense to get more than one change per kill.
        console.assert(changes.length === 1);
        let change = changes[0];

        // If an "empty kill" is followed by up/down or typing,
        // the empty kill won't fire a change event, then we'll get an
        // unrelated change event that shouldn't be treated as a kill.
        if (change.origin !== "+delete")
            return;

        // Killing a newline by itself is reported as deletion of two
        // empty strings, so check the change's ranges to detect this.
        let killedText;
        if (change.to.line === change.from.line + 1)
            killedText = "\n";
        else {
            console.assert(change.removed.length === 1);
            killedText = change.removed[0];
        }

        const shouldPrependToKillRing = false;
        InspectorFrontendHost.killText(killedText, shouldPrependToKillRing, this._nextKillStartsNewSequence);

        // If the editor loses focus or the caret / selection changes
        // (not as a result of the kill), then the next kill should
        // start a new kill ring sequence.
        this._nextKillStartsNewSequence = false;
        this._codeMirror.on("blur", this._handleEditorBlurListener);
        this._codeMirror.on("cursorActivity", this._handleSelectionOrCaretChangeListener);
    }

    _handleEditorBlur(codeMirror)
    {
        this._nextKillStartsNewSequence = true;
        this._codeMirror.off("blur", this._handleEditorBlurListener);
        this._codeMirror.off("cursorActivity", this._handleSelectionOrCaretChangeListener);
    }

    _handleSelectionOrCaretChange(codeMirror)
    {
        if (this._expectingChangeEventForKill)
            return;

        this._nextKillStartsNewSequence = true;
        this._codeMirror.off("blur", this._handleEditorBlurListener);
        this._codeMirror.off("cursorActivity", this._handleSelectionOrCaretChangeListener);
    }
};
