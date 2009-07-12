description("This tests behaviour of path modification APIs on an empty path.");
var canvas = document.createElement('canvas');
canvas.width=300;
canvas.height=300;
var ctx = canvas.getContext('2d');

function getColor(x,y) {
    var imageData = ctx.getImageData(x, y, 1, 1);
    var data = imageData.data;
    return [data[0], data[1], data[2], data[3]];
}

// Test no drawing cases
ctx.fillStyle = 'green';
ctx.strokeStyle = 'red';
ctx.lineWidth = 50;

debug("Test lineTo")
ctx.beginPath();
ctx.fillRect(0, 0, 100, 100);
ctx.lineTo(50, 50);
ctx.stroke();
shouldBe("getColor(50,50)", "[0,128,0,255]");


// Test path modifications that result in drawing
ctx.fillStyle = 'red';
ctx.strokeStyle = 'green';

debug("Test lineTo sequence")
ctx.beginPath();
ctx.fillRect(0, 0, 100, 100);
ctx.lineTo(0, 50);
ctx.lineTo(100, 50);
ctx.stroke();
shouldBe("getColor(50,50)", "[0,128,0,255]");

debug("Test quadraticCurveTo")
ctx.beginPath();
ctx.fillRect(0, 0, 100, 100);
ctx.quadraticCurveTo(0, 50, 100, 50);
ctx.stroke();
shouldBe("getColor(50,50)", "[0,128,0,255]");

debug("Test bezierCurveTo")
ctx.beginPath();
ctx.fillRect(0, 0, 100, 100);
ctx.bezierCurveTo(0, 50, 50, 50, 100, 50);
ctx.stroke();
shouldBe("getColor(50,50)", "[0,128,0,255]");

var successfullyParsed = true;
