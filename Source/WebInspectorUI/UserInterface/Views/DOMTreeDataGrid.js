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

WI.DOMTreeDataGrid = class DOMTreeDataGrid extends WI.DataGrid
{
    constructor()
    {
        super({
            name: {title: WI.UIString("Node"), sortable: false, icon: true}
        });

        this._previousHoveredElement = null;

        this.inline = true;

        this.element.classList.add("dom-tree-data-grid");

        this.element.addEventListener("mousemove", this._onmousemove.bind(this), false);
        this.element.addEventListener("mouseout", this._onmouseout.bind(this), false);
    }

    // Private

    _onmousemove(event)
    {
        var gridNode = this.dataGridNodeFromNode(event.target);
        if (!gridNode || this._previousHoveredElement === gridNode.domNode)
            return;
        this._previousHoveredElement = gridNode.domNode;
        WI.domManager.highlightDOMNode(gridNode.domNode.id);
    }

    _onmouseout(event) {
        if (!this._previousHoveredElement)
            return;
        WI.domManager.hideDOMNodeHighlight();
        this._previousHoveredElement = null;
    }
};
