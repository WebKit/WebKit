description("Test for canvas regression where gradient clips were not cleared https://bugs.webkit.org/show_bug.cgi?id=21498")

function pixelValueAt(context, x, y) {
    var imageData = context.getImageData(x, y, 1, 1);
    return imageData.data;
}

function pixelToString(p) {
    return "[" + p[0] + ", " + p[1] + ", " + p[2] + ", " + p[3] + "]"
}

function pixelShouldBe(context, x, y, expectedPixelString) {
    var pixel = pixelValueAt(context, x, y);
    var expectedPixel = eval(expectedPixelString);
    
    var pixelString = "pixel " + x + ", " + y;
    if (areArraysEqual(pixel, expectedPixel)) {
        testPassed(pixelString + " is " + pixelToString(pixel));
    } else {
        testFailed(pixelString + " should be " + pixelToString(expectedPixel) + " was " + pixelToString(pixel));
    }
}

function fillWithColor(context, color) {
    context.save();
    context.fillStyle = color;
    context.fillRect(0, 0, canvas.width, canvas.height);
    context.restore();
}

var canvas = document.createElement("canvas");
canvas.height = 100;
canvas.width = 100;
canvas.style.height = "100";
canvas.style.width = "100";

document.body.appendChild(canvas);

var greenImage = document.createElement("canvas");
greenImage.height = 10;
greenImage.width = 10;
var greenCtx = greenImage.getContext('2d');
fillWithColor(greenCtx, "green");
var greenPixel = pixelValueAt(greenCtx, 0, 0);


var ctx = canvas.getContext('2d');
var gradient = ctx.createLinearGradient(0, 0, 10, 0);
gradient.addColorStop(0, "blue");
gradient.addColorStop(1, "red");
ctx.fillStyle = gradient;
ctx.beginPath();
ctx.moveTo(0, 0);
ctx.lineTo(10, 5);
ctx.lineTo(10, 10);
ctx.lineTo(5, 10);
ctx.closePath();
ctx.fill();

ctx.fillStyle = "green";
ctx.fillRect(20, 20, 10, 10);

pixelShouldBe(ctx, 20, 20, "greenPixel");

var successfullyParsed = true;
