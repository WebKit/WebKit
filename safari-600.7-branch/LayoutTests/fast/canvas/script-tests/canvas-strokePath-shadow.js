description("Ensure correct behavior of canvas with path stroke shadow");

function print(message, color)
{
    var paragraph = document.createElement("div");
    paragraph.appendChild(document.createTextNode(message));
    paragraph.style.fontFamily = "monospace";
    if (color)
        paragraph.style.color = color;
    document.getElementById("console").appendChild(paragraph);
}

// Level of tolerance we expect of most pixel comparisons in this test.
function shouldBeAlmost(_a, _b)
{
    shouldBeCloseTo(_a, _b, 2);
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
ctx.strokeStyle = 'rgba(0, 0, 255, 1)';
ctx.lineWidth = 30;
ctx.closePath();
ctx.stroke();

ctx.beginPath();
ctx.moveTo(300,650);
ctx.lineTo(300,400);
ctx.bezierCurveTo(200, 390, 75, 500, 30, 380);
ctx.quadraticCurveTo(250, 425, 50, 650);
ctx.shadowOffsetX = 350;
ctx.shadowColor = 'rgba(255, 0, 0, 0.5)';
ctx.shadowBlur = 30;
ctx.strokeStyle = 'rgba(0, 0, 255, 1)';
ctx.lineWidth = 30;
ctx.closePath();
ctx.stroke();

var imageData, data;

// Verify solid shadow.
imageData = ctx.getImageData(650, 300, 1, 1);
data = imageData.data;
shouldBeAlmost('data[0]', 255);
shouldBeAlmost('data[1]', 20);
shouldBeAlmost('data[2]', 0);

imageData = ctx.getImageData(650, 50, 1, 1);
data = imageData.data;
shouldBeAlmost('data[0]', 255);
shouldBeAlmost('data[1]', 20);
shouldBeAlmost('data[2]', 0);

imageData = ctx.getImageData(380, 30, 1, 1);
data = imageData.data;
shouldBeAlmost('data[0]', 255);
shouldBeAlmost('data[1]', 20);
shouldBeAlmost('data[2]', 0);

imageData = ctx.getImageData(400, 40, 1, 1);
data = imageData.data;
shouldBeAlmost('data[0]', 255);
shouldBeAlmost('data[1]', 20);
shouldBeAlmost('data[2]', 0);

// Verify blurry shadow.
imageData = ctx.getImageData(640, 640, 1, 1);
data = imageData.data;
shouldBeAlmost('data[0]', 255);
shouldBeAlmost('data[1]', 0);
shouldBeAlmost('data[2]', 0);
shouldNotBe('data[3]', '255');

imageData = ctx.getImageData(650, 400, 1, 1);
data = imageData.data;
shouldBeAlmost('data[0]', 255);
shouldBeAlmost('data[1]', 0);
shouldBeAlmost('data[2]', 0);
shouldNotBe('data[3]', '255');

imageData = ctx.getImageData(380, 380, 1, 1);
data = imageData.data;
shouldBeAlmost('data[0]', 255);
shouldBeAlmost('data[1]', 0);
shouldBeAlmost('data[2]', 0);
shouldNotBe('data[3]', '255');

imageData = ctx.getImageData(350, 380, 1, 1);
data = imageData.data;
shouldBeAlmost('data[0]', 255);
shouldBeAlmost('data[1]', 0);
shouldBeAlmost('data[2]', 0);
shouldNotBe('data[3]', '255');
