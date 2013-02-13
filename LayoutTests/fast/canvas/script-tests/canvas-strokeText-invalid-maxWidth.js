descriptionQuiet("Series of tests to ensure that strokeText() does not display any text when maxWidth is invalid.");

var canvas = document.createElement('canvas');
var ctx = canvas.getContext('2d');
var canvasWidth = 100;
var canvasHeight = 50;
canvas.setWidth = canvasWidth;
canvas.setHeight = canvasHeight;


ctx.fillStyle = '#0f0';
ctx.fillRect(0, 0, canvasWidth, canvasHeight);
ctx.font = '35px Arial, sans-serif';

debug("Test canvas.strokeText() with maxWidth zero");
ctx.strokeStyle = '#f00';
ctx.strokeText("fail fail fail fail fail", 5, 35, 0);

var imageData = ctx.getImageData(0, 0, canvasWidth, canvasHeight);
var w = imageData.width, h = imageData.height, d = imageData.data;
for (var i = 0; i < h; ++i) {
    for (var j = 0; j < w; ++j) {
        if (d[4 * (w * i + j) + 0] != 0) shouldBe("d[4 * (w * i + j) + 0]", "0"); 
        if (d[4 * (w * i + j) + 1] != 255) shouldBe("d[4 * (w * i + j) + 1]", "255"); 
        if (d[4 * (w * i + j) + 2] != 0) shouldBe("d[4 * (w * i + j) + 2]", "0"); 
        if (d[4 * (w * i + j) + 3] != 255) shouldBe("d[4 * (w * i + j) + 3]", "255");
    }
}

ctx.fillStyle = '#0f0';
ctx.fillRect(0, 0, canvasWidth, canvasHeight);
debug("Test canvas.strokeText() with maxWidth -1");
ctx.strokeStyle = '#f00';
ctx.strokeText("fail fail fail fail fail", 5, 35, -1);

var imageData = ctx.getImageData(0, 0, canvasWidth, canvasHeight);
var w = imageData.width, h = imageData.height, d = imageData.data;
for (var i = 0; i < h; ++i) {
    for (var j = 0; j < w; ++j) {
        if (d[4 * (w * i + j) + 0] != 0) shouldBe("d[4 * (w * i + j) + 0]", "0"); 
        if (d[4 * (w * i + j) + 1] != 255) shouldBe("d[4 * (w * i + j) + 1]", "255"); 
        if (d[4 * (w * i + j) + 2] != 0) shouldBe("d[4 * (w * i + j) + 2]", "0"); 
        if (d[4 * (w * i + j) + 3] != 255) shouldBe("d[4 * (w * i + j) + 3]", "255");
    }
}

