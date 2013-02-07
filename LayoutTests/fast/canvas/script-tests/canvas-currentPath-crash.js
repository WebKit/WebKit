description("Series of tests to ensure that currentPath does not take non-Path values.");
var ctx = document.createElement('canvas').getContext('2d');
ctx.fillStyle = "green";
ctx.beginPath();
ctx.rect(0,0,100,100);

ctx.currentPath = null;
ctx.currentPath = 0;
ctx.currentPath = {};
ctx.currentPath = new Array();
ctx.currentPath = undefined;

ctx.fill();
var imageData = ctx.getImageData(50, 50, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");