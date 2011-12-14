description("Test if putImageData gives back the same result as getImageData");
var ctx = document.createElement('canvas').getContext('2d');

ctx.fillStyle = "red";
ctx.fillRect(0,0,50,20);
ctx.fillStyle = "green";
ctx.fillRect(50,0,50,20);
ctx.fillStyle = "blue";
ctx.fillRect(100,0,50,20);

var data = ctx.getImageData(0,0,150,20);
ctx.putImageData(data, 0, 20);

var imageData = ctx.getImageData(1, 21, 48, 18);
var imgdata = imageData.data;
shouldBe("imgdata[4]", "255");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "0");

imageData = ctx.getImageData(51, 21, 48, 18);
imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "128");
shouldBe("imgdata[6]", "0");

imageData = ctx.getImageData(101, 21, 48, 18);
imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "255");

var successfullyParsed = true;
