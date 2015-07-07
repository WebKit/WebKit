description("Test that drawImage() does nothing with an incomplete image or video");

if (window.testRunner)
    testRunner.dumpAsText();

var canvas = document.createElement("canvas");
var ctx = canvas.getContext('2d');
ctx.fillStyle = 'red';
ctx.fillRect(0,0,150,150);

var img = new Image();
img.src = '../../http/tests/misc/resources/image-slow.pl';

var video = document.createElement("video");

shouldBe("ctx.drawImage(img, 0, 0)", "undefined");

var imgdata = ctx.getImageData(0, 0, 1, 1).data;
shouldBe("imgdata[0]", "255");
shouldBe("imgdata[1]", "0");
shouldBe("imgdata[2]", "0");
shouldBe("imgdata[3]", "255");

shouldBe("ctx.drawImage(video, 0, 0)", "undefined");

imgdata = ctx.getImageData(0, 0, 1, 1).data;
shouldBe("imgdata[0]", "255");
shouldBe("imgdata[1]", "0");
shouldBe("imgdata[2]", "0");
shouldBe("imgdata[3]", "255");
