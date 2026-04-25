description("Series of tests to ensure correct behaviour of getImageData and putImageData when alpha is involved");
var ctx = document.getElementById('canvas').getContext('2d');
ctx.fillStyle = 'rgba(255, 0, 0, 0.01)';
ctx.fillRect(0, 0, 1, 200);
ctx.fillStyle = 'rgba(0, 255, 0, 0.995)';
ctx.fillRect(1, 0, 199, 200);
var imageData = ctx.getImageData(0, 0, 200, 200);
var imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "255");
shouldBe("imgdata[6]", "0");
shouldBe("imgdata[7]", "254");

ctx.putImageData(imageData, 0, 0);

imgdata = ctx.getImageData(0, 0, 200, 200).data;

shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "255");
shouldBe("imgdata[6]", "0");
shouldBe("imgdata[7]", "254");
