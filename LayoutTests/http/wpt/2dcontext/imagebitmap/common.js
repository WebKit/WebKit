function testCanvasDisplayingPattern(canvas)
{
    var tolerance = 5; // for creating ImageBitmap from a video, the tolerance needs to be high
    _assertPixelApprox(canvas, 5,5, 255,0,0,255, "5,5", "255,0,0,255", tolerance);
    _assertPixelApprox(canvas, 15,5, 0,255,0,255, "15,5", "0,255,0,255", tolerance);
    _assertPixelApprox(canvas, 5,15, 0,0,255,255, "5,15", "0,0,255,255", tolerance);
    _assertPixelApprox(canvas, 15,15, 0,0,0,255, "15,15", "0,0,0,255", tolerance);
}

function testDrawImageBitmap(source)
{
    var canvas = document.createElement("canvas");
    canvas.width = 20;
    canvas.height = 20;
    var ctx = canvas.getContext("2d");
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    return createImageBitmap(source).then(imageBitmap => {
        ctx.drawImage(imageBitmap, 0, 0);
        testCanvasDisplayingPattern(canvas);
    });
}

function initializeTestCanvas(testCanvas)
{
    testCanvas.width = 20;
    testCanvas.height = 20;
    var testCtx = testCanvas.getContext("2d");
    testCtx.fillStyle = "rgb(255, 0, 0)";
    testCtx.fillRect(0, 0, 10, 10);
    testCtx.fillStyle = "rgb(0, 255, 0)";
    testCtx.fillRect(10, 0, 10, 10);
    testCtx.fillStyle = "rgb(0, 0, 255)";
    testCtx.fillRect(0, 10, 10, 10);
    testCtx.fillStyle = "rgb(0, 0, 0)";
    testCtx.fillRect(10, 10, 10, 10);
}

function initializeImageData(imgData, width, height)
{
    for (var i = 0; i < width * height * 4; i+=4) {
        imgData.data[i] = 0;
        imgData.data[i + 1] = 0;
        imgData.data[i + 2] = 0;
        imgData.data[i + 3] = 255; //alpha channel: 255
    }
    var halfWidth = width/2;
    var halfHeight = height/2;
    // initialize to R, G, B, Black, with each one 10*10 pixels
    for (var i = 0; i < halfHeight; i++)
        for (var j = 0; j < halfWidth; j++)
            imgData.data[i * width * 4 + j * 4] = 255;
    for (var i = 0; i < halfHeight; i++)
        for (var j = halfWidth; j < width; j++)
            imgData.data[i * width * 4 + j * 4 + 1] = 255;
    for (var i = halfHeight; i < height; i++)
        for (var j = 0; j < halfWidth; j++)
            imgData.data[i * width * 4 + j * 4 + 2] = 255;
}

function createCanvasOfSize(width, height)
{
    const c = document.createElement("canvas");
    c.width = width;
    c.height = height;
    return c;
}

function create9x9CanvasWith2dContext()
{
    const c = createCanvasOfSize(9, 9);
    return [c, c.getContext("2d")];
}

function create18x18CanvasWith2dContext()
{
    const c = createCanvasOfSize(18, 18);
    return [c, c.getContext("2d")];
}

function create9x9CanvasWithTargetImage()
{
    const c = createCanvasOfSize(9, 9);
    const ctx = c.getContext("2d");
    ctx.fillStyle = "#00ff00";
    ctx.fillRect(0, 0, 9, 9);
    ctx.fillStyle = "#0000ff";
    ctx.fillRect(4, 4, 1, 1);
    return c;
}
