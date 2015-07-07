description("Ensure correct behavior of canvas with createPattern + fillRect with shadow.");

function print(message, color)
{
    var paragraph = document.createElement("div");
    paragraph.appendChild(document.createTextNode(message));
    paragraph.style.fontFamily = "monospace";
    if (color)
        paragraph.style.color = color;
    document.getElementById("console").appendChild(paragraph);
}

function shouldBeAround(a, b)
 {
     var evalA;
     try {
         evalA = eval(a);
     } catch(e) {
         evalA = e;
     }

     if (Math.abs(evalA - b) < 10)
         print("PASS " + a + " is around " + b , "green")
     else
         print("FAIL " + a + " is not around " + b + " (actual: " + evalA + ")", "red");
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
aCtx.fillStyle = 'blue';
aCtx.arc(100, 100, 100, 0, Math.PI*2, false);
aCtx.fill();

// Create the image object to be drawn on the master canvas.
var img = new Image();
img.onload = drawImageToCanvasAndCheckPixels;
img.src = aCanvas.toDataURL(); // set a data URI of the base64 enconded image as the source

// Create master canvas.
var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '700');
canvas.setAttribute('height', '1100');
var ctx = canvas.getContext('2d');

function drawImageToCanvasAndCheckPixels() {
    ctx.shadowOffsetX = 250;
    ctx.fillStyle = ctx.createPattern(img, 'repeat');

    ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
    ctx.fillRect(50, 50, 200, 200);

    ctx.shadowColor = 'rgba(255, 0, 0, 0.2)';
    ctx.fillRect(50, 300, 200, 200);

    ctx.shadowBlur = 10;
    ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
    ctx.fillRect(50, 550, 200, 200);

    ctx.shadowColor = 'rgba(255, 0, 0, 0.2)';
    ctx.fillRect(50, 800, 200, 200);

    checkPixels();
}

function checkPixels() {
    var imageData, data;

    // Verify solid shadow.
    imageData = ctx.getImageData(300, 50, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBe('d[3]', '255');

    imageData = ctx.getImageData(300, 249, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBe('d[3]', '255');

    imageData = ctx.getImageData(490, 240, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBe('d[3]', '255');

    // Verify solid alpha shadow.
    imageData = ctx.getImageData(310, 350, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '51');

    imageData = ctx.getImageData(490, 490, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '51');

    imageData = ctx.getImageData(300, 499, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '51');

    // Verify blurry shadow.
    imageData = ctx.getImageData(310, 550, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '141');

    imageData = ctx.getImageData(490, 750, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '113');

    imageData = ctx.getImageData(499, 499, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '51');

    // Verify blurry alpha shadow.
    imageData = ctx.getImageData(300, 850, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '29');

    imageData = ctx.getImageData(500, 875, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '23');

    imageData = ctx.getImageData(300, 900, 1, 1);
    d = imageData.data;
    shouldBe('d[0]', '255');
    shouldBe('d[1]', '0');
    shouldBe('d[2]', '0');
    shouldBeAround('d[3]', '29');
}
