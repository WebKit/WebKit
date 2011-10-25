description("Ensure correct behavior of canvas with image shadow. A square with a cut-out top-right corner should be displayed with solid shadow (top) and blur shadow (bottom).");

function print(message, color)
{
    var paragraph = document.createElement("div");
    paragraph.appendChild(document.createTextNode(message));
    paragraph.style.fontFamily = "monospace";
    if (color)
        paragraph.style.color = color;
    document.getElementById("console").appendChild(paragraph);
}

function shouldNotBe(a, b)
{
    var evalA;
    try {
        evalA = eval(a);
    } catch(e) {
        evalA = e;
    }

    if (evalA != b)
        print("PASS " + a + " should not be " + b + " and it's not.", "green")
    else
        print("FAIL " + a + " should not be " + b + " but it is.", "red");
}

// Create auxiliary canvas to draw to and create an image from.
// This is done instead of simply loading an image from the file system
// because that would throw a SECURITY_ERR DOM Exception.
var aCanvas = document.createElement('canvas');
aCanvas.setAttribute('width', '200');
aCanvas.setAttribute('height', '200');
var aCtx = aCanvas.getContext('2d');

// Draw a circle on the same canvas.
aCtx.beginPath();
aCtx.fillStyle = 'green';
aCtx.arc(100, 100, 150, 0, -Math.PI/2, false);
aCtx.fill();

// Create the image object to be drawn on the master canvas.
var img = new Image();
img.onload = drawImageToCanvasAndCheckPixels;
img.src = aCanvas.toDataURL(); // set a data URI of the base64 enconded image as the source

// Create master canvas.
var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '600');
canvas.setAttribute('height', '600');
var ctx = canvas.getContext('2d');

function drawImageToCanvasAndCheckPixels() {
    ctx.shadowOffsetX = 250;
    ctx.shadowColor = 'rgba(240, 50, 50, 1.0)';
    ctx.drawImage(img, 50, 50);

    ctx.shadowOffsetX = 250;
    ctx.shadowBlur = 6;
    ctx.shadowColor = 'rgba(50, 50, 200, 0.9)';
    ctx.shadowColor = 'rgba(0, 0, 255, 1.0)';
    ctx.drawImage(img, 50, 300);

    checkPixels();
}

function checkPixels() {
    var imageData, data;

    // Verify solid shadow.
    imageData = ctx.getImageData(260, 300, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '0');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBe('d[3]', '0');

    imageData = ctx.getImageData(350, 100, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '240');
    shouldBe('d[1]', '50');
    shouldBe('d[2]', '50');
    shouldBe('d[3]', '255');

    imageData = ctx.getImageData(400, 200, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '240');
    shouldBe('d[1]', '50');
    shouldBe('d[2]', '50');
    shouldBe('d[3]', '255');

    imageData = ctx.getImageData(490, 65, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '0');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBe('d[3]', '0');

    imageData = ctx.getImageData(485, 65, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '0');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBe('d[3]', '0');

    // Verify blurry shadow.
    imageData = ctx.getImageData(260, 400, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '0');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBe('d[3]', '0');

    imageData = ctx.getImageData(350, 300, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '0');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '255');
    shouldNotBe('d[3]', '255');

    imageData = ctx.getImageData(300, 400, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '0');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '255');
    shouldNotBe('d[3]', '255');

    imageData = ctx.getImageData(300, 500, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '0');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '255');
    shouldNotBe('d[3]', '255');

    imageData = ctx.getImageData(400, 500, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '0');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '255');
    shouldNotBe('d[3]', '255');

    imageData = ctx.getImageData(400, 400, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '0');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '255');

    imageData = ctx.getImageData(490, 315, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '0');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBe('d[3]', '0');

    imageData = ctx.getImageData(485, 320, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '0');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBe('d[3]', '0');
}
