description("Ensure correct behavior of canvas with fillRect using a gradient fillStyle and a shadow");

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

    if (Math.abs(evalA - b) < 5)
        print("PASS " + a + " is around " + b , "green")
    else
        print("FAIL " + a + " is not around " + b + " (actual: " + evalA + ")", "red");
}

function shouldBeSmaller(a, b)
{
    var evalA;
    try {
        evalA = eval(a);
    } catch(e) {
        evalA = e;
    }

    if (evalA < b)
        print("PASS " + a + " is smaller than " + b , "green")
    else
        print("FAIL " + a + " is not smaller than " + b + " (actual: " + evalA + ")", "red");
}

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '400');
canvas.setAttribute('height', '800');
var ctx = canvas.getContext('2d');

var gradient = ctx.createLinearGradient(0, 0, 100, 100);
gradient.addColorStop(0, 'rgba(0, 0, 255, 1.0)');
gradient.addColorStop(1, 'rgba(0, 0, 255, 1.0)');

ctx.shadowOffsetX = 200;
ctx.fillStyle = gradient;

ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
ctx.fillRect(50, 50, 100, 100);

ctx.shadowColor = 'rgba(255, 0, 0, 0.3)';
ctx.fillRect(50, 200, 100, 100);

ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
ctx.shadowBlur = 5;
ctx.fillRect(50, 350, 100, 100);

ctx.shadowColor = 'rgba(255, 0, 0, 0.3)';
ctx.fillRect(50, 500, 100, 100);

ctx.rotate(Math.PI/2);
ctx.fillRect(650, -150, 100, 100);

var d; // imageData.data

// Verify solid shadow.
d = ctx.getImageData(250, 50, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '255');

d = ctx.getImageData(250, 149, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '255');

d = ctx.getImageData(349, 50, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '255');

d = ctx.getImageData(349, 149, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '255');

// Verify solid alpha shadow.
d = ctx.getImageData(250, 200, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '76');

d = ctx.getImageData(250, 299, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '76');

d = ctx.getImageData(349, 200, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '76');

d = ctx.getImageData(349, 299, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '76');

// Verify blurry shadow.
d = ctx.getImageData(248, 348, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeSmaller('d[3]', '25');

d = ctx.getImageData(248, 451, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeSmaller('d[3]', '25');

d = ctx.getImageData(351, 348, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeSmaller('d[3]', '25');

d = ctx.getImageData(351, 451, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeSmaller('d[3]', '25');

// Verify blurry alpha shadow.
d = ctx.getImageData(248, 498, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeSmaller('d[3]', '10');

d = ctx.getImageData(248, 601, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeSmaller('d[3]', '10');

d = ctx.getImageData(351, 498, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeSmaller('d[3]', '10');

d = ctx.getImageData(351, 601, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeSmaller('d[3]', '10');

// Verify blurry alpha shadow with rotation.
d = ctx.getImageData(249, 649, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeSmaller('d[3]', '15');

d = ctx.getImageData(248, 751, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeSmaller('d[3]', '15');

d = ctx.getImageData(350, 649, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeSmaller('d[3]', '15');

d = ctx.getImageData(350, 750, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeSmaller('d[3]', '15');
