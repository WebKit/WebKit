description("Ensure correct behavior of canvas with fillRect+shadow after scaling. A blue and red checkered pattern should be displayed.");

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

    if (Math.abs(evalA - b) < 20)
        print("PASS " + a + " is around " + b , "green")
    else
        print("FAIL " + a + " is not around " + b + " (actual: " + evalA + ")", "red");
}

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '1000');
canvas.setAttribute('height', '1000');
var ctx = canvas.getContext('2d');

ctx.scale(2, 2);
ctx.shadowOffsetX = 100;
ctx.shadowOffsetY = 100;
ctx.fillStyle = 'rgba(0, 0, 255, 1)';

ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
ctx.fillRect(50, 50, 50, 50);

ctx.shadowColor = 'rgba(255, 0, 0, 0.3)';
ctx.fillRect(50, 150, 50, 50);

ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
ctx.shadowBlur = 10;
ctx.fillRect(150, 50, 50, 50);

ctx.shadowColor = 'rgba(255, 0, 0, 0.3)';
ctx.fillRect(150, 150, 50, 50);

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
d = ctx.getImageData(201, 405, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '76');

d = ctx.getImageData(298, 405, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '76');

d = ctx.getImageData(205, 498, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '76');

// Verify blurry shadow.
d = ctx.getImageData(398, 205, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '83');

d = ctx.getImageData(501, 205, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '83');

d = ctx.getImageData(500, 300, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '53');

// Verify blurry alpha shadow.
d = ctx.getImageData(398, 405, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '24');

d = ctx.getImageData(405, 501, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '24');

d = ctx.getImageData(405, 501, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '24');
