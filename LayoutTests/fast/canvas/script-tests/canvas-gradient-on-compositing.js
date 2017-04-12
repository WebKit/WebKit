description("Test that compositing does not influence the gradient space to context space mapping.");
var ctx = document.createElement('canvas').getContext('2d');
const tolerance = 3;

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

function pixelShouldBeNear(x, y, colour) {
    var c = getPixel(x, y);
    var diff = Math.max(Math.abs(c[0]-colour[0]), Math.abs(c[1]-colour[1]), Math.abs(c[2]-colour[2]), Math.abs(c[3]-colour[3]));
    if (diff < tolerance)
        testPassed("getPixel(" + [x, y] + ") is close to [" + colour + "].");
    else
        testFailed("getPixel(" + [x, y] + ") should be close to [" + colour + "]. Was [" + c + "].");
}

var grad = ctx.createLinearGradient(100,0,200,0);
grad.addColorStop(0, 'rgba(0,0,0,0)');
grad.addColorStop(0.5, 'rgba(0,0,0,0)');
grad.addColorStop(0.5, 'rgba(0,128,0,1)');
grad.addColorStop(1, 'rgba(0,128,0,1)');

ctx.fillStyle = "rgba(0,0,0,0)";
ctx.beginPath();
ctx.moveTo(100,0);
ctx.rect(100,0,100,100);
ctx.closePath();
ctx.fill();

ctx.globalCompositeOperation = 'destination-atop';

ctx.fillStyle = grad;
ctx.beginPath();
ctx.moveTo(100,0);
ctx.rect(100,0,100,100);
ctx.closePath();
ctx.fill();

pixelShouldBeNear(125,50,[0,0,0,0]);
pixelShouldBeNear(175,50,[0,128,0,255]);
