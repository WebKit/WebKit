description("Series of tests to ensure correct results of isPointInStroke with Path2D argument.");

var ctx = document.createElement('canvas').getContext('2d');
document.body.appendChild(ctx.canvas);

ctx.strokeStyle = '#0ff';

var path = new Path2D();
path.rect(20,20,100,100);

shouldBeTrue("ctx.isPointInStroke(path,20,20)");
shouldBeTrue("ctx.isPointInStroke(path,120,20)");
shouldBeTrue("ctx.isPointInStroke(path,20,120)");
shouldBeTrue("ctx.isPointInStroke(path,120,120)");
shouldBeTrue("ctx.isPointInStroke(path,70,20)");
shouldBeTrue("ctx.isPointInStroke(path,20,70)");
shouldBeTrue("ctx.isPointInStroke(path,120,70)");
shouldBeTrue("ctx.isPointInStroke(path,70,120)");
shouldBeFalse("ctx.isPointInStroke(path,22,22)");
shouldBeFalse("ctx.isPointInStroke(path,118,22)");
shouldBeFalse("ctx.isPointInStroke(path,22,118)");
shouldBeFalse("ctx.isPointInStroke(path,118,118)");
shouldBeFalse("ctx.isPointInStroke(path,70,18)");
shouldBeFalse("ctx.isPointInStroke(path,122,70)");
shouldBeFalse("ctx.isPointInStroke(path,70,122)");
shouldBeFalse("ctx.isPointInStroke(path,18,70)");
shouldBeFalse("ctx.isPointInStroke(path,NaN,122)");
shouldBeFalse("ctx.isPointInStroke(path,18,NaN)");

shouldThrow("ctx.isPointInStroke(null,70,20)");
shouldThrow("ctx.isPointInStroke([],20,70)");
shouldThrow("ctx.isPointInStroke({},120,70)");
shouldThrow("ctx.isPointInPath('path2d', 50, 50)");
shouldThrow("ctx.isPointInPath(undefined, 50, 50)");
shouldThrow("ctx.isPointInPath(Number.MAX_VALUE, 50, 50)");
shouldThrow("ctx.isPointInPath(function() {}, 50, 50)");
shouldThrow("ctx.isPointInPath(false, 50, 50)");
shouldThrow("ctx.isPointInPath(new Date(), 50, 50)");

ctx.lineWidth = 10;
shouldBeTrue("ctx.isPointInStroke(path,22,22)");
shouldBeTrue("ctx.isPointInStroke(path,118,22)");
shouldBeTrue("ctx.isPointInStroke(path,22,118)");
shouldBeTrue("ctx.isPointInStroke(path,118,118)");
shouldBeTrue("ctx.isPointInStroke(path,70,18)");
shouldBeTrue("ctx.isPointInStroke(path,122,70)");
shouldBeTrue("ctx.isPointInStroke(path,70,122)");
shouldBeTrue("ctx.isPointInStroke(path,18,70)");
shouldBeFalse("ctx.isPointInStroke(path,26,70)");
shouldBeFalse("ctx.isPointInStroke(path,70,26)");
shouldBeFalse("ctx.isPointInStroke(path,70,114)");
shouldBeFalse("ctx.isPointInStroke(path,114,70)");

path = new Path2D();
path.moveTo(10,10);
path.lineTo(110,20);
path.lineTo(10,30);
ctx.lineJoin = "bevel";
shouldBeFalse("ctx.isPointInStroke(path,113,20)");

ctx.miterLimit = 40.0;
ctx.lineJoin = "miter";
shouldBeTrue("ctx.isPointInStroke(path,113,20)");

ctx.miterLimit = 2.0;
shouldBeFalse("ctx.isPointInStroke(path,113,20)");

path = new Path2D();
path.moveTo(10,10);
path.lineTo(110,10);
ctx.lineCap = "butt";
shouldBeFalse("ctx.isPointInStroke(path,112,10)");

ctx.lineCap = "round";
shouldBeTrue("ctx.isPointInStroke(path,112,10)");
shouldBeFalse("ctx.isPointInStroke(path,117,10)");

ctx.lineCap = "square";
shouldBeTrue("ctx.isPointInStroke(path,112,10)");
shouldBeFalse("ctx.isPointInStroke(path,117,10)");

ctx.lineCap = "butt";
ctx.setLineDash([10,10]);
shouldBeTrue("ctx.isPointInStroke(path,15,10)");
shouldBeFalse("ctx.isPointInStroke(path,25,10)");
shouldBeTrue("ctx.isPointInStroke(path,35,10)");

ctx.lineDashOffset = 10;
shouldBeFalse("ctx.isPointInStroke(path,15,10)");
shouldBeTrue("ctx.isPointInStroke(path,25,10)");
shouldBeFalse("ctx.isPointInStroke(path,35,10)");