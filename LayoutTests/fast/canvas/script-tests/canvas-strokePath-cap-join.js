description("Ensure correct behavior of canvas with path stroke with cap and join");

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '700');
canvas.setAttribute('height', '200');
var ctx = canvas.getContext('2d');

ctx.miterLimit = 5;
ctx.lineWidth = 15;

ctx.fillStyle="rgba(255, 255, 255, 1.0)";
ctx.fillRect(0, 0, 700, 200);

ctx.strokeStyle="rgba(0, 0, 0, 1.0)";
ctx.lineJoin = "miter";
ctx.lineCap = "round";

ctx.translate(0, 50);
ctx.save();

ctx.beginPath();
ctx.moveTo(10, 100);
ctx.lineTo(30, 2);
ctx.lineTo(50, 100);
ctx.stroke();

ctx.translate(60 ,0);
ctx.beginPath();
ctx.moveTo(10, 100);
ctx.lineTo(30, 3);
ctx.lineTo(50, 100);
ctx.stroke();

ctx.translate(90 ,0);
ctx.save();
ctx.rotate(0.2);
ctx.beginPath();
ctx.moveTo(10, 100);
ctx.lineTo(30, 3);
ctx.lineTo(50, 100);
ctx.closePath();
ctx.stroke();
ctx.restore();

ctx.restore();

ctx.lineJoin = "bevel";
ctx.lineCap = "square";

ctx.translate(200, 0);
ctx.save();

ctx.beginPath();
ctx.moveTo(10, 100);
ctx.lineTo(30, 2);
ctx.lineTo(50, 100);
ctx.stroke();

ctx.translate(60 ,0);
ctx.beginPath();
ctx.moveTo(10, 100);
ctx.lineTo(30, 3);
ctx.lineTo(50, 100);
ctx.stroke();

ctx.translate(90 ,0);
ctx.save();
ctx.rotate(0.2);
ctx.beginPath();
ctx.moveTo(10, 100);
ctx.lineTo(30, 3);
ctx.lineTo(50, 100);
ctx.closePath();
ctx.stroke();
ctx.restore();

ctx.restore();

ctx.lineJoin = "round";
ctx.lineCap = "butt";

ctx.translate(200, 0);
ctx.save();

ctx.beginPath();
ctx.moveTo(10, 100);
ctx.lineTo(30, 2);
ctx.lineTo(50, 100);
ctx.stroke();

ctx.translate(60 ,0);
ctx.beginPath();
ctx.moveTo(10, 100);
ctx.lineTo(30, 3);
ctx.lineTo(50, 100);
ctx.stroke();

ctx.translate(90 ,0);
ctx.save();
ctx.rotate(0.2);
ctx.beginPath();
ctx.moveTo(10, 100);
ctx.lineTo(30, 3);
ctx.lineTo(50, 100);
ctx.closePath();
ctx.stroke();
ctx.restore();

ctx.restore();

var imageData, data;

// Verify Join : miter, Cap : round.
imageData = ctx.getImageData(30, 51, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(30, 49, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(14, 154, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(14, 157, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(89, 22, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(89, 12, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(184, 29, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(180, 27, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(132, 152, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(130, 157, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

// Verify Join : bevel, Cap : square.
imageData = ctx.getImageData(202, 154, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(201, 150, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(228, 52, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(225, 48, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(316, 154, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(316, 157, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(289, 52, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(289, 48, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(372, 58, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(373, 54, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(380, 159, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(383, 162, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

// Verify Join : round, Cap : butt.
imageData = ctx.getImageData(405, 147, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(405, 151, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(429, 46, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(429, 43, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(464, 146, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(464, 150, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(489, 46, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(489, 43, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(534, 151, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(531, 153, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(579, 52, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(579, 48, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

