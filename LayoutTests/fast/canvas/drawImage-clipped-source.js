description("Series of tests to ensure correct behaviour of drawImage with partially out of bounds source rectangles");
var canvas2 = document.createElement('canvas');
canvas2.width = 30;
canvas2.height = 30;
var ctx2 = canvas2.getContext('2d');

function patternTest(canvas) {

    this.testPixel = function(x, y, color) {
        var imageData = this.canvas.getImageData(this.xOffset + x * this.scale, this.yOffset + y * this.scale, 1, 1);
        imgdata = imageData.data;
        shouldBe("imgdata[0]", color[0].toString());
        shouldBe("imgdata[1]", color[1].toString());
        shouldBe("imgdata[2]", color[2].toString());
    }

    this.testRedSquare = function(x, y, w, h) {
        var red = new Array(255, 0, 0);
        debug("-- red square, x = " + x + ", y = " + y + ", w = " + w + ", h = " + h);
        this.testPixel(x, y, red);
        this.testPixel(x + w - 1, y + h - 1, red);
    }

    this.testPattern = function(x, y, filterPadding) {
        var yellow = new Array(255, 255, 0);
        var cyan = new Array(0, 255, 255);
        var magenta = new Array(255, 0, 255);
        var green = new Array(0, 255, 0);
        var blue = new Array(0, 0, 255);
        debug("-- test pattern, x = " + x + ", y = " + y);
        this.testPixel(x + 9, y + 9, yellow);
        this.testPixel(x + 20 + filterPadding, y + 9, cyan);
        this.testPixel(x + 9, y + 20 + filterPadding, magenta);
        this.testPixel(x + 20 + filterPadding, y + 20 + filterPadding, green);
        this.testPixel(x + 10 + filterPadding, y + 10 + filterPadding, blue);
        this.testPixel(x + 19, y + 19, blue); 
    }

    this.testAggregatePattern = function(x, y, scale, filterPadding){
        this.scale = scale;
        this.xOffset = x;
        this.yOffset = y;
        debug("- clipped by left edge");
        this.testRedSquare(0, 0, 30, 30);
        this.testPattern(30, 0, filterPadding);
        debug("- clipped by right edge");
        this.testRedSquare(90, 0, 30, 30);
        this.testPattern(60, 0, filterPadding);
        debug("- clipped by top edge");
        this.testRedSquare(0, 30, 30, 30);
        this.testPattern(0, 60, filterPadding);
        debug("- clipped by bottom edge");
        this.testRedSquare(30, 60, 30, 30);
        this.testPattern(30, 30, filterPadding);
        debug("- clipped by all edges");
        this.testRedSquare(60, 30, 60, 60);
        this.testRedSquare(74, 44, 32, 32);
        this.testPattern(75, 45, filterPadding);
    }

    this.canvas = canvas;
}

var drawTestPattern = function(canvas, srcImage) {
    canvas.drawImage(srcImage, -30, 0, 60, 30, 0, 0, 60, 30); // left edge clip
    canvas.drawImage(srcImage, 0, 0, 60, 30, 60, 0, 60, 30); // right edge clip
    canvas.drawImage(srcImage, 0, -30, 30, 60, 0, 30, 30, 60); // top edge clip
    canvas.drawImage(srcImage, 0, 0, 30, 60, 30, 30, 30, 60); // bottom edge clip
    canvas.drawImage(srcImage, -15, -15, 60, 60, 60, 30, 60, 60); // all edge clip
    // repeat with negative source dimensions
    canvas.drawImage(srcImage, 30, 30, -60, -30, 0, 90, 60, 30); // left edge clip
    canvas.drawImage(srcImage, 60, 30, -60, -30, 60, 90, 60, 30); // right edge clip
    canvas.drawImage(srcImage, 30, 30, -30, -60, 0, 120, 30, 60); // top edge clip
    canvas.drawImage(srcImage, 30, 60, -30, -60, 30, 120, 30, 60); // bottom edge clip
    canvas.drawImage(srcImage, 45, 45, -60, -60, 60, 120, 60, 60); // all edge clip
}

var executeTest = function() {
    var canvas = document.getElementById('canvas').getContext('2d');
    canvas.fillStyle = '#f00';
    canvas.fillRect(0, 0, 240, 540);

    try {
        canvas.save();
        drawTestPattern(canvas, canvas2);
        canvas.translate(120, 0);
        drawTestPattern(canvas, imageSource);
        canvas.restore();
        canvas.scale(2, 2);
        canvas.translate(0, 90);
        drawTestPattern(canvas, imageSource);
    } catch(err) {
        testFailed("Unexpected Exception: " + err.message);
    }

    var test = new patternTest(canvas);
    debug("Canvas to canvas draw")
    test.testAggregatePattern(0, 0, 1, 0);
    debug("Canvas to canvas draw with negative source dimensions");
    test.testAggregatePattern(0, 90, 1, 0);
    debug("Image to canvas draw");
    test.testAggregatePattern(120, 0, 1, 0);
    debug("Image to canvas draw with negative source dimensions");
    test.testAggregatePattern(120, 90, 1, 0);
    debug("Image to canvas draw with 2x scale");
    test.testAggregatePattern(0, 180, 2, 1);
    debug("Image to canvas draw with negative source dimensions and 2x scale");
    test.testAggregatePattern(0, 360, 2, 1);
    finishJSTest();
}

// The following test pattern is designed to make it easy to verify
// scale, position and orientation
ctx2.fillStyle = '#ff0';
ctx2.fillRect(0, 0, 15, 15);
ctx2.fillStyle = '#f0f';
ctx2.fillRect(0, 15, 15, 15);
ctx2.fillStyle = '#0ff';
ctx2.fillRect(15, 0, 15, 15);
ctx2.fillStyle = '#0f0';
ctx2.fillRect(15, 15, 15, 15);
ctx2.fillStyle = '#00f';
ctx2.fillRect(10, 10, 10, 10);

var imageSource = new Image();
imageSource.onload = executeTest;
imageSource.src = canvas2.toDataURL();

jsTestIsAsync = true;

var successfullyParsed = true;
