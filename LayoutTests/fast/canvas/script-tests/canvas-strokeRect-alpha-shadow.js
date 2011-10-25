description("Ensure correct behavior of canvas with strokeRect using a semi-transparent solid strokeStyle and a shadow");

function print(message, color)
{
    var paragraph = document.createElement("div");
    paragraph.appendChild(document.createTextNode(message));
    paragraph.style.fontFamily = "monospace";
    if (color)
        paragraph.style.color = color;
    document.getElementById("console").appendChild(paragraph);
}

function shouldBeAround(a, b)
{
    var evalA;
    try {
        evalA = eval(a);
    } catch(e) {
        evalA = e;
    }

    if (Math.abs(evalA - b) < 15)
        print("PASS " + a + " is around " + b , "green")
    else
        print("FAIL " + a + " is not around " + b + " (actual: " + evalA + ")", "red");
}

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '600');
canvas.setAttribute('height', '1100');
var ctx = canvas.getContext('2d');

ctx.save();
ctx.strokeStyle = 'rgba(0, 0, 255, 0.5)';
ctx.shadowColor = 'rgba(255, 0, 0, 0.5)';
ctx.shadowOffsetX = 250;
ctx.lineWidth = 25;

var side = 200;

// Alpha shadow.
ctx.shadowBlur = 0;
ctx.strokeRect(50, 50, side, side);

// Blurry shadow.
ctx.shadowBlur = 10;
ctx.strokeRect(50, 300, side, side);

ctx.rotate(Math.PI / 2);

// Rotated alpha shadow.
ctx.shadowBlur = 0;
ctx.strokeRect(550, -250, side, side);

// Rotated blurry shadow.
ctx.shadowBlur = 10;
ctx.strokeRect(800, -250, side, side);

ctx.restore();

var imageData, data;
ctx.fillStyle = 'black';

function test(alphaTestFunction, x, y, r, g, b, a) {
    // Get pixel.
    imageData = ctx.getImageData(x, y, 1, 1);
    data = imageData.data;
    // Test pixel color components.
    shouldBe('data[0]', r+'');
    shouldBe('data[1]', g+'');
    shouldBe('data[2]', b+'');
    alphaTestFunction('data[3]', a+'');
    // Plot test point.
    ctx.fillRect(x, y, 3, 3);
}

print('Verifying alpha shadow...');
test(shouldBe, 400, 150, 0, 0, 0, 0);

test(shouldBe, 400, 75,  0, 0, 0, 0);
test(shouldBe, 400, 225, 0, 0, 0, 0);
test(shouldBe, 325, 150, 0, 0, 0, 0);
test(shouldBe, 475, 150, 0, 0, 0, 0);

test(shouldBeAround, 400,  50, 255, 0, 0, 64);
test(shouldBeAround, 500, 150, 255, 0, 0, 64);
test(shouldBeAround, 400, 250, 255, 0, 0, 64);
test(shouldBeAround, 300, 150, 255, 0, 0, 64);

test(shouldBe, 400, 25,  0, 0, 0, 0);
test(shouldBe, 525, 150, 0, 0, 0, 0);
test(shouldBe, 400, 275, 0, 0, 0, 0);
test(shouldBe, 275, 150, 0, 0, 0, 0);

print(' ');
print('Verifying blurry shadow...');
test(shouldBe, 400, 400, 0, 0, 0, 0);

test(shouldBe, 400, 325, 0, 0, 0, 0);
test(shouldBe, 475, 400, 0, 0, 0, 0);
test(shouldBe, 400, 475, 0, 0, 0, 0);
test(shouldBe, 325, 400, 0, 0, 0, 0);

test(shouldBeAround, 400, 300, 255, 0, 0, 64);
test(shouldBeAround, 400, 500, 255, 0, 0, 64);
test(shouldBeAround, 300, 400, 255, 0, 0, 64);
test(shouldBeAround, 500, 400, 255, 0, 0, 64);

test(shouldBe, 525, 400, 0, 0, 0, 0);
test(shouldBe, 275, 400, 0, 0, 0, 0);

print(' ');
print('Verifying rotated alpha shadow...');
test(shouldBe, 400, 650, 0, 0, 0, 0);

test(shouldBe, 400, 575, 0, 0, 0, 0);
test(shouldBe, 400, 725, 0, 0, 0, 0);
test(shouldBe, 325, 650, 0, 0, 0, 0);
test(shouldBe, 475, 650, 0, 0, 0, 0);

test(shouldBeAround, 400, 550, 255, 0, 0, 64);
test(shouldBeAround, 500, 650, 255, 0, 0, 64);
test(shouldBeAround, 400, 750, 255, 0, 0, 64);
test(shouldBeAround, 300, 650, 255, 0, 0, 64);

test(shouldBe, 400, 525, 0, 0, 0, 0);
test(shouldBe, 525, 650, 0, 0, 0, 0);
test(shouldBe, 400, 775, 0, 0, 0, 0);
test(shouldBe, 275, 650, 0, 0, 0, 0);

print(' ');
print('Verifying rotated blurry shadow...');
test(shouldBe, 400, 900, 0, 0, 0, 0);

test(shouldBe, 400, 825, 0, 0, 0, 0);
test(shouldBe, 475, 900, 0, 0, 0, 0);
test(shouldBe, 400, 975, 0, 0, 0, 0);
test(shouldBe, 325, 900, 0, 0, 0, 0);

test(shouldBeAround, 400, 800,  255, 0, 0, 64);
test(shouldBeAround, 400, 1000, 255, 0, 0, 64);
test(shouldBeAround, 300, 900,  255, 0, 0, 64);
test(shouldBeAround, 500, 900,  255, 0, 0, 64);

test(shouldBe, 525, 900,  0, 0, 0, 0);
test(shouldBe, 275, 900,  0, 0, 0, 0);
test(shouldBe, 400, 1025, 0, 0, 0, 0);

print(' ');
