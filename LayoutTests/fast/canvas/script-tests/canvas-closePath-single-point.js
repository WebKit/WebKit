description("Test the behavior of closePath on a path with a single point");
var ctx = document.createElement('canvas').getContext('2d');

document.body.appendChild(ctx.canvas);

ctx.strokeStyle = '#f00';
ctx.lineWidth = 20;

ctx.fillStyle = '#0f0';
ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);

ctx.beginPath();
ctx.moveTo(10, 10);
ctx.lineTo(100, 100);
ctx.closePath();
ctx.stroke();

var imageData = ctx.getImageData(0, 0, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "255");
shouldBe("imgdata[2]", "0");
shouldBe("imgdata[3]", "255");
