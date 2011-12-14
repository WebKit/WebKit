description("Test for canvas regression where pattern transforms were ignored https://bugs.webkit.org/show_bug.cgi?id=21498");

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

var patternImage = document.createElement("canvas");
patternImage.height = 10;
patternImage.width = 10;
var patternImageCtx = patternImage.getContext('2d');
fillWithColor(patternImageCtx, "green");
var greenPixel = pixelValueAt(patternImageCtx, 0, 0);


var ctx = canvas.getContext('2d');
var pattern = ctx.createPattern(patternImage, "no-repeat");

fillWithColor(ctx, "blue");

ctx.fillStyle = pattern;
ctx.translate(20, 20);
ctx.fillRect(0, 0, 10, 10);
pixelShouldBe(ctx, 20, 20, "greenPixel");

var successfullyParsed = true;
