description("This test checks ellipse API in canvas v5");

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '400');
canvas.setAttribute('height', '400');
var ctx = canvas.getContext('2d');

ctx.fillStyle="rgba(255, 255, 255, 1.0)";
ctx.fillRect(0, 0, 400, 400);

ctx.strokeStyle="rgba(0, 0, 0, 1.0)";
ctx.lineWidth = 10;
ctx.beginPath();
ctx.moveTo(0, 100);
ctx.ellipse(200, 200, 100, 150, Math.PI / 9, -Math.PI, Math.PI * 5 / 9, false);
ctx.lineTo(0, 300);
ctx.stroke();

var imageData, data;

// Verify the method must add a straight line from the last point in the subpath
// to the start point of the ellipse.
imageData = ctx.getImageData(5, 103, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(49, 130, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(103, 163, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(36, 108, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

// Verify ellipse API draws well.
imageData = ctx.getImageData(101, 179, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(119, 132, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(260, 62, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(301, 122, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(273, 272, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(169, 344, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(196, 362, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

// Verify the last point of ellipse is the start point of the next subpath.
imageData = ctx.getImageData(128, 331, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(65, 315, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(9, 302, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(58, 300, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

