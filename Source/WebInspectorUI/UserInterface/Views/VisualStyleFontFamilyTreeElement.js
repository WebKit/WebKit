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

WI.VisualStyleFontFamilyTreeElement = class VisualStyleFontFamilyTreeElement extends WI.GeneralTreeElement
{
    constructor(text)
    {
        super([WI.VisualStyleCommaSeparatedKeywordEditor.ListItemClassName, "visual-style-font-family-list-item"], text);

        this._keywordEditor = document.createElement("input");
        this._keywordEditor.classList.add("visual-style-comma-separated-keyword-item-editor");
        this._keywordEditor.placeholder = WI.UIString("(modify the boxes below to add a value)");
        this._keywordEditor.spellcheck = false;
        this._keywordEditor.addEventListener("keydown", this._keywordEditorKeyDown.bind(this));
        this._keywordEditor.addEventListener("keyup", this._keywordEditorKeyUp.bind(this));
        this._keywordEditor.addEventListener("blur", this._keywordEditorBlurred.bind(this));
    }

    // Public

    editorBounds(padding)
    {
        if (this.keywordEditorHidden)
            return;

        let bounds = WI.Rect.rectFromClientRect(this._keywordEditor.getBoundingClientRect());
        return bounds.pad(padding || 0);
    }

    updateMainTitle(text)
    {
        this.mainTitle = this._keywordEditor.value = text;
        this._listItemNode.style.fontFamily = text + ", " + WI.VisualStyleFontFamilyTreeElement.FontFamilyFallback;

        let hasText = text && text.length;
        this._listItemNode.classList.toggle("no-value", !hasText);
        if (!hasText)
            this.subtitle = this._keywordEditor.placeholder;

        this.dispatchEventToListeners(WI.VisualStyleFontFamilyTreeElement.Event.KeywordChanged);
    }

    get currentlyEditing()
    {
        return !this.keywordEditorHidden;
    }

    showKeywordEditor()
    {
        if (!this.keywordEditorHidden)
            return;

        this.subtitle = "";
        this._listItemNode.classList.remove("editor-hidden");
        this._listItemNode.scrollIntoViewIfNeeded();

        this._keywordEditor.value = this._mainTitle;
        this._keywordEditor.select();
    }

    hideKeywordEditor()
    {
        if (this.keywordEditorHidden)
            return;

        this.updateMainTitle(this._keywordEditor.value);
        this._listItemNode.classList.add("editor-hidden");
    }

    get keywordEditorHidden()
    {
        return this._listItemNode.classList.contains("editor-hidden");
    }

    // Protected

    onattach()
    {
        super.onattach();

        this._listItemNode.style.fontFamily = this._mainTitle;
        this._listItemNode.classList.add("editor-hidden");
        this._listItemNode.appendChild(this._keywordEditor);
        this._listItemNode.addEventListener("click", this.showKeywordEditor.bind(this));
    }

    ondeselect()
    {
        this.hideKeywordEditor();
    }

    // Private

    _keywordEditorKeyDown(event)
    {
        if (this.keywordEditorHidden)
            return;

        let keyCode = event.keyCode;
        let enterKeyCode = WI.KeyboardShortcut.Key.Enter.keyCode;
        if (keyCode === enterKeyCode) {
            this._listItemNode.classList.add("editor-hidden");
            this.dispatchEventToListeners(WI.VisualStyleFontFamilyTreeElement.Event.EditorKeyDown, {key: "Enter"});
            return;
        }

        let escapeKeyCode = WI.KeyboardShortcut.Key.Escape.keyCode;
        if (keyCode === escapeKeyCode) {
            this.dispatchEventToListeners(WI.VisualStyleFontFamilyTreeElement.Event.EditorKeyDown, {key: "Escape"});
            return;
        }

        let tabKeyCode = WI.KeyboardShortcut.Key.Tab.keyCode;
        if (keyCode === tabKeyCode) {
            event.preventDefault();
            this._dontFireKeyUp = true;
            this.dispatchEventToListeners(WI.VisualStyleFontFamilyTreeElement.Event.EditorKeyDown, {key: "Tab"});
            return;
        }

        let key = event.keyIdentifier;
        if (key === "Up" || key === "Down") {
            event.preventDefault();
            this._dontFireKeyUp = true;
            this.dispatchEventToListeners(WI.VisualStyleFontFamilyTreeElement.Event.EditorKeyDown, {key});
            return;
        }

        this.dispatchEventToListeners(WI.VisualStyleFontFamilyTreeElement.Event.EditorKeyDown);
    }

    _keywordEditorKeyUp()
    {
        if (this.keywordEditorHidden || this._dontFireKeyUp)
            return;

        this.updateMainTitle(this._keywordEditor.value);
    }

    _keywordEditorBlurred()
    {
        this.hideKeywordEditor();
        this.dispatchEventToListeners(WI.VisualStyleFontFamilyTreeElement.Event.EditorBlurred);
    }
};

WI.VisualStyleFontFamilyTreeElement.FontFamilyFallback = "-webkit-system-font, sans-serif";

WI.VisualStyleFontFamilyTreeElement.Event = {
    KeywordChanged: "visual-style-font-family-tree-element-keyword-changed",
    EditorKeyDown: "visual-style-font-family-tree-element-editor-key-down",
    EditorBlurred: "visual-style-font-family-tree-element-editor-blurred"
};
