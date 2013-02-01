description("Test different constructors of Path.");
var ctx = document.createElement('canvas').getContext('2d');

debug("Test constructor Path().")
ctx.beginPath();
var p1 = new Path();
p1.rect(0,0,100,100);
ctx.fillStyle = 'yellow';
ctx.currentPath = p1;
ctx.fill();
var imageData = ctx.getImageData(0, 0, 100, 100);
var imgdata = imageData.data;
shouldBe("imgdata[4]", "255");
shouldBe("imgdata[5]", "255");
shouldBe("imgdata[6]", "0");
shouldBe("imgdata[7]", "255");
debug("");

debug("Test constructor Path(DOMString) which takes a SVG data string.")
ctx.beginPath();
var p2 = new Path("M100,0L200,0L200,100L100,100z");
ctx.currentPath = p2;
ctx.fillStyle = 'blue';
ctx.fill();
imageData = ctx.getImageData(100, 0, 100, 100);
imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "0");
shouldBe("imgdata[6]", "255");
shouldBe("imgdata[7]", "255");
debug("");

debug("Test constructor Path(Path) which takes another Path object.")
ctx.beginPath();
var p3 = new Path(p1);
ctx.translate(200,0);
ctx.currentPath = p3;
ctx.fillStyle = 'green';
ctx.translate(-200,0);
ctx.fill();
imageData = ctx.getImageData(200, 0, 100, 100);
imgdata = imageData.data;
shouldBe("imgdata[4]", "0");
shouldBe("imgdata[5]", "128");
shouldBe("imgdata[6]", "0");
shouldBe("imgdata[7]", "255");
debug("");