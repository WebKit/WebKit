description('Test Path2D as argument for stroke.');

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
path.rect(0,0,100,100);

debug('Simple test of Path2D as argumeny for stroke().');
ctx.strokeStyle = 'rgb(0,255,0)';
ctx.lineWidth = 20;
ctx.stroke(path);
var imageData = ctx.getImageData(0, 0, 10, 10);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
imageData = ctx.getImageData(90, 90, 20, 20);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
debug('');

ctx.clearRect(0,0,300,150);
debug('Transform context and apply stroke(). Context path should be ignored.');
ctx.save()
ctx.scale(2,2);
ctx.translate(50,50);
ctx.beginPath();
ctx.rect(-100,-100,100,100); // Ignore this path.
ctx.stroke(path);
ctx.restore();
imageData = ctx.getImageData(80, 80, 40, 40);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
imageData = ctx.getImageData(280, 130, 20, 20);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
debug('');

ctx.clearRect(0,0,300,150);
debug('Path2D should not affect context path.');
ctx.beginPath();
ctx.rect(100,100,50,50);
ctx.stroke(path);
imageData = ctx.getImageData(0, 0, 10, 10);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
imageData = ctx.getImageData(90, 90, 20, 20);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
ctx.clearRect(0,0,300,150);
ctx.stroke();
imageData = ctx.getImageData(90, 90, 20, 20);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
imageData = ctx.getImageData(140, 140, 20, 10);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
debug('');

shouldThrow("ctx.stroke(0)");
shouldThrow("ctx.stroke(null)");
shouldThrow("ctx.stroke('path2d')");
shouldThrow("ctx.stroke(undefined)");
shouldThrow("ctx.stroke(Number.MAX_VALUE)");
shouldThrow("ctx.stroke(function() {})");
shouldThrow("ctx.stroke(false)");
shouldThrow("ctx.stroke(new Date())");
