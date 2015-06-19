description("Test to verify the shadow of an image drawn on canvas");

// Create an auxiliary canvas to draw to and create an image from.
// This is done instead of simply loading an image from the file system
// because that would throw a SECURITY_ERR DOM Exception.
var aCanvas = document.createElement('canvas');
aCanvas.setAttribute('width', '200');
aCanvas.setAttribute('height', '200');
var aCtx = aCanvas.getContext('2d');

// Draw a rectangle on the same canvas.
aCtx.fillStyle = 'rgb(0, 255, 0)';
aCtx.rect(0, 0, 100, 50);
aCtx.fill();

// Create the image object to be drawn on the master canvas.
var img = new Image();
img.onload = draw;
img.src = aCanvas.toDataURL();

function draw()
{
    var canvas = document.getElementById('myCanvas');
    var ctx = canvas.getContext('2d');
    ctx.shadowOffsetX = 200;
    ctx.shadowOffsetY = 50;
    ctx.fillStyle = ctx.createPattern(img, 'repeat-x');
    ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
    ctx.fillRect(0, 0, 200, 200);

    var imageData = ctx.getImageData(10, 10, 1, 1);
    imgdata = imageData.data;
    shouldBe("imgdata[0]", "0");
    shouldBe("imgdata[1]", "255");
    shouldBe("imgdata[2]", "0");

    imageData = ctx.getImageData(290, 60, 1, 1);
    imgdata = imageData.data;
    shouldBe("imgdata[0]", "255");
    shouldBe("imgdata[1]", "0");
    shouldBe("imgdata[2]", "0");

    imageData = ctx.getImageData(90, 60, 1, 1);
    imgdata = imageData.data;
    shouldBe("imgdata[0]", "0");
    shouldBe("imgdata[1]", "0");
    shouldBe("imgdata[2]", "0");
}
