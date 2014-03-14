description('Test Path2D as argument for fill.');

function areaColor(data, refColor) {
    if (!data.length)
        return true;
    var c = {r:0,g:0,b:0,a:0};
    for (var i = 0; i < data.length; i += 4) {
        c.r += data[i];
        c.g += data[i+1];
        c.b += data[i+2];
        c.a += data[i+3];
    }
    if (refColor.r == Math.round(c.r/data.length*4)
        && refColor.g == Math.round(c.g/data.length*4)
        && refColor.b == Math.round(c.b/data.length*4)
        && refColor.a == Math.round(c.a/data.length*4))
        return true;
    return false;
}

var ctx = document.createElement('canvas').getContext('2d');
document.body.appendChild(ctx.canvas);

var path = new Path2D();
path.moveTo(0,0);
path.lineTo(100,0);
path.lineTo(100,100);
path.lineTo(0,100);
path.lineTo(0,20);
path.lineTo(80,20);
path.lineTo(80,80);
path.lineTo(20,80);
path.lineTo(20,20);
path.lineTo(0,20);
path.closePath();

debug('Simple test of Path2D as argumeny for fill().');
ctx.fillStyle = 'rgb(0,255,0)';
ctx.fill(path);
var imageData = ctx.getImageData(0, 0, 100, 100);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
debug('');

ctx.clearRect(0,0,300,150);
debug('Transform context and apply fill(). Context path should be ignored.');
ctx.save();
ctx.scale(2,2);
ctx.translate(50,50);
ctx.beginPath();
ctx.rect(-100,-100,100,100); // Ignore this path.
ctx.fill(path);
ctx.restore();
imageData = ctx.getImageData(100, 100, 200, 50);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
imageData = ctx.getImageData(0, 0, 100, 100);
shouldBe('areaColor(imageData.data, {r:0,g:0,b:0,a:0})', 'true');
debug('');

ctx.clearRect(0,0,300,150);
debug('Path2D should not affect context path.');
ctx.beginPath();
ctx.rect(100,100,50,50);
ctx.fill(path);
imageData = ctx.getImageData(0, 0, 100, 100);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
imageData = ctx.getImageData(100, 100, 50, 50);
shouldBe('areaColor(imageData.data, {r:0,g:0,b:0,a:0})', 'true');
ctx.clearRect(0,0,300,150);
ctx.fill();
imageData = ctx.getImageData(0, 0, 100, 100);
shouldBe('areaColor(imageData.data, {r:0,g:0,b:0,a:0})', 'true');
imageData = ctx.getImageData(100, 100, 50, 50);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
debug('');

ctx.clearRect(0,0,300,150);
debug('Test WindingRule for Path2D.');
ctx.fill(path, 'nonzero');
imageData = ctx.getImageData(0, 0, 100, 100);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
ctx.clearRect(0,0,300,150);
ctx.fill(path, 'evenodd');
imageData = ctx.getImageData(0, 0, 100, 20);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
imageData = ctx.getImageData(0, 80, 100, 20);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
imageData = ctx.getImageData(0, 20, 20, 60);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
imageData = ctx.getImageData(80, 20, 20, 60);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
imageData = ctx.getImageData(20, 20, 60, 60);
shouldBe('areaColor(imageData.data, {r:0,g:0,b:0,a:0})', 'true');