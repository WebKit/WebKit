description("Ensure correct behavior of canvas with path fill shadow");

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

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '700');
canvas.setAttribute('height', '700');
var ctx = canvas.getContext('2d');

ctx.beginPath();
ctx.moveTo(300, 300);
ctx.lineTo(300, 50);
ctx.bezierCurveTo(200, 40, 75, 150, 30, 30);
ctx.quadraticCurveTo(250, 75, 50, 300);
ctx.shadowOffsetX = 350;
ctx.shadowColor = 'rgba(255, 20, 0, 0.5)';
ctx.shadowBlur = 0;
ctx.fillStyle = 'rgba(0, 0, 255, 1)';
ctx.lineWidth = 30;
ctx.fill();

ctx.beginPath();
ctx.moveTo(300,650);
ctx.lineTo(300,400);
ctx.bezierCurveTo(200, 390, 75, 500, 30, 380);
ctx.quadraticCurveTo(250, 425, 50, 650);
ctx.shadowOffsetX = 350;
ctx.shadowColor = 'rgba(255, 0, 0, 0.5)';
ctx.shadowBlur = 30;
ctx.fillStyle = 'rgba(0, 0, 255, 1)';
ctx.lineWidth = 30;
ctx.fill();

var imageData, data;

// Verify solid shadow.
imageData = ctx.getImageData(640, 290, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');
shouldBe('data[1]', '20');
shouldBe('data[2]', '0');

imageData = ctx.getImageData(570, 85, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');
shouldBe('data[1]', '20');
shouldBe('data[2]', '0');

imageData = ctx.getImageData(380, 30, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');
shouldBe('data[1]', '20');
shouldBe('data[2]', '0');

imageData = ctx.getImageData(400, 40, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');
shouldBe('data[1]', '20');
shouldBe('data[2]', '0');

// Verify blurry shadow.
imageData = ctx.getImageData(640, 640, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');
shouldBe('data[1]', '0');
shouldBe('data[2]', '0');
shouldNotBe('data[3]', '255');

imageData = ctx.getImageData(650, 400, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');
shouldBe('data[1]', '0');
shouldBe('data[2]', '0');
shouldNotBe('data[3]', '255');

imageData = ctx.getImageData(380, 380, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');
shouldBe('data[1]', '0');
shouldBe('data[2]', '0');
shouldNotBe('data[3]', '255');

imageData = ctx.getImageData(375, 390, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');
shouldBe('data[1]', '0');
shouldBe('data[2]', '0');
shouldNotBe('data[3]', '255');
