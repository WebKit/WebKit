/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
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

WebInspector.FontView = function(resource)
{
    WebInspector.ResourceView.call(this, resource);

    this.element.addStyleClass("font");

    var uniqueFontName = "WebInspectorFontPreview" + this.resource.identifier;

    this.fontStyleElement = document.createElement("style");
    this.fontStyleElement.textContent = "@font-face { font-family: \"" + uniqueFontName + "\"; src: url(" + this.resource.url + "); }";
    document.head.appendChild(this.fontStyleElement);

    this.fontPreviewElement = document.createElement("div");
    this.fontPreviewElement.className = "preview";
    this.contentElement.appendChild(this.fontPreviewElement);

    this.fontPreviewElement.style.setProperty("font-family", uniqueFontName, null);
    this.fontPreviewElement.innerHTML = "ABCDEFGHIJKLM<br>NOPQRSTUVWXYZ<br>abcdefghijklm<br>nopqrstuvwxyz<br>1234567890";

    this.updateFontPreviewSize();
}

WebInspector.FontView.prototype = {
    show: function(parentElement)
    {
        WebInspector.ResourceView.prototype.show.call(this, parentElement);
        this.updateFontPreviewSize();
    },

    resize: function()
    {
        this.updateFontPreviewSize();
    },

    updateFontPreviewSize: function ()
    {
        if (!this.fontPreviewElement || !this.visible)
            return;

        this.fontPreviewElement.removeStyleClass("preview");

        var measureFontSize = 50;
        this.fontPreviewElement.style.setProperty("position", "absolute", null);
        this.fontPreviewElement.style.setProperty("font-size", measureFontSize + "px", null);
        this.fontPreviewElement.style.removeProperty("height");

        var height = this.fontPreviewElement.offsetHeight;
        var width = this.fontPreviewElement.offsetWidth;

        var containerWidth = this.contentElement.offsetWidth;

        // Subtract some padding. This should match the padding in the CSS plus room for the scrollbar.
        containerWidth -= 40;

        if (!height || !width || !containerWidth) {
            this.fontPreviewElement.style.removeProperty("font-size");
            this.fontPreviewElement.style.removeProperty("position");
            this.fontPreviewElement.addStyleClass("preview");
            return;
        }

        var lineCount = this.fontPreviewElement.getElementsByTagName("br").length + 1;
        var realLineHeight = Math.floor(height / lineCount);
        var fontSizeLineRatio = measureFontSize / realLineHeight;
        var widthRatio = containerWidth / width;
        var finalFontSize = Math.floor(realLineHeight * widthRatio * fontSizeLineRatio) - 1;

        this.fontPreviewElement.style.setProperty("font-size", finalFontSize + "px", null);
        this.fontPreviewElement.style.setProperty("height", this.fontPreviewElement.offsetHeight + "px", null);
        this.fontPreviewElement.style.removeProperty("position");

        this.fontPreviewElement.addStyleClass("preview");
    }
}

WebInspector.FontView.prototype.__proto__ = WebInspector.ResourceView.prototype;
