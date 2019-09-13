/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

WI.LocalResourceOverridePopover = class LocalResourceOverridePopover extends WI.Popover
{
    constructor(delegate)
    {
        super(delegate);

        this._urlCodeMirror = null;
        this._mimeTypeCodeMirror = null;
        this._statusCodeCodeMirror = null;
        this._statusTextCodeMirror = null;
        this._headersDataGrid = null;

        this._serializedDataWhenShown = null;

        this.windowResizeHandler = this._presentOverTargetElement.bind(this);
    }

    // Public

    get serializedData()
    {
        if (!this._targetElement)
            return null;

        let url = this._urlCodeMirror.getValue();
        if (!url)
            return null;

        const schemes = ["http:", "https:", "file:"];
        if (!schemes.some((scheme) => url.startsWith(scheme)))
            return null;

        // NOTE: We can allow an empty mimeType / statusCode / statusText to pass
        // network values through, but lets require them for overrides so that
        // the popover doesn't have to have an additional state for "pass through".

        let mimeType = this._mimeTypeCodeMirror.getValue();
        if (!mimeType)
            return null;

        let statusCode = parseInt(this._statusCodeCodeMirror.getValue());
        if (isNaN(statusCode) || statusCode < 0)
            return null;

        let statusText = this._statusTextCodeMirror.getValue();
        if (!statusText)
            return null;

        let headers = {};
        for (let node of this._headersDataGrid.children) {
            let {name, value} = node.data;
            if (!name || !value)
                continue;
            if (name.toLowerCase() === "content-type")
                continue;
            if (name.toLowerCase() === "set-cookie")
                continue;
            headers[name] = value;
        }

        let data = {
            url,
            mimeType,
            statusCode,
            statusText,
            headers,
        };

        // No change.
        let oldSerialized = JSON.stringify(this._serializedDataWhenShown);
        let newSerialized = JSON.stringify(data);
        if (oldSerialized === newSerialized)
            return null;

        return data;
    }

    show(localResource, targetElement, preferredEdges)
    {
        this._targetElement = targetElement;
        this._preferredEdges = preferredEdges;

        let url = localResource ? localResource.url : "";
        let mimeType = localResource ? localResource.mimeType : "";
        let statusCode = localResource ? String(localResource.statusCode) : "";
        let statusText = localResource ? localResource.statusText : "";
        let responseHeaders = localResource ? localResource.responseHeaders : {};

        if (!url)
            url = this._defaultURL();
        if (!mimeType)
            mimeType = "text/javascript";
        if (!statusCode || statusCode === "NaN")
            statusCode = "200";
        if (!statusText)
            statusText = WI.HTTPUtilities.statusTextForStatusCode(statusCode);

        let popoverContentElement = document.createElement("div");
        popoverContentElement.className = "local-resource-override-popover-content";

        let table = popoverContentElement.appendChild(document.createElement("table"));

        let createRow = (label, id, text) => {
            let row = table.appendChild(document.createElement("tr"));
            let headerElement = row.appendChild(document.createElement("th"));
            let dataElement = row.appendChild(document.createElement("td"));

            let labelElement = headerElement.appendChild(document.createElement("label"));
            labelElement.textContent = label;

            let editorElement = dataElement.appendChild(document.createElement("div"));
            editorElement.classList.add("editor", id);

            let codeMirror = this._createEditor(editorElement, text);
            let inputField = codeMirror.getInputField();
            inputField.id = `local-resource-override-popover-${id}-input-field`;
            labelElement.setAttribute("for", inputField.id);

            return {codeMirror, dataElement};
        };

        let urlRow = createRow(WI.UIString("URL"), "url", url);
        this._urlCodeMirror = urlRow.codeMirror;
        this._urlCodeMirror.setOption("mode", "text/x-local-override-url");

        let mimeTypeRow = createRow(WI.UIString("MIME Type"), "mime", mimeType);
        this._mimeTypeCodeMirror = mimeTypeRow.codeMirror;

        let statusCodeRow = createRow(WI.UIString("Status"), "status", statusCode);
        this._statusCodeCodeMirror = statusCodeRow.codeMirror;

        let statusTextEditorElement = statusCodeRow.dataElement.appendChild(document.createElement("div"));
        statusTextEditorElement.className = "editor status-text";
        this._statusTextCodeMirror = this._createEditor(statusTextEditorElement, statusText);

        let editCallback = () => {};
        let deleteCallback = (node) => {
            if (node === contentTypeDataGridNode)
                return;

            let siblingToSelect = node.nextSibling || node.previousSibling;
            dataGrid.removeChild(node);
            if (siblingToSelect)
                siblingToSelect.select();

            dataGrid.updateLayoutIfNeeded();
            this.update();
        };

        let columns = {
            name: {
                title: WI.UIString("Name"),
                width: "30%",
            },
            value: {
                title: WI.UIString("Value"),
            },
        };

        let dataGrid = this._headersDataGrid = new WI.DataGrid(columns, {editCallback, deleteCallback});
        dataGrid.inline = true;
        dataGrid.variableHeightRows = true;
        dataGrid.copyTextDelimiter = ": ";

        function addDataGridNodeForHeader(name, value) {
            let node = new WI.DataGridNode({name, value});
            dataGrid.appendChild(node);
            return node;
        }

        let contentTypeDataGridNode = addDataGridNodeForHeader("Content-Type", mimeType);
        contentTypeDataGridNode.editable = false;

        for (let name in responseHeaders) {
            if (name.toLowerCase() === "content-type")
                continue;
            if (name.toLowerCase() === "set-cookie")
                continue;
            addDataGridNodeForHeader(name, responseHeaders[name]);
        }

        let headersRow = table.appendChild(document.createElement("tr"));
        let headersHeader = headersRow.appendChild(document.createElement("th"));
        let headersData = headersRow.appendChild(document.createElement("td"));
        let headersLabel = headersHeader.appendChild(document.createElement("label"));
        headersLabel.textContent = WI.UIString("Headers");
        headersData.appendChild(dataGrid.element);
        dataGrid.updateLayoutIfNeeded();

        let addHeaderButton = headersData.appendChild(document.createElement("button"));
        addHeaderButton.className = "add-header";
        addHeaderButton.textContent = WI.UIString("Add Header");
        addHeaderButton.addEventListener("click", (event) => {
            let newNode = new WI.DataGridNode({name: "Header", value: "value"});
            dataGrid.appendChild(newNode);
            dataGrid.updateLayoutIfNeeded();
            this.update();
            dataGrid.startEditingNode(newNode);
        });

        let incrementStatusCode = () => {
            let x = parseInt(this._statusCodeCodeMirror.getValue());
            if (isNaN(x) || x >= 999)
                return;

            if (WI.modifierKeys.shiftKey) {
                // 200 => 300 and 211 => 300
                x = (x - (x % 100)) + 100;
            } else
                x += 1;

            if (x > 999)
                x = 999;

            this._statusCodeCodeMirror.setValue(`${x}`);
            this._statusCodeCodeMirror.setCursor(this._statusCodeCodeMirror.lineCount(), 0);
        };

        let decrementStatusCode = () => {
            let x = parseInt(this._statusCodeCodeMirror.getValue());
            if (isNaN(x) || x <= 0)
                return;

            if (WI.modifierKeys.shiftKey) {
                // 311 => 300 and 300 => 200
                let original = x;
                x = (x - (x % 100));
                if (original === x)
                    x -= 100;
            } else
                x -= 1;

            if (x < 0)
                x = 0;

            this._statusCodeCodeMirror.setValue(`${x}`);
            this._statusCodeCodeMirror.setCursor(this._statusCodeCodeMirror.lineCount(), 0);
        };

        this._statusCodeCodeMirror.addKeyMap({
            "Up": incrementStatusCode,
            "Shift-Up": incrementStatusCode,
            "Down": decrementStatusCode,
            "Shift-Down": decrementStatusCode,
        });

        // Update statusText when statusCode changes.
        this._statusCodeCodeMirror.on("change", (cm) => {
            let statusCode = parseInt(cm.getValue());
            let statusText = WI.HTTPUtilities.statusTextForStatusCode(statusCode);
            this._statusTextCodeMirror.setValue(statusText);
        });

        // Update mimeType when URL gets a file extension.
        this._urlCodeMirror.on("change", (cm) => {
            let extension = WI.fileExtensionForURL(cm.getValue());
            if (!extension)
                return;

            let mimeType = WI.mimeTypeForFileExtension(extension);
            if (!mimeType)
                return;

            this._mimeTypeCodeMirror.setValue(mimeType);
            contentTypeDataGridNode.data = {name: "Content-Type", value: mimeType};
        });

        // Update Content-Type header when mimeType changes.
        this._mimeTypeCodeMirror.on("change", (cm) => {
            let mimeType = cm.getValue();
            contentTypeDataGridNode.data = {name: "Content-Type", value: mimeType};
        });

        this._serializedDataWhenShown = this.serializedData;

        this.content = popoverContentElement;
        this._presentOverTargetElement();

        // CodeMirror needs a refresh after the popover displays, to layout, otherwise it doesn't appear.
        setTimeout(() => {
            this._urlCodeMirror.refresh();
            this._mimeTypeCodeMirror.refresh();
            this._statusCodeCodeMirror.refresh();
            this._statusTextCodeMirror.refresh();

            this._urlCodeMirror.focus();
            this._urlCodeMirror.setCursor(this._urlCodeMirror.lineCount(), 0);
        });
    }

    // Private

    _createEditor(element, value)
    {
        let codeMirror = WI.CodeMirrorEditor.create(element, {
            extraKeys: {"Tab": false, "Shift-Tab": false},
            lineWrapping: false,
            mode: "text/plain",
            matchBrackets: true,
            placeholder: "http://example.com/index.html",
            scrollbarStyle: null,
            value,
        });

        codeMirror.addKeyMap({
            "Enter": () => { this.dismiss(); },
            "Shift-Enter": () => { this.dismiss(); },
            "Esc": () => { this.dismiss(); },
        });

        return codeMirror;
    }

    _defaultURL()
    {
        // We avoid just doing "http://example.com/" here because users can
        // accidentally override the main resource, even though the popover
        // typically prevents no-edit cases.
        let mainFrame = WI.networkManager.mainFrame;
        if (mainFrame && mainFrame.securityOrigin.startsWith("http"))
            return mainFrame.securityOrigin + "/path";

        return "https://";
    }

    _presentOverTargetElement()
    {
        if (!this._targetElement)
            return;

        let targetFrame = WI.Rect.rectFromClientRect(this._targetElement.getBoundingClientRect());
        this.present(targetFrame, this._preferredEdges);
    }
};
