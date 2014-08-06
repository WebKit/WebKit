description("Ensure correct behavior of canvas with stroke rect with line join");

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '600');
canvas.setAttribute('height', '400');
var ctx = canvas.getContext('2d');

ctx.fillStyle="rgba(255, 255, 255, 1.0)";
ctx.fillRect(0, 0, canvas.width, canvas.height);

ctx.strokeStyle="rgba(0, 0, 0, 1.0)";

ctx.lineWidth = 20;

ctx.save();

ctx.lineJoin = "miter";
ctx.translate(20, 20);
ctx.strokeRect(0, 0, 100, 100);

ctx.lineJoin = "bevel";
ctx.translate(200, 0);
ctx.strokeRect(0, 0, 100, 100);

ctx.lineJoin = "round";
ctx.translate(200, 0);
ctx.strokeRect(0, 0, 100, 100);

ctx.restore();

ctx.translate(0, 200);

ctx.scale(20, 20);

ctx.lineWidth = 1.0;

ctx.lineJoin = "miter";
ctx.translate(1, 1);
ctx.strokeRect(0, 0, 5, 5);

ctx.lineJoin = "bevel";
ctx.translate(10, 0);
ctx.strokeRect(0, 0, 5, 5);

ctx.lineJoin = "round";
ctx.translate(10, 0);
ctx.strokeRect(0, 0, 5, 5);

var imageData, data;

// Verify Join : miter.
imageData = ctx.getImageData(10, 10, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(10, 17, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(17, 10, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(20, 20, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(30, 30, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(110, 110, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(129, 129, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

// Verify Join : bevel.
imageData = ctx.getImageData(210, 10, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(210, 17, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(217, 10, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(220, 20, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(230, 30, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(310, 110, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(329, 129, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

// Verify Join : round.
imageData = ctx.getImageData(410, 10, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(410, 17, 1, 1);
data = imageData.data;
shouldNotBe('data[0]', '255');

imageData = ctx.getImageData(417, 10, 1, 1);
data = imageData.data;
shouldNotBe('data[0]', '255');

imageData = ctx.getImageData(420, 20, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(430, 30, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(510, 110, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(529, 129, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

// Verify Join : miter using scaling.
imageData = ctx.getImageData(10, 210, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(10, 217, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(17, 210, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(20, 220, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(30, 230, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(110, 310, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(129, 329, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

// Verify Join : bevel using scaling.
imageData = ctx.getImageData(210, 210, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(210, 217, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(217, 210, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(220, 220, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(230, 230, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(310, 310, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(329, 329, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

// Verify Join : round using scaling.
imageData = ctx.getImageData(410, 210, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(410, 217, 1, 1);
data = imageData.data;
shouldNotBe('data[0]', '255');

imageData = ctx.getImageData(417, 210, 1, 1);
data = imageData.data;
shouldNotBe('data[0]', '255');

imageData = ctx.getImageData(420, 220, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(430, 230, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');

imageData = ctx.getImageData(510, 310, 1, 1);
data = imageData.data;
shouldBe('data[0]', '0');

imageData = ctx.getImageData(529, 329, 1, 1);
data = imageData.data;
shouldBe('data[0]', '255');
