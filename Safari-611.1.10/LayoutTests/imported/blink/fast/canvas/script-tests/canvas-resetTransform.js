description("This test checks resetTransform in canvas v5");

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '100');
canvas.setAttribute('height', '100');
var ctx = canvas.getContext('2d');

debug("resetTransform should reset other transforms.");
ctx.save();
ctx.scale(0.5, 0.5);
ctx.resetTransform();
ctx.fillStyle = 'green';
ctx.fillRect(0, 0, 100, 100);
ctx.restore();

var imageData = ctx.getImageData(98, 98, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");

debug("resetTransform should not affect CTM outside of save() and restore().");
ctx.save();
ctx.scale(0.5, 0.5);
ctx.save();
ctx.resetTransform();
ctx.fillStyle = 'green';
ctx.fillRect(0, 0, 100, 100);
ctx.restore();
ctx.fillStyle = 'red';
ctx.fillRect(0, 0, 100, 100);
ctx.restore();

imageData = ctx.getImageData(98, 98, 1, 1);
imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");

imageData = ctx.getImageData(48, 48, 1, 1);
imgdata = imageData.data;
shouldBe("imgdata[0]", "255");
shouldBe("imgdata[1]", "0");
shouldBe("imgdata[2]", "0");

debug("resetTransform should restore the path transform to identity.");
/* This should draw a green rectangle on on top of a red one. The red should not be visible. */
ctx.save();
ctx.beginPath();
ctx.moveTo(0, 0);
ctx.lineTo(100, 0);
ctx.lineTo(100, 100);
ctx.lineTo(0, 100);
ctx.fillStyle = 'red';
ctx.fill();
ctx.translate(200, 0);
ctx.resetTransform();
ctx.fillStyle = 'green';
ctx.fill();
ctx.restore();

imageData = ctx.getImageData(50, 50, 1, 1);
imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");

debug("resetTransform should resolve the non-invertible CTM state.");
ctx.save();
ctx.fillStyle = 'red';
ctx.fillRect(0, 0, 100, 100);
ctx.beginPath();
ctx.moveTo(0, 0);
ctx.lineTo(100, 0);
ctx.lineTo(100, 100);
ctx.lineTo(0, 100);
ctx.scale(0, 0);
ctx.resetTransform();
ctx.fillStyle = 'green';
ctx.fill();
ctx.restore();

imageData = ctx.getImageData(98, 98, 1, 1);
imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");

debug("The path object should not be updated on the non-invertible CTM state.");
debug("resetTransform should restore the path object just before CTM became non-invertible.");
ctx.save();
ctx.fillStyle = 'red';
ctx.fillRect(0, 0, 100, 100);
ctx.beginPath();
ctx.moveTo(0, 0);
ctx.lineTo(100, 0);
ctx.lineTo(100, 50);
ctx.scale(0, 0);
ctx.lineTo(100, 100);
ctx.resetTransform();
ctx.lineTo(0, 100);
ctx.fillStyle = 'green';
ctx.fill();
ctx.restore();

imageData = ctx.getImageData(98, 98, 1, 1);
imgdata = imageData.data;
shouldBe("imgdata[0]", "255");
shouldBe("imgdata[1]", "0");
shouldBe("imgdata[2]", "0");

imageData = ctx.getImageData(98, 48, 1, 1);
imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");
