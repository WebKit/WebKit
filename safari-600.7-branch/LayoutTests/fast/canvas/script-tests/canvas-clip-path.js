description('Test Path2D as argument for clip.');

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

debug('Simple test of Path2D as argumeny for clip().');
ctx.fillStyle = 'rgb(0,255,0)';
ctx.save();
ctx.clip(path);
ctx.fillRect(0,0,300,150);
ctx.restore();
var imageData = ctx.getImageData(0, 0, 100, 100);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
imageData = ctx.getImageData(100, 100, 50, 50);
shouldBe('areaColor(imageData.data, {r:0,g:0,b:0,a:0})', 'true');
debug('');

ctx.clearRect(0,0,300,150);
debug('Transform context and apply clip(). Context path should be ignored.');
ctx.save();
ctx.scale(2,2);
ctx.translate(50,50);
ctx.beginPath();
ctx.rect(-100,-100,100,100); // Ignore this path.
ctx.clip(path);
ctx.fillRect(0,0,300,150);
ctx.restore();
imageData = ctx.getImageData(100, 100, 200, 50);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
imageData = ctx.getImageData(0, 0, 100, 100);
shouldBe('areaColor(imageData.data, {r:0,g:0,b:0,a:0})', 'true');
debug('');

ctx.clearRect(0,0,300,150);
debug('Path2D should not affect context path.');
ctx.save();
ctx.beginPath();
ctx.rect(100,100,50,50);
ctx.clip(path);
ctx.fillRect(0,0,300,150);
ctx.restore();
imageData = ctx.getImageData(0, 0, 100, 100);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
imageData = ctx.getImageData(100, 100, 50, 50);
shouldBe('areaColor(imageData.data, {r:0,g:0,b:0,a:0})', 'true');
ctx.clearRect(0,0,300,150);
ctx.save();
ctx.clip();
ctx.fillRect(0,0,300,150);
ctx.restore();
imageData = ctx.getImageData(0, 0, 100, 100);
shouldBe('areaColor(imageData.data, {r:0,g:0,b:0,a:0})', 'true');
imageData = ctx.getImageData(100, 100, 50, 50);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
debug('');

ctx.clearRect(0,0,300,150);
debug('Test WindingRule for Path2D.');
ctx.save();
ctx.clip(path, 'nonzero');
ctx.fillRect(0,0,300,150);
ctx.restore();
imageData = ctx.getImageData(0, 0, 100, 100);
shouldBe('areaColor(imageData.data, {r:0,g:255,b:0,a:255})', 'true');
ctx.clearRect(0,0,300,150);
ctx.save();
ctx.clip(path, 'evenodd');
ctx.fillRect(0,0,300,150);
ctx.restore();
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

shouldThrow("ctx.clip(0)");
shouldThrow("ctx.clip(null)");
shouldThrow("ctx.clip('path2d')");
shouldThrow("ctx.clip(undefined)");
shouldThrow("ctx.clip(Number.MAX_VALUE)");
shouldThrow("ctx.clip(function() {})");
shouldThrow("ctx.clip(false)");
shouldThrow("ctx.clip(new Date())");
shouldThrow("ctx.clip(0, 'nonzero')");
shouldThrow("ctx.clip(null, 'nonzero')");
shouldThrow("ctx.clip('path2d', 'nonzero')");
shouldThrow("ctx.clip(undefined, 'nonzero')");
shouldThrow("ctx.clip(Number.MAX_VALUE, 'nonzero')");
shouldThrow("ctx.clip(function() {}, 'nonzero')");
shouldThrow("ctx.clip(false, 'nonzero')");
shouldThrow("ctx.clip(new Date(), 'nonzero')");
