description("Ensure that canvas shadowBlur is not affected by transformations.");

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

    if (Math.abs(evalA - b) < 10)
        print("PASS " + a + " is around " + b , "green")
    else
        print("FAIL " + a + " is not around " + b + " (actual: " + evalA + ")", "red");
}

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '600');
canvas.setAttribute('height', '600');
var ctx = canvas.getContext('2d');

ctx.shadowBlur = 25;
ctx.shadowOffsetX = 100;
ctx.shadowOffsetY = 100;
ctx.fillStyle = 'rgba(0, 0, 255, 1)';

// top left
ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
ctx.scale(4, 4);
ctx.rotate(Math.PI/2);
ctx.translate(25, -50);
ctx.fillRect(0, 0, 25, 25);

// bottom left
ctx.shadowColor = 'rgba(255, 0, 0, 0.5)';
ctx.setTransform(1, 0, 0, 1, 0, 0);
ctx.scale(0.5, 0.5);
ctx.fillRect(200, 600, 200, 200);

// top right
ctx.shadowColor = 'rgba(255, 0, 0, 1.0)';
ctx.scale(2, 2);
ctx.fillRect(300, 100, 100, 100);

// bottom right
ctx.shadowColor = 'rgba(255, 0, 0, 0.5)';
ctx.fillRect(300, 300, 100, 100);

var d;

// top left
d = ctx.getImageData(250, 250, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '255');

d = ctx.getImageData(250, 175, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

d = ctx.getImageData(250, 325, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

d = ctx.getImageData(175, 250, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

d = ctx.getImageData(325, 250, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

// bottom left
d = ctx.getImageData(250, 450, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '126');

d = ctx.getImageData(250, 375, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

d = ctx.getImageData(250, 525, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

d = ctx.getImageData(175, 450, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

d = ctx.getImageData(325, 450, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

d = ctx.getImageData(250, 250, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '255');

// top right
d = ctx.getImageData(450, 250, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '255');

d = ctx.getImageData(450, 175, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

d = ctx.getImageData(450, 325, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

d = ctx.getImageData(375, 250, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

d = ctx.getImageData(525, 250, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

// top right
d = ctx.getImageData(450, 450, 1, 1).data;
shouldBe('d[0]', '255');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBeAround('d[3]', '126');

d = ctx.getImageData(450, 375, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

d = ctx.getImageData(450, 525, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

d = ctx.getImageData(375, 450, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');

d = ctx.getImageData(525, 450, 1, 1).data;
shouldBe('d[0]', '0');
shouldBe('d[1]', '0');
shouldBe('d[2]', '0');
shouldBe('d[3]', '0');
