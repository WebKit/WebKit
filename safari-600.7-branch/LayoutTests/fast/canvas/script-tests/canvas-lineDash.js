description("Basic test for setLineDash, getLineDash and lineDashOffset");

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '700');
canvas.setAttribute('height', '700');
var ctx = canvas.getContext('2d');

function dataToArray(data) {
    var result = new Array(data.length)
    for (var i = 0; i < data.length; i++)
        result[i] = data[i];
    return result;
}

function getPixel(x, y) {
    var data = ctx.getImageData(x,y,1,1);
    if (!data) // getImageData failed, which should never happen
        return [-1,-1,-1,-1];
    return dataToArray(data.data);
}

function pixelShouldBe(x, y, colour) {
    shouldBe("getPixel(" + [x, y] +")", "["+colour+"]");
}

// Verify default values.
shouldBe('ctx.lineDashOffset', '0');

// Set dash-style.
ctx.setLineDash([15, 10]);
ctx.lineDashOffset = 5;
ctx.strokeRect (10,10,100,100);

// Verify dash and offset.
var lineDash;
lineDash = ctx.getLineDash();
shouldBe('lineDash[0]', '15');
shouldBe('lineDash[1]', '10');
shouldBe('ctx.lineDashOffset', '5');

// Set dash style to even number
ctx.setLineDash([5, 10, 15]);
ctx.strokeRect(20, 20, 120, 120);

// Verify dash pattern is normalized
lineDash = ctx.getLineDash();
shouldBe('lineDash[0]', '5');
shouldBe('lineDash[1]', '10');
shouldBe('lineDash[2]', '15');
shouldBe('lineDash[3]', '5');
shouldBe('lineDash[4]', '10');
shouldBe('lineDash[5]', '15');

// Verify that conversion from string works
ctx.setLineDash(["1", 2]);
lineDash = ctx.getLineDash();
shouldBe('lineDash[0]', '1');
shouldBe('lineDash[1]', '2');

// Verify that line dash offset persists after
// clearRect (which causes a save/restore of the context
// state to the stack).
ctx.clearRect(0, 0, 700, 700);
shouldBe('ctx.lineDashOffset', '5');

// Verify dash rendering
ctx.setLineDash([20, 10]);
ctx.lineDashOffset = 0;
ctx.lineWidth = 4; // To make the test immune to plaform anti-aliasing discrepancies
ctx.strokeStyle = '#00FF00';
ctx.strokeRect(10.5, 10.5, 30, 30);

pixelShouldBe(25, 10, [0, 255, 0, 255]);
pixelShouldBe(35, 10, [0, 0, 0, 0]);
pixelShouldBe(40, 25, [0, 255, 0, 255]);
pixelShouldBe(40, 35, [0, 0, 0, 0]);
pixelShouldBe(25, 40, [0, 255, 0, 255]);
pixelShouldBe(15, 40, [0, 0, 0, 0]);
pixelShouldBe(10, 25, [0, 255, 0, 255]);
pixelShouldBe(10, 15, [0, 0, 0, 0]);

// Verify that lineDashOffset works as expected
ctx.lineDashOffset = 20;
ctx.strokeRect(50.5, 10.5, 30, 30);
pixelShouldBe(55, 10, [0, 0, 0, 0]);
pixelShouldBe(65, 10, [0, 255, 0, 255]);
pixelShouldBe(80, 15, [0, 0, 0, 0]);
pixelShouldBe(80, 25, [0, 255, 0, 255]);
pixelShouldBe(75, 40, [0, 0, 0, 0]);
pixelShouldBe(65, 40, [0, 255, 0, 255]);
pixelShouldBe(50, 35, [0, 0, 0, 0]);
pixelShouldBe(50, 25, [0, 255, 0, 255]);

// Verify negative lineDashOffset
ctx.lineDashOffset = -10;
ctx.strokeRect(90.5, 10.5, 30, 30);
pixelShouldBe(95, 10, [0, 0, 0, 0]);
pixelShouldBe(105, 10, [0, 255, 0, 255]);
pixelShouldBe(120, 15, [0, 0, 0, 0]);
pixelShouldBe(120, 25, [0, 255, 0, 255]);
pixelShouldBe(115, 40, [0, 0, 0, 0]);
pixelShouldBe(105, 40, [0, 255, 0, 255]);
pixelShouldBe(90, 35, [0, 0, 0, 0]);
pixelShouldBe(90, 25, [0, 255, 0, 255]);

