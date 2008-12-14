description("Series of tests to ensure correct behaviour of composite on a fully transparent color.");
var ctx = document.createElement('canvas').getContext('2d');

ctx.globalCompositeOperation = "source-over";
ctx.fillStyle = 'rgba(0,0,0,0)';
ctx.fillRect(0,0,100,100);

ctx.save();
ctx.translate(0,100);
ctx.scale(1,-1);
ctx.fillStyle = 'green';
ctx.fillRect(0,0,100,100);
ctx.restore();

var imageData = ctx.getImageData(1, 1, 98, 98);
var imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "128");
shouldBe("imgdata[6]", "0");

var successfullyParsed = true;
