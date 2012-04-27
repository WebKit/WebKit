/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @constructor
 * @extends {WebInspector.View}
 */
WebInspector.ResourceWebSocketFrameView = function(resource)
{
    WebInspector.View.call(this);
    this.element.addStyleClass("html");
    this.resource = resource;
    this.element.removeChildren();
    var table = document.createElement("table");
    var header = document.createElement("thead");

    var headerRow = document.createElement("tr");
    headerRow.createChild("td");

    var timeHeader = document.createElement("td");
    timeHeader.innerText = WebInspector.UIString("Time");
    headerRow.appendChild(timeHeader);

    var opCodeHeader = document.createElement("td");
    opCodeHeader.innerText = WebInspector.UIString("OpCode");
    headerRow.appendChild(opCodeHeader);

    var maskHeader = document.createElement("td");
    maskHeader.innerText = WebInspector.UIString("Mask");
    headerRow.appendChild(maskHeader);

    var lengthHeader = document.createElement("td");
    lengthHeader.innerText = WebInspector.UIString("Length");
    headerRow.appendChild(lengthHeader);

    var dataHeader = document.createElement("td");
    dataHeader.innerText = WebInspector.UIString("Data");
    headerRow.appendChild(dataHeader);
    table.appendChild(headerRow);

    var frames = this.resource.frames();
    for (var i = 0; i < frames.length; i++) {
        var payload = frames[i];

        var row = document.createElement("tr");

        var count = document.createElement("td");
        count.innerText = i + 1;
        row.appendChild(count);

        var dateCell = document.createElement("td");
        var date = new Date();
        date.setTime(payload.time * 1000);
        dateCell.innerText = String.sprintf("%s %s", (payload.sent ? "\u2192" : (payload.errorMessage ? "" : "\u2190")), date.toISOString());
        row.appendChild(dateCell);

        if (payload.errorMessage) {
            var spanCell = document.createElement("td");
            spanCell.setAttribute("colspan", 4);
            spanCell.innerText = payload.errorMessage;
            row.appendChild(spanCell);
        } else {
            var opcode = document.createElement("td");
            opcode.innerText = payload.opcode;
            row.appendChild(opcode);

            var mask = document.createElement("td");
            mask.innerText = payload.mask;
            row.appendChild(mask);

            var length = document.createElement("td");
            length.innerText = payload.payloadData.length;
            row.appendChild(length);

            var data = document.createElement("td");
            data.textContent = payload.payloadData;
            row.appendChild(data);
        }
        table.appendChild(row);
    }
    this.element.appendChild(table);
}

WebInspector.ResourceWebSocketFrameView.prototype.__proto__ = WebInspector.View.prototype;
