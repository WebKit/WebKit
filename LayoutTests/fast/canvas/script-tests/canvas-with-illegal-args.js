description("Series of tests to ensure correct behaviour of calling canvas methods with illegal arguments (Infintiy and NaN)");

var ctx;

debug("Test scale.");
ctx = document.createElement('canvas').getContext('2d');

ctx.scale(NaN, 1);
ctx.fillStyle = 'green';
ctx.fillRect(0, 0, 100, 100);

var imageData = ctx.getImageData(50, 50, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");

ctx = document.createElement('canvas').getContext('2d');

ctx.scale(1, NaN);
ctx.fillStyle = 'green';
ctx.fillRect(0, 0, 100, 100);

var imageData = ctx.getImageData(50, 50, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");

ctx = document.createElement('canvas').getContext('2d');

ctx.scale(Infinity, 1);
ctx.fillStyle = 'green';
ctx.fillRect(0, 0, 100, 100);

var imageData = ctx.getImageData(50, 50, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");

ctx = document.createElement('canvas').getContext('2d');

ctx.scale(1, Infinity);
ctx.fillStyle = 'green';
ctx.fillRect(0, 0, 100, 100);

var imageData = ctx.getImageData(50, 50, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");



debug("Test translate.");
ctx = document.createElement('canvas').getContext('2d');

ctx.translate(NaN, 1);
ctx.fillStyle = 'green';
ctx.fillRect(0, 0, 100, 100);

var imageData = ctx.getImageData(50, 50, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");

ctx = document.createElement('canvas').getContext('2d');

ctx.translate(1, NaN);
ctx.fillStyle = 'green';
ctx.fillRect(0, 0, 100, 100);

var imageData = ctx.getImageData(50, 50, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");

ctx = document.createElement('canvas').getContext('2d');

ctx.translate(Infinity, 1);
ctx.fillStyle = 'green';
ctx.fillRect(0, 0, 100, 100);

var imageData = ctx.getImageData(50, 50, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");

ctx = document.createElement('canvas').getContext('2d');

ctx.translate(1, Infinity);
ctx.fillStyle = 'green';
ctx.fillRect(0, 0, 100, 100);

var imageData = ctx.getImageData(50, 50, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");


debug("Test rotate.");
ctx = document.createElement('canvas').getContext('2d');

ctx.rotate(NaN);
ctx.fillStyle = 'green';
ctx.fillRect(0, 0, 100, 100);

var imageData = ctx.getImageData(50, 50, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");

ctx = document.createElement('canvas').getContext('2d');

ctx.rotate(Infinity);
ctx.fillStyle = 'green';
ctx.fillRect(0, 0, 100, 100);

var imageData = ctx.getImageData(50, 50, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");
