description("Series of tests to ensure correct behaviour on transform of a pattern");
var canvas2 = document.createElement('canvas');
canvas2.width = 100;
canvas2.height = 100;
var ctx2 = canvas2.getContext('2d');
ctx2.fillStyle = '#0f0';
ctx2.fillRect(0, 0, 50, 50);
ctx2.fillRect(50, 50, 50, 50);
ctx2.fillStyle = '#f00';
ctx2.fillRect(50, 0, 50, 50);
ctx2.fillRect(0, 50, 50, 50);

var ctx = document.getElementById('canvas').getContext('2d');

ctx.save();
ctx.transform(2, 0, 0, 2, 0, 0);
var pattern = ctx.createPattern(canvas2, 'repeat');
ctx.fillStyle = pattern;
ctx.fillRect(0,0,100,100);
ctx.restore();

ctx.save();
ctx.transform(0.5, 0, 0, 0.5, 0, 0);
pattern = ctx.createPattern(canvas2, 'repeat');
ctx.fillStyle = pattern;
ctx.fillRect(0,0,100,100);
ctx.restore();

var imageData = ctx.getImageData(25, 25, 75, 75);
var imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "255");
shouldBe("imgdata[6]", "0");
imageData = ctx.getImageData(25, 0, 25, 25);
imgdata = imageData.data;
shouldBe("imgdata[4]", "255");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "0");
imageData = ctx.getImageData(0, 0, 25, 25);
imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "255");
shouldBe("imgdata[6]", "0");

var successfullyParsed = true;
