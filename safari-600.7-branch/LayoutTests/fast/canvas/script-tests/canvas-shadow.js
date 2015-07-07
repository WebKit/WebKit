description("Some tests for Canvas shadows");
var ctx = document.createElement('canvas').getContext('2d');

ctx.fillStyle = "green";
ctx.fillRect(0,0,100,100);

ctx.shadowColor = "green";
ctx.shadowOffsetX = 0;
ctx.shadowOffsetY = 0;
ctx.shadowBlur = 0;

var imageData = ctx.getImageData(0, 0, 200, 50);
var imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "128");
shouldBe("imgdata[6]", "0");

ctx.clearRect(0,0,200,50);

ctx.shadowColor = "green";
ctx.shadowOffsetX = 100;
ctx.shadowOffsetY = 0;
ctx.shadowBlur = 2;

ctx.fillStyle = "green";
ctx.fillRect(0,0,100,50);

imageData = ctx.getImageData(110, 10, 80, 30);
imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "128");
shouldBe("imgdata[6]", "0");
