// Based on http://philip.html5.org/tests/canvas/suite/tests/2d.pattern.modify.canvas1.html

description("This test checks if pattern changes after the source canvas is modified. See https://bugs.webkit.org/show_bug.cgi?id=20578 .");

function dataToArray(data) {
    var result = new Array(data.length)
    for (var i = 0; i < data.length; i++)
        result[i] = data[i];
    return result;
}

function getPixel(ctx, x, y) {
    var data = ctx.getImageData(x,y,1,1);
    if (!data) // getImageData failed, which should never happen
        return [-1,-1,-1,-1];
    return dataToArray(data.data);
}

function pixelShouldBe(ctx, x, y, colour) {
    shouldBe("getPixel(ctx, " + [x, y] +")", "["+colour+"]");
}

function createCanvasImage(width, height, colour) {
    var c = document.createElement("canvas");
    c.width = width;
    c.height = height;
    var context = c.getContext("2d");
    context.fillStyle = colour;
    context.fillRect(0,0,width,height);
    return c;
}

var canvas = createCanvasImage(100, 50, '#fff');
var ctx = canvas.getContext('2d');

var patternCanvas = createCanvasImage(100, 50, '#0f0');
var pattern = ctx.createPattern(patternCanvas, 'no-repeat');

// Modify the original canvas after we create a pattern.
var patternCtx = patternCanvas.getContext('2d');
patternCtx.fillStyle = '#f00';
patternCtx.fillRect(0, 0, 100, 50);

ctx.fillStyle = pattern;
ctx.fillRect(0, 0, 100, 50);

pixelShouldBe(ctx, 1, 1, [0, 255, 0, 255]);
pixelShouldBe(ctx, 98, 1, [0, 255, 0, 255]);
pixelShouldBe(ctx, 1, 48, [0, 255, 0, 255]);
pixelShouldBe(ctx, 98, 48, [0, 255, 0, 255]);
