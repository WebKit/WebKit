description("Test negative values for lineDashOffset");

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '200');
canvas.setAttribute('height', '200');
var ctx = canvas.getContext('2d');

ctx.lineWidth = 200;
ctx.setLineDash([100,100]);
ctx.strokeStyle = "rgb(0,255,0)";
ctx.moveTo(0,100);
ctx.lineTo(200,100);
ctx.lineDashOffset = -100;
ctx.stroke();

// Verify value.
shouldBe('ctx.lineDashOffset', '-100');

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

pixelShouldBe(50,100,[0,0,0,0]);
pixelShouldBe(150,100,[0,255,0,255]);