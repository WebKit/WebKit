/*
* Copyright (C) 2016 Apple Inc. All rights reserved.
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

WebInspector.HeapSnapshotInstanceDataGridNode = class HeapSnapshotInstanceDataGridNode extends WebInspector.DataGridNode
{
    constructor(node, tree)
    {
        super(node, false);

        console.assert(node instanceof WebInspector.HeapSnapshotNode);

        this._node = node;
        this._tree = tree;

        // FIXME: Make instance grid nodes copyable.
        this.copyable = false;

        this._percent = (this._node.size / this._tree._heapSnapshot.totalSize) * 100;
    }

    // Protected

    get data() { return this._node; }
    get selectable() { return false; }

    createCells()
    {
        super.createCells();

        this.element.classList.add("instance");
    }

    createCellContent(columnIdentifier)
    {
        if (columnIdentifier === "size") {
            let size = this._node.size;
            let percent = this._percent;
            let fragment = document.createDocumentFragment();
            let timeElement = fragment.appendChild(document.createElement("span"));
            timeElement.classList.add("size");
            timeElement.textContent = Number.bytesToString(size);
            let percentElement = fragment.appendChild(document.createElement("span"));
            percentElement.classList.add("percentage");
            percentElement.textContent = Number.percentageString(percent);
            return fragment;
        }

        if (columnIdentifier === "className") {
            let {className, id, internal} = this._node;
            let containerElement = document.createElement("span");
            containerElement.addEventListener("contextmenu", this._contextMenuHandler.bind(this));

            let iconElement = containerElement.appendChild(document.createElement("img"));
            iconElement.classList.add("icon", WebInspector.HeapSnapshotClusterContentView.iconStyleClassNameForClassName(className, internal));

            let idElement = containerElement.appendChild(document.createElement("span"));
            idElement.classList.add("object-id");
            idElement.textContent = "@" + id + " ";

            HeapAgent.getPreview(id, (error, string, functionDetails, objectPreviewPayload) => {
                if (error) {
                    let previewErrorElement = containerElement.appendChild(document.createElement("span"));
                    previewErrorElement.classList.add("preview-error");
                    previewErrorElement.textContent = internal ? WebInspector.UIString("Internal object") : WebInspector.UIString("No preview available");
                    return;
                }

                if (string) {
                    let primitiveRemoteObject = WebInspector.RemoteObject.fromPrimitiveValue(string);
                    containerElement.appendChild(WebInspector.FormattedValue.createElementForRemoteObject(primitiveRemoteObject));
                    return;
                }

                if (functionDetails) {
                    let {location, name, displayName} = functionDetails;
                    let functionNameElement = containerElement.appendChild(document.createElement("span"));
                    functionNameElement.classList.add("function-name");
                    functionNameElement.textContent = name || displayName || WebInspector.UIString("(anonymous function)");
                    let sourceCode = WebInspector.debuggerManager.scriptForIdentifier(location.scriptId);
                    if (sourceCode) {
                        let locationElement = containerElement.appendChild(document.createElement("span"));
                        locationElement.classList.add("location");
                        let sourceCodeLocation = sourceCode.createSourceCodeLocation(location.lineNumber, location.columnNumber);
                        sourceCodeLocation.populateLiveDisplayLocationString(locationElement, "textContent", WebInspector.SourceCodeLocation.ColumnStyle.Hidden, WebInspector.SourceCodeLocation.NameStyle.Short);

                        const dontFloat = true;
                        const useGoToArrowButton = true;
                        let goToArrowButtonLink = WebInspector.createSourceCodeLocationLink(sourceCodeLocation, dontFloat, useGoToArrowButton);
                        containerElement.appendChild(goToArrowButtonLink);
                    }
                    return;
                }

                if (objectPreviewPayload) {
                    let objectPreview = WebInspector.ObjectPreview.fromPayload(objectPreviewPayload);
                    let previewElement = WebInspector.FormattedValue.createObjectPreviewOrFormattedValueForObjectPreview(objectPreview);
                    containerElement.appendChild(previewElement);
                    return;
                }
            });

            return containerElement;
        }

        return super.createCellContent(columnIdentifier);
    }

    sort()
    {
        // No children to sort.
    }

    // Private

    _contextMenuHandler(event)
    {
        let contextMenu = WebInspector.ContextMenu.createFromEvent(event);

        contextMenu.appendSeparator();

        contextMenu.appendItem(WebInspector.UIString("Log Value"), () => {
            let heapObjectIdentifier = this._node.id;
            HeapAgent.getRemoteObject(heapObjectIdentifier, WebInspector.RuntimeManager.ConsoleObjectGroup, function(error, remoteObjectPayload) {
                const synthetic = true;
                let remoteObject = error ? WebInspector.RemoteObject.fromPrimitive(undefined) : WebInspector.RemoteObject.fromPayload(remoteObjectPayload);
                let text = WebInspector.UIString("Heap Snapshot Object (@%d)").format(heapObjectIdentifier);
                WebInspector.consoleLogViewController.appendImmediateExecutionWithResult(text, remoteObject, synthetic);
            });
        });
    }
};
