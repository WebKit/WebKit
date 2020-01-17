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
        this._isCaseSensitiveCheckbox = null;
        this._isRegexCheckbox = null;
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

        let isRegex = this._isRegexCheckbox && this._isRegexCheckbox.checked;
        if (!isRegex) {
            const schemes = ["http:", "https:", "file:"];
            if (!schemes.some((scheme) => url.toLowerCase().startsWith(scheme)))
                return null;
        }

        // NOTE: We can allow an empty mimeType / statusCode / statusText to pass
        // network values through, but lets require them for overrides so that
        // the popover doesn't have to have an additional state for "pass through".

        let mimeType = this._mimeTypeCodeMirror.getValue() || this._mimeTypeCodeMirror.getOption("placeholder");
        if (!mimeType)
            return null;

        let statusCode = parseInt(this._statusCodeCodeMirror.getValue());
        if (isNaN(statusCode))
            statusCode = parseInt(this._statusCodeCodeMirror.getOption("placeholder"));
        if (isNaN(statusCode) || statusCode < 0)
            return null;

        let statusText = this._statusTextCodeMirror.getValue() || this._statusTextCodeMirror.getOption("placeholder");
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

        if (this._isCaseSensitiveCheckbox)
            data.isCaseSensitive = this._isCaseSensitiveCheckbox.checked;

        if (this._isRegexCheckbox)
            data.isRegex = this._isRegexCheckbox.checked;

        // No change.
        let oldSerialized = JSON.stringify(this._serializedDataWhenShown);
        let newSerialized = JSON.stringify(data);
        if (oldSerialized === newSerialized)
            return null;

        return data;
    }

    show(localResourceOverride, targetElement, preferredEdges)
    {
        this._targetElement = targetElement;
        this._preferredEdges = preferredEdges;

        let localResource = localResourceOverride ? localResourceOverride.localResource : null;

        let data = {};
        let resourceData = {};
        if (localResource) {
            data.url = resourceData.url = localResource.url;
            data.mimeType = resourceData.mimeType = localResource.mimeType;
            data.statusCode = resourceData.statusCode = String(localResource.statusCode);
            data.statusText = resourceData.statusText = localResource.statusText;
        }

        if (!data.url)
            data.url = this._defaultURL();

        if (!data.mimeType)
            data.mimeType = "text/javascript";

        if (!data.statusCode || data.statusCode === "NaN") {
            data.statusCode = "200";
            resourceData.statusCode = undefined;
        }

        if (!data.statusText) {
            data.statusText = WI.HTTPUtilities.statusTextForStatusCode(parseInt(data.statusCode));
            resourceData.statusText = undefined;
        }

        let responseHeaders = localResource ? localResource.responseHeaders : {};

        let popoverContentElement = document.createElement("div");
        popoverContentElement.className = "local-resource-override-popover-content";

        let table = popoverContentElement.appendChild(document.createElement("table"));

        let createRow = (label, id, value, placeholder) => {
            let row = table.appendChild(document.createElement("tr"));
            let headerElement = row.appendChild(document.createElement("th"));
            let dataElement = row.appendChild(document.createElement("td"));

            let labelElement = headerElement.appendChild(document.createElement("label"));
            labelElement.textContent = label;

            let editorElement = dataElement.appendChild(document.createElement("div"));
            editorElement.classList.add("editor", id);

            let codeMirror = this._createEditor(editorElement, {value, placeholder});
            let inputField = codeMirror.getInputField();
            inputField.id = `local-resource-override-popover-${id}-input-field`;
            labelElement.setAttribute("for", inputField.id);

            return {codeMirror, dataElement};
        };

        let urlRow = createRow(WI.UIString("URL"), "url", resourceData.url || "", data.url);
        this._urlCodeMirror = urlRow.codeMirror;

        let updateURLCodeMirrorMode = () => {
            let isRegex = this._isRegexCheckbox && this._isRegexCheckbox.checked;

            this._urlCodeMirror.setOption("mode", isRegex ? "text/x-regex" : "text/x-local-override-url");

            if (!isRegex) {
                let url = this._urlCodeMirror.getValue();
                if (url) {
                    const schemes = ["http:", "https:", "file:"];
                    if (!schemes.some((scheme) => url.toLowerCase().startsWith(scheme)))
                        this._urlCodeMirror.setValue("http://" + url);
                }
            }
        };

        if (InspectorBackend.hasCommand("Network.addInterception", "caseSensitive")) {
            let isCaseSensitiveLabel = urlRow.dataElement.appendChild(document.createElement("label"));
            isCaseSensitiveLabel.className = "is-case-sensitive";

            this._isCaseSensitiveCheckbox = isCaseSensitiveLabel.appendChild(document.createElement("input"));
            this._isCaseSensitiveCheckbox.type = "checkbox";
            this._isCaseSensitiveCheckbox.checked = localResourceOverride ? localResourceOverride.isCaseSensitive : true;

            isCaseSensitiveLabel.append(WI.UIString("Case Sensitive"));
        }

        if (InspectorBackend.hasCommand("Network.addInterception", "isRegex")) {
            let isRegexLabel = urlRow.dataElement.appendChild(document.createElement("label"));
            isRegexLabel.className = "is-regex";

            this._isRegexCheckbox = isRegexLabel.appendChild(document.createElement("input"));
            this._isRegexCheckbox.type = "checkbox";
            this._isRegexCheckbox.checked = localResourceOverride ? localResourceOverride.isRegex : false;
            this._isRegexCheckbox.addEventListener("change", (event) => {
                updateURLCodeMirrorMode();
            });

            isRegexLabel.append(WI.UIString("Regular Expression"));
        }

        let mimeTypeRow = createRow(WI.UIString("MIME Type"), "mime", resourceData.mimeType || "", data.mimeType);
        this._mimeTypeCodeMirror = mimeTypeRow.codeMirror;

        let statusCodeRow = createRow(WI.UIString("Status"), "status", resourceData.statusCode || "", data.statusCode);
        this._statusCodeCodeMirror = statusCodeRow.codeMirror;

        let statusTextEditorElement = statusCodeRow.dataElement.appendChild(document.createElement("div"));
        statusTextEditorElement.className = "editor status-text";
        this._statusTextCodeMirror = this._createEditor(statusTextEditorElement, {value: resourceData.statusText || "", placeholder: data.statusText});

        let editCallback = () => {};
        let deleteCallback = (node) => {
            if (node === contentTypeDataGridNode)
                return;

            let siblingToSelect = node.nextSibling || node.previousSibling;
            this._headersDataGrid.removeChild(node);
            if (siblingToSelect)
                siblingToSelect.select();

            this._headersDataGrid.updateLayoutIfNeeded();
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

        this._headersDataGrid = new WI.DataGrid(columns, {editCallback, deleteCallback});
        this._headersDataGrid.inline = true;
        this._headersDataGrid.variableHeightRows = true;
        this._headersDataGrid.copyTextDelimiter = ": ";

        let addDataGridNodeForHeader = (name, value, options = {}) => {
            let node = new WI.DataGridNode({name, value}, options);
            this._headersDataGrid.appendChild(node);
            return node;
        };

        let contentTypeDataGridNode = addDataGridNodeForHeader("Content-Type", data.mimeType, {selectable: false, editable: false, classNames: ["header-content-type"]});

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
        headersData.appendChild(this._headersDataGrid.element);
        this._headersDataGrid.updateLayoutIfNeeded();

        let addHeaderButton = headersData.appendChild(document.createElement("button"));
        addHeaderButton.className = "add-header";
        addHeaderButton.textContent = WI.UIString("Add Header");
        addHeaderButton.addEventListener("click", (event) => {
            let newNode = new WI.DataGridNode({name: "Header", value: "value"});
            this._headersDataGrid.appendChild(newNode);
            this._headersDataGrid.updateLayoutIfNeeded();
            this.update();
            this._headersDataGrid.startEditingNode(newNode);
        });

        headersData.appendChild(WI.createReferencePageLink("local-overrides", "configuring-local-overrides"));

        let incrementStatusCode = () => {
            let x = parseInt(this._statusCodeCodeMirror.getValue());
            if (isNaN(x))
                x = parseInt(this._statusCodeCodeMirror.getOption("placeholder"));
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
            if (isNaN(x))
                x = parseInt(this._statusCodeCodeMirror.getOption("placeholder"));
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
            if (isNaN(statusCode)) {
                this._statusTextCodeMirror.setValue("");
                return;
            }

            let statusText = WI.HTTPUtilities.statusTextForStatusCode(statusCode);
            this._statusTextCodeMirror.setValue(statusText);
        });

        // Update mimeType when URL gets a file extension.
        this._urlCodeMirror.on("change", (cm) => {
            if (this._isRegexCheckbox && this._isRegexCheckbox.checked)
                return;

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
            let mimeType = cm.getValue() || cm.getOption("placeholder");
            contentTypeDataGridNode.data = {name: "Content-Type", value: mimeType};
        });

        updateURLCodeMirrorMode();

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

            this.update();
        });
    }

    // Private

    _createEditor(element, options = {})
    {
        let codeMirror = WI.CodeMirrorEditor.create(element, {
            extraKeys: {"Tab": false, "Shift-Tab": false},
            lineWrapping: false,
            mode: "text/plain",
            matchBrackets: true,
            scrollbarStyle: null,
            ...options,
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
        this.present(targetFrame.pad(2), this._preferredEdges);
    }
};
