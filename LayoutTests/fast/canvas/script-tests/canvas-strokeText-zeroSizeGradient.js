description("Series of tests to ensure that strokeText() paints nothing on canvas when the strokeStyle is set to a zero-size gradient.");
var ctx = document.createElement('canvas').getContext('2d');

ctx.fillStyle = '#0f0';
ctx.fillRect(0, 0, 1, 1);

var g = ctx.createLinearGradient(0, 0, 0, 0); // zero-length line (undefined direction);
g.addColorStop(0, '#f00');
g.addColorStop(1, '#f00');
ctx.strokeStyle = g;
ctx.font = '1px sans-serif';
ctx.strokeText("AA", 0, 1);

var imageData = ctx.getImageData(0, 0, 1, 1);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "0");
shouldBe("imgdata[1]", "255");
shouldBe("imgdata[2]", "0");

