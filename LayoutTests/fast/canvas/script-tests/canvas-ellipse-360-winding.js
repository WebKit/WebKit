description("This tests canvas full arc fill with nonzero winding rule. Eight green concentric thick circumferences should be displayed.");

var canvas = document.createElement('canvas');
document.body.appendChild(canvas)
canvas.setAttribute('width', '300');
canvas.setAttribute('height', '150');
var ctx = canvas.getContext('2d');

var r;
var anticlockwise = true;
ctx.beginPath();
for (r = 200; r >= 10; r -= 10) {
    ctx.moveTo(150 + r, 75);
    // Test that anticlockwise is both optional and defaults to false.
    if (anticlockwise) {
      ctx.ellipse(150, 75, r, r * 1.2, 0, 0, Math.PI * 2, anticlockwise);
    } else {
      ctx.ellipse(150, 75, r, r * 1.2, 0, 0, Math.PI * 2);
    }
    ctx.closePath();
    anticlockwise = !anticlockwise;
}
ctx.fillStyle = 'rgba(0, 255, 0, 1)';
ctx.strokeStyle = 'rgba(0, 255, 0, 1)';
ctx.fill();
ctx.stroke();

var imageData = ctx.getImageData(297, 75, 1, 1);
var data = imageData.data;
shouldBe("data[1]", "0");

imageData = ctx.getImageData(295, 144, 1, 1);
data = imageData.data;
shouldBe("data[1]", "255");

imageData = ctx.getImageData(272, 144, 1, 1);
data = imageData.data;
shouldBe("data[1]", "255");

imageData = ctx.getImageData(262, 144, 1, 1);
data = imageData.data;
shouldBe("data[1]", "0");

imageData = ctx.getImageData(239, 144, 1, 1);
data = imageData.data;
shouldBe("data[1]", "0");

imageData = ctx.getImageData(228, 144, 1, 1);
data = imageData.data;
shouldBe("data[1]", "255");
