description("Ensure correct behavior when drawing a canvas on a canvas with shadows. A blue and red checkered pattern should be displayed.");

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

    if (Math.abs(evalA - b) < 15)
        print("PASS " + a + " is around " + b , "green")
    else
        print("FAIL " + a + " is not around " + b + " (actual: " + evalA + ")", "red");
}

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '600');
canvas.setAttribute('height', '600');
var ctx = canvas.getContext('2d');
ctx.shadowOffsetX = 100;
ctx.shadowOffsetY = 100;

var aCanvas = document.createElement('canvas');
aCanvas.width = 300;
aCanvas.height = 300;

var aCtx = aCanvas.getContext('2d');
aCtx.fillStyle = 'rgba(0, 0, 255, 1.0)';
aCtx.fillRect(100, 100, 100, 100);

ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
ctx.drawImage(aCanvas, 0, 0);

ctx.shadowColor = 'rgba(255, 0, 0, 0.5)';
ctx.drawImage(aCanvas, 0, 200);

ctx.shadowBlur = 5;
ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
ctx.drawImage(aCanvas, 200, 0);

ctx.shadowColor = 'rgba(255, 0, 0, 0.5)';
ctx.drawImage(aCanvas, 200, 200);

var d; // imageData.data

// Verify solid shadow.
d = ctx.getImageData(200, 205, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '255');

d = ctx.getImageData(299, 295, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '255');

d = ctx.getImageData(200, 299, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '255');

// Verify solid alpha shadow.
d = ctx.getImageData(200, 405, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '127');

d = ctx.getImageData(299, 405, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '127');

d = ctx.getImageData(205, 499, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '127');

// Verify blurry shadow.
d = ctx.getImageData(500, 211, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '100');

d = ctx.getImageData(399, 205, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '100');

d = ctx.getImageData(450, 300, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '100');

// Verify blurry alpha shadow.
d = ctx.getImageData(500, 411, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '50');

d = ctx.getImageData(399, 405, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '50');

d = ctx.getImageData(450, 500, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '50');
