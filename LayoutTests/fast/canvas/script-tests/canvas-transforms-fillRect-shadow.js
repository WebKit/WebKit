description("Ensure correct behavior of canvas with fillRect+shadow after translation+rotation+scaling. A blue and red checkered pattern should be displayed.");

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

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '600');
canvas.setAttribute('height', '600');
var ctx = canvas.getContext('2d');

ctx.fillStyle = 'rgba(0, 0, 255, 1.0)';
ctx.shadowOffsetX = 100;
ctx.shadowOffsetY = 100;

ctx.translate(-100, -100);
ctx.rotate(Math.PI/2);
ctx.scale(2, 2);

ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
ctx.fillRect(100, -150, 50, 50);

ctx.shadowColor = 'rgba(255, 0, 0, 0.5)';
ctx.fillRect(200, -150, 50, 50);

ctx.shadowBlur = 5;
ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
ctx.fillRect(100, -250, 50, 50);

ctx.shadowColor = 'rgba(255, 0, 0, 0.5)';
ctx.fillRect(200, -250, 50, 50);

var d; // imageData.data

// Verify solid shadow.
d = ctx.getImageData(201, 205, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '255');

d = ctx.getImageData(298, 298, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '255');

d = ctx.getImageData(201, 298, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '255');

// Verify solid alpha shadow.
d = ctx.getImageData(201, 401, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '127');

d = ctx.getImageData(298, 450, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '127');

d = ctx.getImageData(205, 498, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '127');

// Verify blurry shadow.
d = ctx.getImageData(399, 205, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '106');

d = ctx.getImageData(500, 205, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '106');

d = ctx.getImageData(499, 299, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '83');

// Verify blurry alpha shadow.
d = ctx.getImageData(398, 405, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '36');

d = ctx.getImageData(405, 501, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '36');

d = ctx.getImageData(405, 501, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '36');

var successfullyParsed = true;
