description("This test ensures that putImageData works correctly, the end result should be a 100x100px green square.");

function fillRect(imageData, x, y, width, height, r, g, b, a)
{
    var bytesPerRow = imageData.width * 4;
    var data =imageData.data;
    for (var i = 0; i < height; i++) {
        var rowOrigin = (y+i) * bytesPerRow;
        rowOrigin += x * 4;
        for (var j = 0; j < width; j++) {
            var position = rowOrigin + j * 4;
            data[position + 0] = r;
            data[position + 1] = g;
            data[position + 2] = b;
            data[position + 3] = a;
        }
    }
}

function dataToArray(data) {
    var result = new Array(data.length)
    for (var i = 0; i < data.length; i++)
        result[i] = data[i];
    return result;
}

function getPixel(x, y) {
    var data = context.getImageData(x,y,1,1);
    if (!data) // getImageData failed, which should never happen
        return [-1,-1,-1,-1];
    return dataToArray(data.data);
}

function pixelShouldBe(x, y, colour) {
    shouldBe("getPixel(" + [x, y] +")", "["+colour+"]");
}

var canvas = document.getElementById("canvas");
var context = canvas.getContext("2d");

if (!context.createImageData)
    context.createImageData = function(w,h) { 
        var data = this.getImageData(0, 0, w, h);
        for (var i = 0; i < data.data.length; i++)
            data.data[i] = 0;
    }
var buffer = context.createImageData(100,100);
// Fill top left corner
fillRect(buffer, 0, 0, 50, 50, 0, 128,0,255);
context.putImageData(buffer, 0, 0);
pixelShouldBe( 0,  0, [0, 128,0,255]);
pixelShouldBe(25, 25, [0, 128,0,255]);
pixelShouldBe(49,  0, [0, 128,0,255]);
pixelShouldBe( 0, 49, [0, 128,0,255]);
pixelShouldBe(49, 49, [0, 128,0,255]);
pixelShouldBe(50,  0, [0, 0, 0, 0]);
pixelShouldBe( 0, 50, [0, 0, 0, 0]);
pixelShouldBe(50, 50, [0, 0, 0, 0]);

// Test positioned drawing -- make bottom right green
context.putImageData(buffer, 0, 50);
pixelShouldBe( 0, 50, [0, 128,0,255]);
pixelShouldBe(25, 75, [0, 128,0,255]);
pixelShouldBe(49, 50, [0, 128,0,255]);
pixelShouldBe( 0, 99, [0, 128,0,255]);
pixelShouldBe(49, 99, [0, 128,0,255]);

// Test translation doesn't effect putImageData
context.translate(50, -50);
context.putImageData(buffer, 50, 50);
pixelShouldBe(50, 50, [0, 128,0,255]);
pixelShouldBe(75, 75, [0, 128,0,255]);
pixelShouldBe(99, 99, [0, 128,0,255]);
pixelShouldBe(50, 49, [0, 0, 0, 0]);
context.translate(-50, 50);

// Test dirty rect handling
buffer = context.createImageData(50,50);
fillRect(buffer, 0, 0, 50, 50, 0, 128, 0, 255);
context.putImageData(buffer, 50, 0);
fillRect(buffer, 0, 0, 50, 50, 255, 0, 0, 255);
context.putImageData(buffer, 50, 0, 15, 15, 20, 20);
context.fillStyle="rgb(0,128,0)";
context.fillRect(65, 15, 20, 20);
var points = [0, 5, 15, 25, 35, 45];
for (var x = 0; x < points.length; x++)
    for (var y = 0; y < points.length; y++)
        pixelShouldBe(points[x] + 50, points[y], [0, 128, 0, 255]);

// Test drawing outside the canvas border
fillRect(buffer, 0, 0, 50, 50, 255, 0, 0, 255);
context.putImageData(buffer, -50, 0);
pixelShouldBe(0, 25, [0, 128,0,255]);
context.putImageData(buffer, 100, 0);
pixelShouldBe(99, 25, [0, 128,0,255]);
context.putImageData(buffer, 0, -50);
pixelShouldBe(25, 0, [0, 128,0,255]);
context.putImageData(buffer, 0, 100);
pixelShouldBe(25, 99, [0, 128,0,255]);

// test drawing with non-intersecting dirty rect
context.putImageData(buffer, 50, 0,  50, 0, 100, 100);
context.putImageData(buffer, 50, 0, -50, 0, 50, 100);
context.putImageData(buffer, 50, 0,  0, 50, 100, 100);
context.putImageData(buffer, 50, 0,  50, -50, 100, 100);
for (var x = 0; x < points.length; x++)
    for (var y = 0; y < points.length; y++)
        pixelShouldBe(points[x] + 50, points[y], [0, 128, 0, 255]);

// Test drawing to region intersect edge of canvas
buffer = context.createImageData(100, 100);
fillRect(buffer, 0, 0, 100, 100, 0, 128, 0, 255);
fillRect(buffer, 10, 10, 80, 80, 255, 0, 0, 255);

//left edge
context.putImageData(buffer, -90, 0);
pixelShouldBe(0, 25, [0, 128,0,255]);
pixelShouldBe(0, 50, [0, 128,0,255]);
pixelShouldBe(0, 75, [0, 128,0,255]);
//right edge
context.putImageData(buffer, 90, 0);
pixelShouldBe(99, 25, [0, 128,0,255]);
pixelShouldBe(99, 50, [0, 128,0,255]);
pixelShouldBe(99, 75, [0, 128,0,255]);
//top edge
context.putImageData(buffer, 0, -90);
pixelShouldBe(25, 0, [0, 128,0,255]);
pixelShouldBe(50, 0, [0, 128,0,255]);
pixelShouldBe(75, 0, [0, 128,0,255]);
//bottom edge
context.putImageData(buffer, 0, 90);
pixelShouldBe(25, 99, [0, 128,0,255]);
pixelShouldBe(50, 99, [0, 128,0,255]);
pixelShouldBe(75, 99, [0, 128,0,255]);

// Test drawing with only part of the dirty region intersecting the window
// left edge
context.putImageData(buffer, 0, 0, -90, 0, 100, 100);
pixelShouldBe(0, 25, [0, 128,0,255]);
pixelShouldBe(0, 50, [0, 128,0,255]);
pixelShouldBe(0, 75, [0, 128,0,255]);
pixelShouldBe(10, 25, [0, 128,0,255]);
pixelShouldBe(10, 50, [0, 128,0,255]);
pixelShouldBe(10, 75, [0, 128,0,255]);
//right edge
context.putImageData(buffer, 0, 0, 90, 0, 100, 100);
pixelShouldBe(99, 25, [0, 128,0,255]);
pixelShouldBe(99, 50, [0, 128,0,255]);
pixelShouldBe(99, 75, [0, 128,0,255]);
pixelShouldBe(89, 25, [0, 128,0,255]);
pixelShouldBe(89, 50, [0, 128,0,255]);
pixelShouldBe(89, 75, [0, 128,0,255]);
// top edge
context.putImageData(buffer, 0, 0, 0, -90, 100, 100);
pixelShouldBe(25, 0, [0, 128,0,255]);
pixelShouldBe(50, 0, [0, 128,0,255]);
pixelShouldBe(75, 0, [0, 128,0,255]);
pixelShouldBe(25, 10, [0, 128,0,255]);
pixelShouldBe(50, 10, [0, 128,0,255]);
pixelShouldBe(75, 10, [0, 128,0,255]);
//bottom edge
context.putImageData(buffer, 0, 0, 0, 90, 100, 100);
pixelShouldBe(25, 99, [0, 128,0,255]);
pixelShouldBe(50, 99, [0, 128,0,255]);
pixelShouldBe(75, 99, [0, 128,0,255]);
pixelShouldBe(25, 89, [0, 128,0,255]);
pixelShouldBe(50, 89, [0, 128,0,255]);
pixelShouldBe(75, 89, [0, 128,0,255]);

// Test clamping of dx/dy
var smallbuffer = context.createImageData(10, 10);
fillRect(smallbuffer, 0, 0, 10, 10, 255, 0, 0, 255);
context.putImageData(smallbuffer, 1.5, 1);
pixelShouldBe(11, 11, [0, 128,0,255]);
fillRect(smallbuffer, 0, 0, 10, 10, 0, 128, 0, 255);
context.putImageData(smallbuffer, 1.5, 1);
pixelShouldBe(1, 1, [0, 128,0,255]);

// test clamping of dirtyX/Y/Width/Height
fillRect(smallbuffer, 0, 0, 10, 10, 0, 128, 0, 255);
context.fillStyle = "red";
context.fillRect(1, 1, 9, 9);
context.putImageData(smallbuffer, 1, 1, 0.5, 0.5, 8.5, 8.5);
pixelShouldBe(1, 1, [0, 128,0,255]);
pixelShouldBe(10, 10, [0, 128,0,255]);
context.fillRect(1, 1, 9, 9);
context.putImageData(smallbuffer, 1, 1, 0.25, 0.25, 9, 9);
pixelShouldBe(1, 1, [0, 128,0,255]);
pixelShouldBe(10, 10, [0, 128,0,255]);
context.fillRect(1, 1, 8, 8);
context.putImageData(smallbuffer, 1, 1, 0.0, 0.0, 8.5, 8.5);
pixelShouldBe(1, 1, [0, 128,0,255]);
pixelShouldBe(9, 9, [0, 128,0,255]);
context.fillRect(1, 1, 8, 8);
context.putImageData(smallbuffer, 1, 1, 0.0, 0.0, 8.25, 8.25);
pixelShouldBe(1, 1, [0, 128,0,255]);
pixelShouldBe(9, 9, [0, 128,0,255]);
context.fillRect(1, 1, 7, 7);
context.putImageData(smallbuffer, 1, 1, 0.5, 0.5, 7.9, 7.9);
pixelShouldBe(1, 1, [0, 128,0,255]);
pixelShouldBe(9, 9, [0, 128,0,255]);


shouldThrow("context.putImageData({}, 0, 0)", "'TypeError: Type error'");
shouldThrow("context.putImageData(buffer, NaN, 0, 0, 0, 0, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, NaN, 0, 0, 0, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, 0, NaN, 0, 0, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, 0, 0, NaN, 0, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, 0, 0, 0, NaN, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, 0, 0, 0, 0, NaN)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, Infinity, 0, 0, 0, 0, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, Infinity, 0, 0, 0, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, 0, Infinity, 0, 0, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, 0, 0, Infinity, 0, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, 0, 0, 0, Infinity, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, 0, 0, 0, 0, Infinity)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, undefined, 0, 0, 0, 0, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, undefined, 0, 0, 0, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, 0, undefined, 0, 0, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, 0, 0, undefined, 0, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, 0, 0, 0, undefined, 0)", "'Error: NotSupportedError: DOM Exception 9'");
shouldThrow("context.putImageData(buffer, 0, 0, 0, 0, 0, undefined)", "'Error: NotSupportedError: DOM Exception 9'");

// Ensure we don't mess up bounds clipping checks
var rectcanvas = document.createElement("canvas");
rectcanvas.width = 20;
rectcanvas.height = 10;
var rectbuffer = rectcanvas.getContext("2d");
rectbuffer.putImageData(smallbuffer, 10, 0);

var rectcanvas = document.createElement("canvas");
rectcanvas.width = 10;
rectcanvas.height = 20;
var rectbuffer = rectcanvas.getContext("2d");
rectbuffer.putImageData(smallbuffer, 0, 10);
