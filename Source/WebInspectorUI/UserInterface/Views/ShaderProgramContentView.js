/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.ShaderProgramContentView = class ShaderProgramContentView extends WI.ContentView
{
    constructor(shaderProgram)
    {
        console.assert(shaderProgram instanceof WI.ShaderProgram);

        super(shaderProgram);

        let contentDidChangeDebouncer = new Debouncer((event) => {
            this._contentDidChange(event);
        });

        this.element.classList.add("shader-program");

        let createEditor = (shaderType) => {
            let textEditor = new WI.TextEditor;
            textEditor.readOnly = false;
            textEditor.addEventListener(WI.TextEditor.Event.Focused, this._editorFocused, this);
            textEditor.addEventListener(WI.TextEditor.Event.NumberOfSearchResultsDidChange, this._numberOfSearchResultsDidChange, this);
            textEditor.addEventListener(WI.TextEditor.Event.ContentDidChange, (event) => {
                contentDidChangeDebouncer.delayForTime(250, event);
            }, this);
            textEditor.element.classList.add("shader");

            let shaderTypeContainer = textEditor.element.insertAdjacentElement("afterbegin", document.createElement("div"));
            shaderTypeContainer.classList.add("type-title");

            switch (shaderType) {
            case WI.ShaderProgram.ShaderType.Vertex:
                shaderTypeContainer.textContent = WI.UIString("Vertex Shader");
                textEditor.mimeType = "x-shader/x-vertex";
                textEditor.element.classList.add("vertex");
                break;

            case WI.ShaderProgram.ShaderType.Fragment:
                shaderTypeContainer.textContent = WI.UIString("Fragment Shader");
                textEditor.mimeType = "x-shader/x-fragment";
                textEditor.element.classList.add("fragment");
                break;
            }

            this.addSubview(textEditor);
            return textEditor;
        };

        this._vertexEditor = createEditor(WI.ShaderProgram.ShaderType.Vertex);
        this._fragmentEditor = createEditor(WI.ShaderProgram.ShaderType.Fragment);
        this._lastActiveEditor = this._vertexEditor;
    }

    // Protected

    shown()
    {
        super.shown();

        this._vertexEditor.shown();
        this._fragmentEditor.shown();

        this.representedObject.requestVertexShaderSource((content) => {
            this._vertexEditor.string = content || "";
        });

        this.representedObject.requestFragmentShaderSource((content) => {
            this._fragmentEditor.string = content || "";
        });
    }

    hidden()
    {
        this._vertexEditor.hidden();
        this._fragmentEditor.hidden();

        super.hidden();
    }

    closed()
    {
        this._vertexEditor.close();
        this._fragmentEditor.close();

        super.closed();
    }

    get supportsSave()
    {
        return true;
    }

    get saveData()
    {
        let filename = WI.UIString("Shader");
        if (this._lastActiveEditor === this._vertexEditor)
            filename = WI.UIString("Vertex");
        else if (this._lastActiveEditor === this._fragmentEditor)
            filename = WI.UIString("Fragment");

        return {
            url: `web-inspector:///${filename}.glsl`,
            content: this._lastActiveEditor.string,
            forceSaveAs: true,
        };
    }

    get supportsSearch()
    {
        return true;
    }

    get numberOfSearchResults()
    {
        return this._lastActiveEditor.numberOfSearchResults;
    }

    get hasPerformedSearch()
    {
        return this._lastActiveEditor.currentSearchQuery !== null;
    }

    set automaticallyRevealFirstSearchResult(reveal)
    {
        this._lastActiveEditor.automaticallyRevealFirstSearchResult = reveal;
    }

    performSearch(query)
    {
        this._lastActiveEditor.performSearch(query);
    }

    searchCleared()
    {
        this._lastActiveEditor.searchCleared();
    }

    searchQueryWithSelection()
    {
        return this._lastActiveEditor.searchQueryWithSelection();
    }

    revealPreviousSearchResult(changeFocus)
    {
        this._lastActiveEditor.revealPreviousSearchResult(changeFocus);
    }

    revealNextSearchResult(changeFocus)
    {
        this._lastActiveEditor.revealNextSearchResult(changeFocus);
    }

    revealPosition(position, textRangeToSelect, forceUnformatted)
    {
        this._lastActiveEditor.revealPosition(position, textRangeToSelect, forceUnformatted);
    }

    // Private

    _editorFocused(event)
    {
        if (this._lastActiveEditor === event.target)
            return;

        let currentSearchQuery = null;

        if (this._lastActiveEditor) {
            currentSearchQuery = this._lastActiveEditor.currentSearchQuery;

            this._lastActiveEditor.searchCleared();
        }

        this._lastActiveEditor = event.target;

        if (currentSearchQuery)
            this._lastActiveEditor.performSearch(currentSearchQuery);
    }

    _numberOfSearchResultsDidChange(event)
    {
        this.dispatchEventToListeners(WI.ContentView.Event.NumberOfSearchResultsDidChange);
    }

    _contentDidChange(event)
    {
        if (event.target === this._vertexEditor)
            this.representedObject.updateVertexShader(this._vertexEditor.string);
        else if (event.target === this._fragmentEditor)
            this.representedObject.updateFragmentShader(this._fragmentEditor.string);
    }
};
