"use strict";

async function digestMessage(message) {
    const encoder = new TextEncoder();
    const data = encoder.encode(message);
    const hashBuffer = await crypto.subtle.digest('SHA-256', data);
    const hashArray = Array.from(new Uint8Array(hashBuffer));
    const hashHex = hashArray.map((b) => b.toString(16).padStart(2, '0')).join('');
    return hashHex;
}

function fullHorizontalLinearGradientCanvasImageData() {
    let canvas = document.createElement("canvas");
    canvas.style = "border: black; border-style: dashed;";
    canvas.width = 30;
    canvas.height = 2;
    document.body.appendChild(canvas);
    let ctx = canvas.getContext("2d");

    let gradient = ctx.createLinearGradient(1, 0, 29, 0);
    gradient.addColorStop(0, "green");
    gradient.addColorStop(1, "black");
    ctx.fillStyle = gradient;
    ctx.fillRect(1, 0, 29, 2);
    return ctx.getImageData(0, 0, canvas.width, canvas.height);
}

function fullVerticalLinearGradientCanvasImageData() {
    let canvas = document.createElement("canvas");
    canvas.style = "border: black; border-style: dashed;";
    canvas.width = 2;
    canvas.height = 30;
    document.body.appendChild(canvas);
    let ctx = canvas.getContext("2d");

    let gradient = ctx.createLinearGradient(0, 1, 0, 29);
    gradient.addColorStop(0, "green");
    gradient.addColorStop(1, "black");
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 1, 2, 29);
    return ctx.getImageData(0, 0, canvas.width, canvas.height);
}

function fullRadialGradientCanvasImageData() {
    let canvas = document.createElement("canvas");
    canvas.style = "border: black; border-style: dashed;";
    canvas.width = 30;
    canvas.height = 30;
    document.body.appendChild(canvas);
    let ctx = canvas.getContext("2d");

    let gradient = ctx.createRadialGradient(15, 15, 5, 15, 15, 12);
    gradient.addColorStop(0, "green");
    gradient.addColorStop(1, "black");
    ctx.fillStyle = gradient;
    ctx.fillRect(1, 1, 29, 29);
    return ctx.getImageData(0, 0, canvas.width, canvas.height);
}

function webGLGradientCanvasDataURL() {
    let canvas = document.createElement("canvas");
    canvas.id = "glcanvas";
    canvas.style = "border: black; border-style: dashed;";
    canvas.width = 640;
    canvas.height = 480;
    document.body.appendChild(canvas);

    return getCanvasDataURL(canvas);
}

function fullTextCanvasImageData() {
    let canvas = document.createElement("canvas");
    canvas.style = "border: black; border-style: dashed;";
    document.body.appendChild(canvas);
    let ctx = canvas.getContext("2d");

    // Text with lowercase/uppercase/punctuation symbols (inspired by BrowserLeaks)
    ctx.beginPath();
    let txt = "WebKit,org <canvas> 1.0";
    ctx.textBaseline = "top";
    // The most common type
    ctx.font = "14px 'Arial'";
    ctx.textBaseline = "alphabetic";
    ctx.fillStyle = "#f60";
    ctx.fillRect(80,1,62,20);
    // Some tricks for color mixing to increase the difference in rendering
    ctx.fillStyle = "#069";
    ctx.fillText(txt, 2, 15);
    ctx.fillStyle = "rgba(102, 204, 0, 0.7)";
    ctx.fillText(txt, 4, 17);

    return ctx.getImageData(0, 0, canvas.width, canvas.height);
}

function initialCanvasImageDataAsObject(imageData, length) {
    if (length < 0)
        return {};
    if (length > imageData.data.length)
        length = imageData.data.length;
    let obj = {};
    for (let i = 0; i < length; ++i)
      obj[i] = imageData.data[i];
    return obj;
}

function isHorizontalLinearGradientCanvasGradient() {
    let imageData = fullHorizontalLinearGradientCanvasImageData();
    for (let i = 4; i < 29*4; ++i) {
        if (imageData.data[i - 4] <= imageData.data[i] && imageData.data[i] <= imageData.data[i + 4])
            continue;
        if (imageData.data[i - 4] >= imageData.data[i] && imageData.data[i] >= imageData.data[i + 4])
            continue;
        return false;
    }
    return true;
}

function isVerticalLinearGradientCanvasGradient() {
    let imageData = fullVerticalLinearGradientCanvasImageData();
    let prevColor = [ imageData.data[0], imageData.data[1], imageData.data[2], imageData.data[3] ];
    for (let i = 8; i < 29*4; ++i) {
        if (imageData.data[i - 4*2] <= imageData.data[i] && imageData.data[i] <= imageData.data[i + 4*2])
            continue;
        if (imageData.data[i - 4*2] >= imageData.data[i] && imageData.data[i] >= imageData.data[i + 4*2])
            continue;
        return false;
    }
    return true;
}

function initialTextCanvasImageDataAsObject(length) {
    return initialCanvasImageDataAsObject(fullTextCanvasImageData(), length);
}

function initialHorizontalLinearGradientCanvasImageDataAsObject(length) {
    return initialCanvasImageDataAsObject(fullHorizontalLinearGradientCanvasImageData(), length);
}

function initialVerticalLinearGradientCanvasImageDataAsObject(length) {
    return initialCanvasImageDataAsObject(fullVerticalLinearGradientCanvasImageData(), length);
}

function initialRadialGradientCanvasImageDataAsObject(length) {
    return initialCanvasImageDataAsObject(fullRadialGradientCanvasImageData(), length);
}

async function fullCanvasHash(data) {
    if (!data)
        data = fullTextCanvasImageData();
    return await digestMessage(data.data);
}
