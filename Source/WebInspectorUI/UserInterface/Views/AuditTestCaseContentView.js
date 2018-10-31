/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

WI.AuditTestCaseContentView = class AuditTestCaseContentView extends WI.AuditTestContentView
{
    constructor(representedObject)
    {
        console.assert(representedObject instanceof WI.AuditTestCase || representedObject instanceof WI.AuditTestCaseResult);

        super(representedObject);

        this.element.classList.add("audit-test-case");
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();

        let nameElement = this.headerView.element.appendChild(document.createElement("h1"));

        this._resultImageElement = nameElement.appendChild(document.createElement("img"));

        nameElement.appendChild(document.createTextNode(this.representedObject.name));

        if (this.representedObject.description) {
            let descriptionElement = this.headerView.element.appendChild(document.createElement("p"));
            descriptionElement.textContent = this.representedObject.description;
        }
    }

    layout()
    {
        if (this.layoutReason !== WI.View.LayoutReason.Dirty)
            return;

        super.layout();

        this._resultImageElement.src = "Images/AuditTestNoResult.svg";

        this.contentView.element.removeChildren();

        let result = this.representedObject.result;
        if (!result) {
            if (this.representedObject.runningState === WI.AuditManager.RunningState.Inactive)
                this.showNoResultPlaceholder();
            else if (this.representedObject.runningState === WI.AuditManager.RunningState.Active)
                this.showRunningPlaceholder();
            else if (this.representedObject.runningState === WI.AuditManager.RunningState.Stopping)
                this.showStoppingPlaceholder();

            return;
        }

        if (result.didError)
            this._resultImageElement.src = "Images/AuditTestError.svg";
        else if (result.didFail)
            this._resultImageElement.src = "Images/AuditTestFail.svg";
        else if (result.didWarn)
            this._resultImageElement.src = "Images/AuditTestWarn.svg";
        else if (result.didPass)
            this._resultImageElement.src = "Images/AuditTestPass.svg";
        else if (result.unsupported)
            this._resultImageElement.src = "Images/AuditTestUnsupported.svg";

        let resultData = result.data;

        if (resultData.domNodes && resultData.domNodes.length) {
            let domNodesContainer = this.contentView.element.appendChild(document.createElement("div"));
            domNodesContainer.classList.add("dom-nodes");

            let domNodeText = domNodesContainer.appendChild(document.createElement("h1"));
            domNodeText.textContent = WI.UIString("DOM Nodes:");

            let tableContainer = domNodesContainer.appendChild(document.createElement("table"));

            resultData.domNodes.forEach((domNode, index) => {
                let rowElement = tableContainer.appendChild(document.createElement("tr"));

                let indexElement = rowElement.appendChild(document.createElement("td"));
                indexElement.textContent = index + 1;

                let dataElement = rowElement.appendChild(document.createElement("td"));

                if (domNode instanceof WI.DOMNode) {
                    let treeOutline = new WI.DOMTreeOutline;
                    treeOutline.setVisible(true);
                    treeOutline.rootDOMNode = domNode;

                    let rootTreeElement = treeOutline.children[0];
                    if (!rootTreeElement.hasChildren)
                        treeOutline.element.classList.add("single-node");

                    if (resultData.domAttributes) {
                        for (let domAttribute of resultData.domAttributes) {
                            rootTreeElement.highlightAttribute(domAttribute);
                            rootTreeElement.updateTitle();
                        }
                    }

                    dataElement.appendChild(treeOutline.element);
                } else if (typeof domNode === "string") {
                    let codeMirror = WI.CodeMirrorEditor.create(dataElement.appendChild(document.createElement("code")), {
                        mode: "css",
                        readOnly: true,
                        lineWrapping: true,
                        showWhitespaceCharacters: WI.settings.showWhitespaceCharacters.value,
                        styleSelectedText: true,
                    });
                    codeMirror.setValue(domNode);

                    if (resultData.domAttributes) {
                        for (let domAttribute of resultData.domAttributes) {
                            let regex = null;
                            if (domAttribute === "id")
                                regex = /(\#[^\#|\.|\[|\s|$]+)/g;
                            else if (domAttribute === "class")
                                regex = /(\.[^\#|\.|\[|\s|$]+)/g;
                            else
                                regex = new RegExp(`\\[\\s*(${domAttribute})\\s*=`, "g");

                            while (true) {
                                let match = regex.exec(domNode);
                                if (!match)
                                    break;

                                let start = match.index + match[0].indexOf(match[1]);
                                codeMirror.markText({line: 0, ch: start}, {line: 0, ch: start + match[1].length}, {className: "mark"});
                            }
                        }
                    }
                }
            });
        }

        if (resultData.errors && resultData.errors.length) {
            let errorContainer = this.contentView.element.appendChild(document.createElement("div"));
            errorContainer.classList.add("errors");

            let errorText = errorContainer.appendChild(document.createElement("h1"));
            errorText.textContent = WI.UIString("Errors:");

            let tableContainer = errorContainer.appendChild(document.createElement("table"));

            resultData.errors.forEach((error, index) => {
                let rowElement = tableContainer.appendChild(document.createElement("tr"));

                let indexElement = rowElement.appendChild(document.createElement("td"));
                indexElement.textContent = index + 1;

                let dataElement = rowElement.appendChild(document.createElement("td"));

                let errorElement = dataElement.appendChild(document.createElement("div"));
                errorElement.classList.add("error");
                errorElement.textContent = error;
            });
        }

        if (!this.contentView.element.children.length)
            this.showNoResultDataPlaceholder();
    }

    showRunningPlaceholder()
    {
        if (!this.placeholderElement || !this.placeholderElement.__placeholderRunning) {
            this.placeholderElement = WI.createMessageTextView(WI.UIString("Running the “%s“ audit").format(this.representedObject.name));
            this.placeholderElement.__placeholderRunning = true;

            let spinner = new WI.IndeterminateProgressSpinner;
            this.placeholderElement.appendChild(spinner.element);
        }

        super.showRunningPlaceholder();
    }
};
