description("Test that the rendering context's lineWidth is intact after calling strokeRect()");
var ctx = document.createElement('canvas').getContext('2d');

ctx.fillStyle = 'red';
ctx.fillRect(0, 0, 1, 1);

var imageData = ctx.getImageData(0, 0, 2, 1);
var imgdata = imageData.data;
shouldBe("ctx.fillStyle", "'#ff0000'");
shouldBe("imgdata[0]", "255");
shouldBe("imgdata[1]", "0");
shouldBe("imgdata[2]", "0");
shouldBe("imgdata[3]", "255");
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "0");
shouldBe("imgdata[7]", "0");

ctx.strokeStyle = 'red';
ctx.lineWidth = 100;
// NOTE: This version of strokeRect() is WebKit-specific and not part of the standard API.
ctx.strokeRect(0, 0, 10, 10, 1);
shouldBe("ctx.lineWidth", "100");

ctx.strokeStyle = 'green';
ctx.beginPath();
ctx.moveTo(0, 0);
ctx.lineTo(20, 20);
ctx.stroke();

imageData = ctx.getImageData(2, 2, 1, 1);
imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");
shouldBe("imgdata[3]", "255");

document.body.appendChild(ctx.canvas)
