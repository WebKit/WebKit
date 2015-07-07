description("Test different constructors of Path.");
var ctx = document.createElement('canvas').getContext('2d');

debug("Test constructor Path2D().")
var p1 = new Path2D();
p1.rect(0,0,100,100);
ctx.fillStyle = 'yellow';
ctx.fill(p1);
var imageData = ctx.getImageData(0, 0, 100, 100);
var imgdata = imageData.data;
shouldBe("imgdata[4]", "255");
shouldBe("imgdata[5]", "255");
shouldBe("imgdata[6]", "0");
shouldBe("imgdata[7]", "255");
debug("");

debug("Test constructor Path2D(DOMString) which takes a SVG data string.")
var p2 = new Path2D("M100,0L200,0L200,100L100,100z");
ctx.fillStyle = 'blue';
ctx.fill(p2);
imageData = ctx.getImageData(100, 0, 100, 100);
imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "255");
shouldBe("imgdata[7]", "255");
debug("");

debug("Test constructor Path2D(Path2D) which takes another Path2D object.")
var p3 = new Path2D(p1);
ctx.fillStyle = 'green';
ctx.fill(p3);
imageData = ctx.getImageData(0, 0, 100, 100);
imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "128");
shouldBe("imgdata[6]", "0");
shouldBe("imgdata[7]", "255");
debug("");