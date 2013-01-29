description("Test the behavior of isPointInStroke in Canvas");
var ctx = document.createElement('canvas').getContext('2d');

document.body.appendChild(ctx.canvas);

ctx.strokeStyle = '#0ff';

// Create new path.
ctx.beginPath();
ctx.rect(20,20,100,100);

debug("Initial behavior: lineWith = 1.0")
shouldBeTrue("ctx.isPointInStroke(20,20)");
shouldBeTrue("ctx.isPointInStroke(120,20)");
shouldBeTrue("ctx.isPointInStroke(20,120)");
shouldBeTrue("ctx.isPointInStroke(120,120)");
shouldBeTrue("ctx.isPointInStroke(70,20)");
shouldBeTrue("ctx.isPointInStroke(20,70)");
shouldBeTrue("ctx.isPointInStroke(120,70)");
shouldBeTrue("ctx.isPointInStroke(70,120)");
shouldBeFalse("ctx.isPointInStroke(22,22)");
shouldBeFalse("ctx.isPointInStroke(118,22)");
shouldBeFalse("ctx.isPointInStroke(22,118)");
shouldBeFalse("ctx.isPointInStroke(118,118)");
shouldBeFalse("ctx.isPointInStroke(70,18)");
shouldBeFalse("ctx.isPointInStroke(122,70)");
shouldBeFalse("ctx.isPointInStroke(70,122)");
shouldBeFalse("ctx.isPointInStroke(18,70)");
debug("");

debug("Set lineWith = 10.0");
ctx.lineWidth = 10;
shouldBeTrue("ctx.isPointInStroke(22,22)");
shouldBeTrue("ctx.isPointInStroke(118,22)");
shouldBeTrue("ctx.isPointInStroke(22,118)");
shouldBeTrue("ctx.isPointInStroke(118,118)");
shouldBeTrue("ctx.isPointInStroke(70,18)");
shouldBeTrue("ctx.isPointInStroke(122,70)");
shouldBeTrue("ctx.isPointInStroke(70,122)");
shouldBeTrue("ctx.isPointInStroke(18,70)");
shouldBeFalse("ctx.isPointInStroke(26,70)");
shouldBeFalse("ctx.isPointInStroke(70,26)");
shouldBeFalse("ctx.isPointInStroke(70,114)");
shouldBeFalse("ctx.isPointInStroke(114,70)");
debug("");

debug("Check lineJoin = 'bevel'");
ctx.beginPath();
ctx.moveTo(10,10);
ctx.lineTo(110,20);
ctx.lineTo(10,30);
ctx.lineJoin = "bevel";
shouldBeFalse("ctx.isPointInStroke(113,20)");
debug("");

debug("Check lineJoin = 'miter'");
ctx.miterLimit = 40.0;
ctx.lineJoin = "miter";
shouldBeTrue("ctx.isPointInStroke(113,20)");
debug("");

debug("Check miterLimit = 2.0");
ctx.miterLimit = 2.0;
shouldBeFalse("ctx.isPointInStroke(113,20)");
debug("");

debug("Check lineCap = 'butt'");
ctx.beginPath();
ctx.moveTo(10,10);
ctx.lineTo(110,10);
ctx.lineCap = "butt";
shouldBeFalse("ctx.isPointInStroke(112,10)");
debug("");

debug("Check lineCap = 'round'");
ctx.lineCap = "round";
shouldBeTrue("ctx.isPointInStroke(112,10)");
shouldBeFalse("ctx.isPointInStroke(117,10)");
debug("");

debug("Check lineCap = 'square'");
ctx.lineCap = "square";
shouldBeTrue("ctx.isPointInStroke(112,10)");
shouldBeFalse("ctx.isPointInStroke(117,10)");
debug("");

debug("Check setLineDash([10,10])");
ctx.lineCap = "butt";
ctx.setLineDash([10,10]);
shouldBeTrue("ctx.isPointInStroke(15,10)");
shouldBeFalse("ctx.isPointInStroke(25,10)");
shouldBeTrue("ctx.isPointInStroke(35,10)");
debug("");

debug("Check dashOffset = 10");
ctx.lineDashOffset = 10;
shouldBeFalse("ctx.isPointInStroke(15,10)");
shouldBeTrue("ctx.isPointInStroke(25,10)");
shouldBeFalse("ctx.isPointInStroke(35,10)");