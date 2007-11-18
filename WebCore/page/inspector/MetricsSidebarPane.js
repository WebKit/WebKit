/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.MetricsSidebarPane = function()
{
    WebInspector.SidebarPane.call(this, WebInspector.UIString("Metrics"));
}

WebInspector.MetricsSidebarPane.prototype = {
    update: function(node)
    {
        var body = this.bodyElement;

        body.removeChildren();

        if (!node)
            return;

        var style;
        if (node.nodeType === Node.ELEMENT_NODE)
            style = node.ownerDocument.defaultView.getComputedStyle(node);
        if (!style)
            return;

        var metricsElement = document.createElement("div");
        metricsElement.className = "metrics";

        function boxPartValue(style, name, suffix)
        {
            var value = style.getPropertyValue(name + suffix);
            if (value === "" || value === "0px")
                value = "\u2012";
            return value.replace(/px$/, "");
        }

        // Display types for which margin is ignored.
        var noMarginDisplayType = {
            "table-cell": true,
            "table-column": true,
            "table-column-group": true,
            "table-footer-group": true,
            "table-header-group": true,
            "table-row": true,
            "table-row-group": true
        };

        // Display types for which padding is ignored.
        var noPaddingDisplayType = {
            "table-column": true,
            "table-column-group": true,
            "table-footer-group": true,
            "table-header-group": true,
            "table-row": true,
            "table-row-group": true
        };

        var boxes = ["content", "padding", "border", "margin"];
        var boxLabels = [WebInspector.UIString("content"), WebInspector.UIString("padding"), WebInspector.UIString("border"), WebInspector.UIString("margin")];
        var previousBox;
        for (var i = 0; i < boxes.length; ++i) {
            var name = boxes[i];

            if (name === "margin" && noMarginDisplayType[style.display])
                continue;
            if (name === "padding" && noPaddingDisplayType[style.display])
                continue;

            var boxElement = document.createElement("div");
            boxElement.className = name;

            if (name === "content") {
                var width = style.width.replace(/px$/, "");
                var height = style.height.replace(/px$/, "");
                boxElement.textContent = width + " \u00D7 " + height;
            } else {
                var suffix = (name === "border" ? "-width" : "");

                var labelElement = document.createElement("div");
                labelElement.className = "label";
                labelElement.textContent = boxLabels[i];
                boxElement.appendChild(labelElement);

                var topElement = document.createElement("div");
                topElement.className = "top";
                topElement.textContent = boxPartValue(style, name + "-top", suffix);
                boxElement.appendChild(topElement);

                var leftElement = document.createElement("div");
                leftElement.className = "left";
                leftElement.textContent = boxPartValue(style, name + "-left", suffix);
                boxElement.appendChild(leftElement);

                if (previousBox)
                    boxElement.appendChild(previousBox);

                var rightElement = document.createElement("div");
                rightElement.className = "right";
                rightElement.textContent = boxPartValue(style, name + "-right", suffix);
                boxElement.appendChild(rightElement);

                var bottomElement = document.createElement("div");
                bottomElement.className = "bottom";
                bottomElement.textContent = boxPartValue(style, name + "-bottom", suffix);
                boxElement.appendChild(bottomElement);
            }

            previousBox = boxElement;
        }

        metricsElement.appendChild(previousBox);
        body.appendChild(metricsElement);
    }
}

WebInspector.MetricsSidebarPane.prototype.__proto__ = WebInspector.SidebarPane.prototype;
