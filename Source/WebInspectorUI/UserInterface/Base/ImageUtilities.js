/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

function useSVGSymbol(url, className, title)
{
    const svgNamespace = "http://www.w3.org/2000/svg";
    const xlinkNamespace = "http://www.w3.org/1999/xlink";

    let svgElement = document.createElementNS(svgNamespace, "svg");
    svgElement.style.width = "100%";
    svgElement.style.height = "100%";

    // URL must contain a fragment reference to a graphical element, like a symbol. If none is given
    // append #root which all of our SVGs have on the top level <svg> element.
    if (!url.includes("#"))
        url += "#root";

    let useElement = document.createElementNS(svgNamespace, "use");
    useElement.setAttributeNS(xlinkNamespace, "xlink:href", url);
    svgElement.appendChild(useElement);

    let wrapper = document.createElement("div");
    wrapper.appendChild(svgElement);

    if (className)
        wrapper.className = className;
    if (title)
        wrapper.title = title;

    return wrapper;
}

(function() {
    let canvas = null;
    let context = null;

    WI.scratchCanvasContext2D = function(callback) {
        if (!canvas)
            canvas = document.createElement("canvas");

        if (!context)
            context = canvas.getContext("2d");

        context.clearRect(0, 0, canvas.width, canvas.height);
        context.save();
        callback(context);
        context.restore();
    };
})();

WI.imageFromImageData = function(data) {
    console.assert(data instanceof ImageData);

    let image = null;
    WI.scratchCanvasContext2D((context) => {
        context.canvas.width = data.width;
        context.canvas.height = data.height;
        context.putImageData(data, 0, 0);

        image = new Image;
        image.src = context.canvas.toDataURL();
    });
    return image;
};

WI.imageFromCanvasGradient = function(gradient, width, height) {
    console.assert(gradient instanceof CanvasGradient);

    let image = null;
    WI.scratchCanvasContext2D((context) => {
        context.canvas.width = width;
        context.canvas.height = height;
        context.fillStyle = gradient;
        context.fillRect(0, 0, width, height);

        image = new Image;
        image.src = context.canvas.toDataURL();
    });
    return image;
};
