description("Test canvas context after draw too small surface.");
if (self.testRunner)
  testRunner.overridePreference("WebKitCanvasUsesAcceleratedDrawing", 0);

var tmpimg = document.createElement('canvas');
tmpimg.width = 720;
tmpimg.height = 960;
ctx = tmpimg.getContext('2d');

var img = document.createElement('canvas');
img.width = 300;
img.height = 233;

ctx.drawImage(img, 337.208496, 735.022339, 225.473785, 0.005207);
ctx.fillStyle = '#808080';
ctx.fillRect(0, 0, 100, 100);
ctx.fillRect(200, 0, 100, 100);

debug("Test left box.");
imgdata = ctx.getImageData(0, 0, 1, 1).data;
shouldBe("imgdata[0]", "128");
debug("Test right box.");
imgdata = ctx.getImageData(200, 0, 1, 1).data;
shouldBe("imgdata[0]", "128");
