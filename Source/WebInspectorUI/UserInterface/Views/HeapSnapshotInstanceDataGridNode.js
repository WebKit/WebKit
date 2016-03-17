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

        console.assert(node instanceof WebInspector.HeapSnapshotNodeProxy);

        this._node = node;
        this._tree = tree;

        // FIXME: Make instance grid nodes copyable.
        this.copyable = false;

        this._percent = (this._node.size / this._tree._heapSnapshot.totalSize) * 100;
    }

    // Static

    static logHeapSnapshotNode(node)
    {
        let heapObjectIdentifier = node.id;
        let synthetic = true;
        let text = WebInspector.UIString("Heap Snapshot Object (@%d)").format(heapObjectIdentifier);

        node.shortestGCRootPath((gcRootPath) => {
            if (gcRootPath.length) {
                gcRootPath = gcRootPath.slice().reverse();
                let windowIndex = gcRootPath.findIndex((x) => {
                    return x instanceof WebInspector.HeapSnapshotNodeProxy && x.className === "Window";
                });

                let heapSnapshotRootPath = WebInspector.HeapSnapshotRootPath.emptyPath();
                for (let i = windowIndex === -1 ? 0 : windowIndex; i < gcRootPath.length; ++i) {
                    let component = gcRootPath[i];
                    if (component instanceof WebInspector.HeapSnapshotNodeProxy) {
                        if (component.className === "Window")
                            heapSnapshotRootPath = heapSnapshotRootPath.appendGlobalScopeName(component, "window");
                    } else if (component instanceof WebInspector.HeapSnapshotEdgeProxy)
                        heapSnapshotRootPath = heapSnapshotRootPath.appendEdge(component);
                }

                if (!heapSnapshotRootPath.isFullPathImpossible()) {
                    synthetic = false;
                    text = heapSnapshotRootPath.fullPath;
                }
            }

            if (node.className === "string") {
                HeapAgent.getPreview(heapObjectIdentifier, function(error, string, functionDetails, objectPreviewPayload) {
                    let remoteObject = error ? WebInspector.RemoteObject.fromPrimitiveValue(undefined) : WebInspector.RemoteObject.fromPrimitiveValue(string);
                    WebInspector.consoleLogViewController.appendImmediateExecutionWithResult(text, remoteObject, synthetic);                
                });
            } else {
                HeapAgent.getRemoteObject(heapObjectIdentifier, WebInspector.RuntimeManager.ConsoleObjectGroup, function(error, remoteObjectPayload) {
                    let remoteObject = error ? WebInspector.RemoteObject.fromPrimitiveValue(undefined) : WebInspector.RemoteObject.fromPayload(remoteObjectPayload);
                    WebInspector.consoleLogViewController.appendImmediateExecutionWithResult(text, remoteObject, synthetic);
                });
            }
        });
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
            idElement.textContent = "@" + id;
            idElement.addEventListener("click", WebInspector.HeapSnapshotInstanceDataGridNode.logHeapSnapshotNode.bind(null, this._node));
            idElement.addEventListener("mouseover", this._mouseoverHandler.bind(this));

            let spacerElement = containerElement.appendChild(document.createElement("span"));
            spacerElement.textContent = " ";

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
        contextMenu.appendItem(WebInspector.UIString("Log Value"), WebInspector.HeapSnapshotInstanceDataGridNode.logHeapSnapshotNode.bind(null, this._node));
    }

    _mouseoverHandler(event)
    {
        let targetFrame = WebInspector.Rect.rectFromClientRect(event.target.getBoundingClientRect());
        if (!targetFrame.size.width && !targetFrame.size.height)
            return;

        if (this._tree.popoverNode === this._node)
            return;

        this._tree.popoverNode = this._node;

        let popoverContentElement = document.createElement("div");
        popoverContentElement.classList.add("heap-snapshot", "heap-snapshot-instance-popover-content");

        function appendPath(path) {
            let tableElement = popoverContentElement.appendChild(document.createElement("table"));
            tableElement.classList.add("table");

            path = path.slice().reverse();
            let windowIndex = path.findIndex((x) => {
                return x instanceof WebInspector.HeapSnapshotNodeProxy && x.className === "Window";
            });

            let edge = null;
            for (let i = windowIndex === -1 ? 0 : windowIndex; i < path.length; ++i) {
                let component = path[i];
                if (component instanceof WebInspector.HeapSnapshotEdgeProxy) {
                    edge = component;
                    continue;
                }
                appendPathRow(tableElement, edge, component);
                edge = null;
            }
        }

        function appendPathRow(tableElement, edge, node) {
            let tableRow = tableElement.appendChild(document.createElement("tr"));

            // Edge name.
            let pathDataElement = tableRow.appendChild(document.createElement("td"));
            pathDataElement.classList.add("edge-name");

            if (node.className === "Window")
                pathDataElement.textContent = "window";
            else if (edge) {
                let edgeString = stringifyEdge(edge);
                pathDataElement.textContent = typeof edgeString === "string" ? edgeString : emDash;
            } else
                pathDataElement.textContent = emDash;

            if (pathDataElement.textContent.length > 10)
                pathDataElement.title = pathDataElement.textContent;

            // Object.
            let objectDataElement = tableRow.appendChild(document.createElement("td"));
            objectDataElement.classList.add("object-data");

            let containerElement = objectDataElement.appendChild(document.createElement("div"));
            containerElement.classList.add("node");

            let iconElement = containerElement.appendChild(document.createElement("img"));
            iconElement.classList.add("icon", WebInspector.HeapSnapshotClusterContentView.iconStyleClassNameForClassName(node.className, node.internal));

            let classNameElement = containerElement.appendChild(document.createElement("span"));
            classNameElement.textContent = sanitizeClassName(node.className);

            let idElement = containerElement.appendChild(document.createElement("span"));
            idElement.classList.add("object-id");
            idElement.textContent = " @" + node.id;
            idElement.addEventListener("click", WebInspector.HeapSnapshotInstanceDataGridNode.logHeapSnapshotNode.bind(null, node));
        }

        function sanitizeClassName(className) {
            if (className.endsWith("LexicalEnvironment"))
                return WebInspector.UIString("Scope");
            return className;
        }

        function stringifyEdge(edge) {
            switch(edge.type) {
            case WebInspector.HeapSnapshotEdgeProxy.EdgeType.Property:
            case WebInspector.HeapSnapshotEdgeProxy.EdgeType.Variable:
                if (/^(?![0-9])\w+$/.test(edge.data))
                    return edge.data;
                return "[" + doubleQuotedString(edge.data) + "]";
            case WebInspector.HeapSnapshotEdgeProxy.EdgeType.Index:
                return "[" + edge.data + "]";
            case WebInspector.HeapSnapshotEdgeProxy.EdgeType.Internal:
            default:
                return null;
            }
        }

        this._node.shortestGCRootPath((path) => {
            if (path.length)
                appendPath(path);
            else if (this._node.gcRoot) {
                let textElement = popoverContentElement.appendChild(document.createElement("div"));
                textElement.textContent = WebInspector.UIString("This object is a root");
            } else {
                let emptyElement = popoverContentElement.appendChild(document.createElement("div"));
                emptyElement.textContent = WebInspector.UIString("This object is referenced by internal objects");
            }
            
            this._tree.popover.presentNewContentWithFrame(popoverContentElement, targetFrame.pad(2), [WebInspector.RectEdge.MAX_Y, WebInspector.RectEdge.MIN_Y, WebInspector.RectEdge.MAX_X]);
        });
    }
};
