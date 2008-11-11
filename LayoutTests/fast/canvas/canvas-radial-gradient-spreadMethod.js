description("Series of tests to ensure correct behaviour of spreadMethod to a radial gradient.");
var ctx = document.createElement('canvas').getContext('2d');

var radgrad = ctx.createRadialGradient(80,90,90,80,50,100);
radgrad.addColorStop(0, 'green');
radgrad.addColorStop(1, 'green');

ctx.fillStyle = radgrad;
ctx.fillRect(0, 0, 100, 100);
var imageData = ctx.getImageData(0, 0, 100, 100);
var imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "128");
shouldBe("imgdata[6]", "0");
shouldBe("imgdata[7]", "255");

var successfullyParsed = true;
