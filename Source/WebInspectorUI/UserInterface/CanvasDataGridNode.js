/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.CanvasDataGridNode = function(profileView, data)
{
    WebInspector.DataGridNode.call(this, data, false);
    this._profileView = profileView;
}

WebInspector.CanvasDataGridNode.prototype = {

    constructor: WebInspector.CanvasDataGridNode,

    get data()
    {
        var data = {};
        return data;
    },

    get rawData()
    {
        return this._data;
    },

    createCell: function(columnIdentifier)
    {
        var cell = WebInspector.DataGridNode.prototype.createCell.call(this, columnIdentifier);
        cell.removeChildren();

        if (this.rawData.url) {
            var wrapperDiv = cell.createChild("div");
            wrapperDiv.appendChild(this._linkifyLocation(this.rawData.url, this.rawData.lineNumber, this.rawData.columnNumber));
        }

        return cell;
    },

    _linkifyLocation: function(url, lineNumber, columnNumber)
    {
        return WebInspector.linkifyLocation(url, lineNumber, columnNumber || 0);
    }
}

WebInspector.CanvasDataGridNode.prototype.__proto__ = WebInspector.DataGridNode.prototype;

