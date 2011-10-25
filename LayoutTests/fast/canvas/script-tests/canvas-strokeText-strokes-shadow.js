description("Test that strokeText() doesn't produce a filled shadow.");
var ctx = document.createElement('canvas').getContext('2d');

ctx.fillStyle = 'green';
ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);

// Stroke an 'I' with its shadow in the upper left corner.
ctx.strokeStyle = 'white';
ctx.lineWidth = 2;
ctx.shadowColor = 'red';
ctx.shadowOffsetX = -15;
ctx.shadowOffsetY = 0;
ctx.font = '128px sans-serif';
ctx.strokeText("I", 0, 50);

imageData = ctx.getImageData(0, 0, 1, 1);
imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "128");
shouldBe("imgdata[2]", "0");
shouldBe("imgdata[3]", "255");
