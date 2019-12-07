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

        let isWebGPU = this.representedObject.canvas.contextType === WI.Canvas.ContextType.WebGPU;
        let sharesVertexFragmentShader = isWebGPU && this.representedObject.sharesVertexFragmentShader;

        this._refreshButtonNavigationItem = new WI.ButtonNavigationItem("refresh", WI.UIString("Refresh"), "Images/ReloadFull.svg", 13, 13);
        this._refreshButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        this._refreshButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._refreshContent, this);

        let contentDidChangeDebouncer = new Debouncer((event) => {
            this._contentDidChange(event);
        });

        this.element.classList.add("shader-program", this.representedObject.programType);

        let createEditor = (shaderType) => {
            let container = this.element.appendChild(document.createElement("div"));

            let header = container.appendChild(document.createElement("header"));

            let shaderTypeContainer = header.appendChild(document.createElement("div"));
            shaderTypeContainer.classList.add("shader-type");

            let textEditor = new WI.TextEditor;
            textEditor.readOnly = false;
            textEditor.addEventListener(WI.TextEditor.Event.Focused, this._editorFocused, this);
            textEditor.addEventListener(WI.TextEditor.Event.NumberOfSearchResultsDidChange, this._numberOfSearchResultsDidChange, this);
            textEditor.addEventListener(WI.TextEditor.Event.ContentDidChange, (event) => {
                contentDidChangeDebouncer.delayForTime(250, event);
            }, this);

            switch (shaderType) {
            case WI.ShaderProgram.ShaderType.Compute:
                shaderTypeContainer.textContent = WI.UIString("Compute Shader");
                textEditor.mimeType = isWebGPU ? "x-pipeline/x-compute" : "x-shader/x-compute";
                break;

            case WI.ShaderProgram.ShaderType.Fragment:
                shaderTypeContainer.textContent = WI.UIString("Fragment Shader");
                textEditor.mimeType = isWebGPU ? "x-pipeline/x-render" : "x-shader/x-fragment";
                break;

            case WI.ShaderProgram.ShaderType.Vertex:
                if (sharesVertexFragmentShader)
                    shaderTypeContainer.textContent = WI.UIString("Vertex/Fragment Shader");
                else
                    shaderTypeContainer.textContent = WI.UIString("Vertex Shader");
                textEditor.mimeType = isWebGPU ? "x-pipeline/x-render" : "x-shader/x-vertex";
                break;
            }

            this.addSubview(textEditor);
            container.appendChild(textEditor.element);
            container.classList.add("shader", shaderType);
            container.classList.toggle("shares-vertex-fragment-shader", sharesVertexFragmentShader);

            return {container, textEditor};
        };

        switch (this.representedObject.programType) {
        case WI.ShaderProgram.ProgramType.Compute: {
            let computeEditor = createEditor(WI.ShaderProgram.ShaderType.Compute);
            this._computeContainer = computeEditor.container;
            this._computeEditor = computeEditor.textEditor;

            this._lastActiveEditor = this._computeEditor;
            break;
        }

        case WI.ShaderProgram.ProgramType.Render: {
            let vertexEditor = createEditor(WI.ShaderProgram.ShaderType.Vertex);
            this._vertexContainer = vertexEditor.container;
            this._vertexEditor = vertexEditor.textEditor;

            if (!sharesVertexFragmentShader) {
                let fragmentEditor = createEditor(WI.ShaderProgram.ShaderType.Fragment);
                this._fragmentContainer = fragmentEditor.container;
                this._fragmentEditor = fragmentEditor.textEditor;
            }

            this._lastActiveEditor = this._vertexEditor;
            break;
        }
        }
    }

    // Public

    get navigationItems()
    {
        return [this._refreshButtonNavigationItem];
    }

    // Protected

    shown()
    {
        super.shown();

        switch (this.representedObject.programType) {
        case WI.ShaderProgram.ProgramType.Compute:
            this._computeEditor.shown();
            break;

        case WI.ShaderProgram.ProgramType.Render:
            this._vertexEditor.shown();
            if (!this.representedObject.sharesVertexFragmentShader)
                this._fragmentEditor.shown();
            break;
        }

        this._refreshContent();
    }

    hidden()
    {
        switch (this.representedObject.programType) {
        case WI.ShaderProgram.ProgramType.Compute:
            this._computeEditor.hidden();
            break;

        case WI.ShaderProgram.ProgramType.Render:
            this._vertexEditor.hidden();
            if (!this.representedObject.sharesVertexFragmentShader)
                this._fragmentEditor.hidden();
            break;
        }

        super.hidden();
    }

    get supportsSave()
    {
        return true;
    }

    get saveData()
    {
        let filename = "";
        switch (this._lastActiveEditor) {
        case this._computeEditor:
            filename = WI.UIString("Compute");
            break;
        case this._fragmentEditor:
            filename = WI.UIString("Fragment");
            break;
        case this._vertexEditor:
            filename = WI.UIString("Vertex");
            break;
        }
        console.assert(filename);

        let extension = "";
        switch (this.representedObject.canvas.contextType) {
        case WI.Canvas.ContextType.WebGL:
        case WI.Canvas.ContextType.WebGL2:
            extension = WI.unlocalizedString(".glsl");
            break;
        case WI.Canvas.ContextType.WebGPU:
            extension = WI.unlocalizedString(".wsl");
            break;
        }
        console.assert(extension);

        return {
            content: this._lastActiveEditor.string,
            suggestedName: filename + extension,
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

    _refreshContent()
    {
        let createCallback = (container, textEditor) => {
            return (source) => {
                if (source === null) {
                    container.remove();
                    return;
                }

                if (!container.parentNode) {
                    switch (container) {
                    case this._computeContainer:
                    case this._vertexContainer:
                        this.element.insertAdjacentElement("afterbegin", container);
                        break;

                    case this._fragmentContainer:
                        this.element.insertAdjacentElement("beforeend", container);
                        break;
                    }
                }

                textEditor.string = source || "";
            };
        };

        switch (this.representedObject.programType) {
        case WI.ShaderProgram.ProgramType.Compute:
            this.representedObject.requestShaderSource(WI.ShaderProgram.ShaderType.Compute, createCallback(this._computeContainer, this._computeEditor));
            return;

        case WI.ShaderProgram.ProgramType.Render:
            this.representedObject.requestShaderSource(WI.ShaderProgram.ShaderType.Vertex, createCallback(this._vertexContainer, this._vertexEditor));
            if (!this.representedObject.sharesVertexFragmentShader)
                this.representedObject.requestShaderSource(WI.ShaderProgram.ShaderType.Fragment, createCallback(this._fragmentContainer, this._fragmentEditor));
            return;
        }

        console.assert();
    }

    _updateShader(shaderType)
    {
        switch (shaderType) {
        case WI.ShaderProgram.ShaderType.Compute:
            this.representedObject.updateShader(shaderType, this._computeEditor.string);
            return;

        case WI.ShaderProgram.ShaderType.Fragment:
            this.representedObject.updateShader(shaderType, this._fragmentEditor.string);
            return;

        case WI.ShaderProgram.ShaderType.Vertex:
            this.representedObject.updateShader(shaderType, this._vertexEditor.string);
            return;
        }

        console.assert();
    }

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
        switch (event.target) {
        case this._computeEditor:
            this._updateShader(WI.ShaderProgram.ShaderType.Compute);
            return;

        case this._fragmentEditor:
            this._updateShader(WI.ShaderProgram.ShaderType.Fragment);
            return;

        case this._vertexEditor:
            this._updateShader(WI.ShaderProgram.ShaderType.Vertex);
            return;
        }

        console.assert();
    }
};
