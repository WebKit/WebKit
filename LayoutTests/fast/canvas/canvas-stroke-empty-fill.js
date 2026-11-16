description("Series of tests to ensure correct behaviour for stroke() on an empty fillStyle() (alpha=0)");
var ctx = document.createElement('canvas').getContext('2d');

ctx.fillStyle = 'rgba(0,0,0,0)';
ctx.strokeStyle = 'green';
ctx.lineWidth = 200;
ctx.moveTo(0,100);
ctx.lineTo(200,100);
ctx.stroke();
var imageData = ctx.getImageData(0, 0, 100, 100);
var imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "128");
shouldBe("imgdata[6]", "0");
