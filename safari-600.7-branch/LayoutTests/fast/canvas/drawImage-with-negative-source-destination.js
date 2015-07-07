description("Series of tests to ensure correct behaviour on negative source/destination of a HTMLCanvasElement");
var canvas2 = document.createElement('canvas');
canvas2.width = 100;
canvas2.height = 100;
var ctx2 = canvas2.getContext('2d');
ctx2.fillStyle = '#f00';
ctx2.fillRect(0, 0, 100, 50);
ctx2.fillStyle = '#0f0';
ctx2.fillRect(0, 50, 100, 50);

var canvas = document.getElementById('canvas').getContext('2d');
canvas.drawImage(canvas2, 100, 50, -50, 50, 0, 0, 50, 50);
canvas.drawImage(canvas2, 100, 100, -50, -50, 0, 100, 50, -50);
canvas.drawImage(canvas2, 0, 100, 100, -50, 100, 100, -50, -50);
canvas.drawImage(canvas2, 0, 50, 100, 50, 100, 0, -50, 50);

var imageData = canvas.getImageData(0, 0, 100, 100);
var imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "255");
shouldBe("imgdata[6]", "0");

// test width or height -1
canvas.fillStyle = '#000';
canvas.fillRect(0, 0, 1, 2);
canvas.drawImage(canvas2, 0, 0, 1, 1, 1, 1, -1, -1);
var imageData = canvas.getImageData(0, 0, 1, 2);
var imgdata = imageData.data;
shouldBe("imgdata[0]", "255");
shouldBe("imgdata[1]", "0");
shouldBe("imgdata[2]", "0");
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "0");
