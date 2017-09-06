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

WI.RecordingActionTreeElement = class RecordingActionTreeElement extends WI.GeneralTreeElement
{
    constructor(representedObject, index, recordingType)
    {
        console.assert(representedObject instanceof WI.RecordingAction);

        let {classNames, copyText, titleFragment} = WI.RecordingActionTreeElement._generateDOM(representedObject, recordingType);

        const subtitle = null;
        super(classNames, titleFragment, subtitle, representedObject);

        this._index = index;
        this._copyText = copyText;
    }

    // Static

    static _generateDOM(recordingAction, recordingType)
    {
        let classNames = ["action"];
        let copyText = recordingAction.name;

        let isInitialState = recordingAction instanceof WI.RecordingInitialStateAction;
        if (recordingAction.isFunction)
            classNames.push("function");
        else if (!isInitialState) {
            classNames.push("attribute");
            if (recordingAction.isGetter)
                classNames.push("getter");
            else
                classNames.push(typeof recordingAction.parameters[0]);
        }

        if (recordingAction.isVisual)
            classNames.push("visual");

        if (!recordingAction.valid)
            classNames.push("invalid");

        let titleFragment = document.createDocumentFragment();

        let nameContainer = titleFragment.appendChild(document.createElement("span"));
        nameContainer.classList.add("name");
        nameContainer.textContent = recordingAction.name;

        if (!recordingAction.isGetter && !isInitialState) {
            if (recordingAction.isFunction) {
                titleFragment.append("(");
                copyText += "(";
            } else {
                titleFragment.append(" = ");
                copyText = " = ";
            }

            for (let i = 0; i < recordingAction.parameters.length; ++i) {
                let parameter = recordingAction.parameters[i];
                let swizzleType = recordingAction.swizzleTypes[i];

                if (i) {
                    titleFragment.append(", ");
                    copyText += ", ";
                }

                let parameterElement = titleFragment.appendChild(document.createElement("span"));
                parameterElement.classList.add("parameter");

                switch (swizzleType) {
                case WI.Recording.Swizzle.Number:
                    parameterElement.textContent = parameter.maxDecimals(2);
                    break;

                case WI.Recording.Swizzle.Boolean:
                    parameterElement.textContent = parameter;
                    break;

                case WI.Recording.Swizzle.String:
                    parameterElement.textContent = JSON.stringify(parameter);
                    break;

                case WI.Recording.Swizzle.Array:
                    parameterElement.classList.add("swizzled");
                    parameterElement.textContent = JSON.stringify(parameter);
                    break;

                case WI.Recording.Swizzle.TypedArray:
                case WI.Recording.Swizzle.Image:
                case WI.Recording.Swizzle.ImageData:
                case WI.Recording.Swizzle.DOMMatrix:
                case WI.Recording.Swizzle.Path2D:
                case WI.Recording.Swizzle.CanvasGradient:
                case WI.Recording.Swizzle.CanvasPattern:
                case WI.Recording.Swizzle.WebGLBuffer:
                case WI.Recording.Swizzle.WebGLFramebuffer:
                case WI.Recording.Swizzle.WebGLRenderbuffer:
                case WI.Recording.Swizzle.WebGLTexture:
                case WI.Recording.Swizzle.WebGLShader:
                case WI.Recording.Swizzle.WebGLProgram:
                case WI.Recording.Swizzle.WebGLUniformLocation:
                    parameterElement.classList.add("swizzled");
                    parameterElement.textContent = WI.Recording.displayNameForSwizzleType(swizzleType);
                    break;
                }

                if (!parameterElement.textContent) {
                    parameterElement.classList.add("invalid");
                    parameterElement.textContent = swizzleType === WI.Recording.Swizzle.None ? parameter : WI.Recording.displayNameForSwizzleType(swizzleType);
                }

                copyText += parameterElement.textContent;

                if (!recordingAction.isFunction)
                    break;
            }

            if (recordingAction.isFunction) {
                titleFragment.append(")");
                copyText += ")";
            }
        } else if (isInitialState)
            classNames.push("initial-state");

        return {
            classNames,
            copyText,
            titleFragment,
        };
    }

    // Public

    get index() { return this._index; }

    get filterableData()
    {
        let text = [];

        function getText(stringOrElement) {
            if (typeof stringOrElement === "string")
                text.push(stringOrElement);
            else if (stringOrElement instanceof Node)
                text.push(stringOrElement.textContent);
        }

        getText(this._mainTitleElement || this._mainTitle);
        getText(this._subtitleElement || this._subtitle);

        return {text};
    }

    // Protected

    onattach()
    {
        super.onattach();

        this.element.dataset.index = this._index.toLocaleString();
    }

    populateContextMenu(contextMenu, event)
    {
        contextMenu.appendItem(WI.UIString("Copy Action"), () => {
            InspectorFrontendHost.copyText("context." + this._copyText + ";");
        });

        contextMenu.appendSeparator();

        let callFrame = this.representedObject.trace[0];
        if (callFrame) {
            contextMenu.appendItem(WI.UIString("Reveal in Resources Tab"), () => {
                WI.showSourceCodeLocation(callFrame.sourceCodeLocation, {
                    ignoreNetworkTab: true,
                    ignoreSearchTab: true,
                });
            });

            contextMenu.appendSeparator();
        }

        super.populateContextMenu(contextMenu, event);
    }
};
