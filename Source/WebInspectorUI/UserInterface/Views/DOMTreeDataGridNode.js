/*
 * Copyright (C) 2013 Adobe Systems Inc. All rights reserved.
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

WebInspector.DOMTreeDataGridNode = class DOMTreeDataGridNode extends WebInspector.DataGridNode
{
    constructor(domNode)
    {
        super();

        this._nameLabel = null;
        this._domNode = domNode;
        this._updateNodeName();
    }

    get domNode()
    {
        return this._domNode;
    }

    // DataGridNode Overrides.

    createCellContent(columnIdentifier, cell)
    {
        if (columnIdentifier !== "name")
            return super.createCellContent(columnIdentifier, cell);

        var cell = this._makeNameCell();
        this._updateNameCellData();
        return cell;
    }

    // Private

    _updateNodeName()
    {
        this.data = {name: WebInspector.displayNameForNode(this._domNode)};
    }

    _makeNameCell()
    {
        var fragment = document.createDocumentFragment();

        fragment.appendChild(document.createElement("img")).classList.add("icon");

        this._nameLabel = document.createElement("div");
        this._nameLabel.classList.add("label");
        fragment.appendChild(this._nameLabel);

        var goToButton = fragment.appendChild(WebInspector.createGoToArrowButton());
        goToButton.addEventListener("click", this._goToArrowWasClicked.bind(this), false);

        return fragment;
    }

    _updateNameCellData()
    {
        this._nameLabel.textContent = this.data.name;
    }

    _goToArrowWasClicked()
    {
        WebInspector.showMainFrameDOMTree(this._domNode);
    }
};
