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

WI.DOMTreeDataGridNode = class DOMTreeDataGridNode extends WI.DataGridNode
{
    constructor(domNode)
    {
        super();

        this._domNode = domNode;
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

        return this._createNameCellDocumentFragment();
    }

    // Private

    _createNameCellDocumentFragment()
    {
        let fragment = document.createDocumentFragment();
        let mainTitle = this._domNode.displayName;
        fragment.append(mainTitle);

        let goToButton = fragment.appendChild(WI.createGoToArrowButton());
        goToButton.addEventListener("click", this._goToArrowWasClicked.bind(this), false);

        return fragment;
    }

    _goToArrowWasClicked()
    {
        WI.showMainFrameDOMTree(this._domNode);
    }
};
